#include "../../include/hardware/GpuController.hpp"
#include "../../include/core/Logger.hpp"
#include <iostream>
#include <tlhelp32.h>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <mutex>

// NVML function pointer typedefs
typedef int (*nvmlInit_t)();
typedef int (*nvmlShutdown_t)();
typedef int (*nvmlDeviceGetHandleByIndex_t)(unsigned int, void**);
typedef int (*nvmlDeviceGetName_t)(void*, char*, unsigned int);
typedef int (*nvmlDeviceGetPowerUsage_t)(void*, unsigned int*);
typedef int (*nvmlDeviceGetTemperature_t)(void*, int, unsigned int*);

struct nvmlProcessInfo_raw {
    unsigned int pid;
    unsigned long long usedGpuMemory;
};

typedef int (*nvmlDeviceGetGraphicsRunningProcesses_t)(void*, unsigned int*, nvmlProcessInfo_raw*);
typedef int (*nvmlDeviceGetComputeRunningProcesses_t)(void*, unsigned int*, nvmlProcessInfo_raw*);

// ============================================================
// Adaptive NVML session:
//   - Loaded on first Query() or when activity is detected
//   - Automatically unloaded after 2 consecutive idle queries (0 processes)
//     allowing the NVIDIA driver to enter D3Cold sleep (0 Watts)
//   - Re-checked periodically (every ~40s) or on demand
// ============================================================
static std::mutex  s_nvmlMutex;
static HMODULE     s_hNvml      = nullptr;
static bool        s_nvmlInited = false;
static bool        s_nvmlHasGpu = false;
static void*       s_nvmlDev    = nullptr;

static nvmlInit_t                              s_nvmlInit      = nullptr;
static nvmlShutdown_t                          s_nvmlShutdown  = nullptr;
static nvmlDeviceGetHandleByIndex_t            s_nvmlGetDevice = nullptr;
static nvmlDeviceGetName_t                     s_nvmlGetName   = nullptr;
static nvmlDeviceGetPowerUsage_t               s_nvmlGetPower  = nullptr;
static nvmlDeviceGetTemperature_t              s_nvmlGetTemp   = nullptr;
static nvmlDeviceGetGraphicsRunningProcesses_t s_nvmlGetGfx    = nullptr;
static nvmlDeviceGetComputeRunningProcesses_t  s_nvmlGetComp   = nullptr;

static std::wstring s_cachedGpuName;

// Number of consecutive idle queries (0 processes) before releasing NVML
// to allow the driver to transition the chip into D3Cold sleep.
static const int    GPU_IDLE_UNLOAD_CYCLES   = 2;  // ~4s at 2s query rate
static int          s_idleCount              = 0;

// After unloading NVML, stay sleeping for this many cycles before checking again
static const int    GPU_SLEEP_RECHECK_CYCLES = 20; // ~40s
static int          s_sleepCounter           = 0;

// Quick check: does nvml.dll exist on this system?
static bool NvmlDllExists() {
    if (s_nvmlHasGpu) return true;
    HMODULE h = LoadLibraryA("nvml.dll");
    if (!h) return false;
    FreeLibrary(h);
    s_nvmlHasGpu = true;
    return true;
}

static void EnsureCachedGpuName() {
    if (!s_cachedGpuName.empty()) return;

    // Fast check via Windows Display Devices (instant, 0 wakeup)
    DISPLAY_DEVICEW dd = { sizeof(dd) };
    for (DWORD i = 0; EnumDisplayDevicesW(NULL, i, &dd, 0); i++) {
        if (wcsstr(dd.DeviceString, L"NVIDIA") || wcsstr(dd.DeviceString, L"GeForce")) {
            s_cachedGpuName = dd.DeviceString;
            return;
        }
    }
}

static bool NvmlDoInit() {
    if (s_nvmlInited) return true;
    if (!NvmlDllExists()) return false;

    s_hNvml = LoadLibraryA("nvml.dll");
    if (!s_hNvml) return false;

    s_nvmlInit      = (nvmlInit_t)                             GetProcAddress(s_hNvml, "nvmlInit_v2");
    s_nvmlShutdown  = (nvmlShutdown_t)                         GetProcAddress(s_hNvml, "nvmlShutdown");
    s_nvmlGetDevice = (nvmlDeviceGetHandleByIndex_t)           GetProcAddress(s_hNvml, "nvmlDeviceGetHandleByIndex_v2");
    s_nvmlGetName   = (nvmlDeviceGetName_t)                    GetProcAddress(s_hNvml, "nvmlDeviceGetName");
    s_nvmlGetPower  = (nvmlDeviceGetPowerUsage_t)              GetProcAddress(s_hNvml, "nvmlDeviceGetPowerUsage");
    s_nvmlGetTemp   = (nvmlDeviceGetTemperature_t)             GetProcAddress(s_hNvml, "nvmlDeviceGetTemperature");
    s_nvmlGetGfx    = (nvmlDeviceGetGraphicsRunningProcesses_t)GetProcAddress(s_hNvml, "nvmlDeviceGetGraphicsRunningProcesses_v3");
    s_nvmlGetComp   = (nvmlDeviceGetComputeRunningProcesses_t) GetProcAddress(s_hNvml, "nvmlDeviceGetComputeRunningProcesses_v3");

    if (!s_nvmlInit || s_nvmlInit() != 0) {
        FreeLibrary(s_hNvml); s_hNvml = nullptr;
        return false;
    }

    if (!s_nvmlGetDevice || s_nvmlGetDevice(0, &s_nvmlDev) != 0) {
        if (s_nvmlShutdown) s_nvmlShutdown();
        FreeLibrary(s_hNvml); s_hNvml = nullptr;
        return false;
    }

    // Cache model name from NVML if not yet cached
    if (s_cachedGpuName.empty() && s_nvmlGetName) {
        char name[64] = { 0 };
        if (s_nvmlGetName(s_nvmlDev, name, sizeof(name)) == 0) {
            std::string n(name);
            s_cachedGpuName = std::wstring(n.begin(), n.end());
        }
    }

    s_nvmlInited = true;
    s_idleCount  = 0;
    return true;
}

static void NvmlDoShutdown() {
    if (!s_nvmlInited) return;
    if (s_nvmlShutdown) s_nvmlShutdown();
    if (s_hNvml) { FreeLibrary(s_hNvml); s_hNvml = nullptr; }
    s_nvmlInited = false;
    s_nvmlDev    = nullptr;
}

// ============================================================
// Helpers
// ============================================================
static std::wstring GetProcessName(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return L"PID " + std::to_wstring(pid);
    wchar_t buffer[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size)) {
        CloseHandle(hProcess);
        std::wstring fullPath(buffer);
        size_t slash = fullPath.find_last_of(L"\\/");
        if (slash != std::wstring::npos) return fullPath.substr(slash + 1);
        return fullPath;
    }
    CloseHandle(hProcess);
    return L"PID " + std::to_wstring(pid);
}

// ============================================================
// GpuController::Query  --  adaptive NVML: sleeps when GPU is idle
// ============================================================
GpuSnapshot GpuController::Query() {
    GpuSnapshot snap;
    std::lock_guard<std::mutex> lock(s_nvmlMutex);

    EnsureCachedGpuName();

    // If NVML is shut down (GPU sleeping in D3Cold), stay asleep until recheck period
    if (!s_nvmlInited) {
        s_sleepCounter++;
        if (s_sleepCounter < GPU_SLEEP_RECHECK_CYCLES) {
            snap.isAvailable  = true;
            snap.model        = s_cachedGpuName;
            snap.powerWatts   = 0.0;
            snap.temperatureC = 0;
            snap.isAwake      = false;
            return snap;
        }

        // Time to recheck: probe NVML
        s_sleepCounter = 0;
        if (!NvmlDoInit()) {
            snap.isAvailable  = true;
            snap.model        = s_cachedGpuName;
            snap.powerWatts   = 0.0;
            snap.temperatureC = 0;
            snap.isAwake      = false;
            return snap;
        }
    }

    if (!NvmlDoInit()) {
        snap.isAvailable  = true;
        snap.model        = s_cachedGpuName;
        snap.powerWatts   = 0.0;
        snap.temperatureC = 0;
        snap.isAwake      = false;
        return snap;
    }

    snap.isAvailable = true;
    EnsureCachedGpuName();
    snap.model = s_cachedGpuName;

    // Power reading with sensor glitch protection:
    // On NVIDIA RTX 40-series Laptop GPUs, when power gating kicks in,
    // nvmlDeviceGetPowerUsage returns a bogus 590006 mW (590 W).
    // An RTX 4050 Laptop max TDP is 95W (up to 115W Dynamic Boost).
    unsigned int pwr = 0;
    if (s_nvmlGetPower && s_nvmlGetPower(s_nvmlDev, &pwr) == 0) {
        if (pwr > 125000 || pwr == 590006) {
            snap.powerWatts = 0.0; // Filter out 590W glitch & sensor artifacts
        } else {
            snap.powerWatts = pwr / 1000.0;
        }
    }

    // Temperature reading
    unsigned int temp = 0;
    if (s_nvmlGetTemp && s_nvmlGetTemp(s_nvmlDev, 0, &temp) == 0) {
        if (temp > 115) {
            snap.temperatureC = 0;
        } else {
            snap.temperatureC = temp;
        }
    }

    // Graphics processes
    unsigned int count = 0;
    if (s_nvmlGetGfx) {
        s_nvmlGetGfx(s_nvmlDev, &count, nullptr);
        if (count > 0) {
            std::vector<nvmlProcessInfo_raw> procs(count);
            if (s_nvmlGetGfx(s_nvmlDev, &count, procs.data()) == 0) {
                for (unsigned int i = 0; i < count; i++) {
                    GpuProcess gp;
                    gp.pid = procs[i].pid;
                    if (procs[i].usedGpuMemory != (unsigned long long)-1 &&
                        procs[i].usedGpuMemory < 64ULL*1024*1024*1024)
                        gp.memoryMB = procs[i].usedGpuMemory / (1024*1024);
                    gp.name = GetProcessName(gp.pid);
                    snap.processes.push_back(gp);
                }
            }
        }
    }

    // Compute processes (de-duplicate)
    unsigned int compCount = 0;
    if (s_nvmlGetComp) {
        s_nvmlGetComp(s_nvmlDev, &compCount, nullptr);
        if (compCount > 0) {
            std::vector<nvmlProcessInfo_raw> procs(compCount);
            if (s_nvmlGetComp(s_nvmlDev, &compCount, procs.data()) == 0) {
                for (unsigned int i = 0; i < compCount; i++) {
                    bool exists = false;
                    for (const auto& p : snap.processes) {
                        if (p.pid == procs[i].pid) { exists = true; break; }
                    }
                    if (!exists) {
                        GpuProcess gp;
                        gp.pid = procs[i].pid;
                        if (procs[i].usedGpuMemory != (unsigned long long)-1 &&
                            procs[i].usedGpuMemory < 64ULL*1024*1024*1024)
                            gp.memoryMB = procs[i].usedGpuMemory / (1024*1024);
                        gp.name = GetProcessName(gp.pid);
                        snap.processes.push_back(gp);
                    }
                }
            }
        }
    }

    snap.isAwake = (snap.powerWatts > 2.0 || !snap.processes.empty());

    // ---- Adaptive idle shutdown ----
    // When no processes are using the dGPU, unload NVML after 2 consecutive cycles (~4s).
    // Releasing NVML is MANDATORY for the NVIDIA driver to put the GPU into D3Cold sleep (0W).
    if (snap.processes.empty()) {
        s_idleCount++;
        if (s_idleCount >= GPU_IDLE_UNLOAD_CYCLES) {
            Logger::Info(L"GpuCtrl", L"dGPU idle (0 processes) - releasing NVML to allow D3Cold sleep");
            NvmlDoShutdown();
            s_sleepCounter    = 0;
            snap.isAwake      = false;
            snap.powerWatts   = 0.0;
            snap.temperatureC = 0;
        }
    } else {
        s_idleCount = 0; // reset if active processes detected
    }

    return snap;
}

void GpuController::Shutdown() {
    std::lock_guard<std::mutex> lock(s_nvmlMutex);
    NvmlDoShutdown();
}

// ============================================================
// Process root PID detection & termination
// ============================================================
static std::wstring ToLowerStr(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}

static std::wstring GetDirectoryOf(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) return path.substr(0, pos);
    return L"";
}

static DWORD FindAppRootPid(DWORD targetPid, std::wstring& outExeName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return targetPid;

    struct ProcItem { DWORD pid; DWORD parentPid; std::wstring name; };
    std::unordered_map<DWORD, ProcItem> procs;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do { procs[pe.th32ProcessID] = { pe.th32ProcessID, pe.th32ParentProcessID, pe.szExeFile }; }
        while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    if (procs.find(targetPid) == procs.end()) return targetPid;
    outExeName = procs[targetPid].name;

    auto getFullPath = [](DWORD pid) -> std::wstring {
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!h) return L"";
        wchar_t buf[MAX_PATH] = { 0 };
        DWORD sz = MAX_PATH;
        if (QueryFullProcessImageNameW(h, 0, buf, &sz)) { CloseHandle(h); return buf; }
        CloseHandle(h); return L"";
    };

    DWORD curr = targetPid;
    std::wstring targetPath = getFullPath(targetPid);
    std::wstring targetDir  = ToLowerStr(GetDirectoryOf(targetPath));

    auto isSystemShell = [](const std::wstring& name) {
        std::wstring n = ToLowerStr(name);
        return (n==L"explorer.exe"||n==L"services.exe"||n==L"svchost.exe"||
                n==L"winlogon.exe"||n==L"csrss.exe"||n==L"smss.exe"||
                n==L"dwm.exe"||n==L"devenv.exe"||n==L"cmd.exe"||
                n==L"powershell.exe"||n==L"windowsterminal.exe"||
                n==L"energysavior.exe"||n==L"energycontrol.exe");
    };

    while (true) {
        auto it = procs.find(curr);
        if (it == procs.end()) break;
        DWORD ppid = it->second.parentPid;
        if (ppid == 0 || ppid == curr) break;
        auto pit = procs.find(ppid);
        if (pit == procs.end()) break;
        if (isSystemShell(pit->second.name)) break;

        std::wstring parentPath = getFullPath(ppid);
        std::wstring parentDir  = ToLowerStr(GetDirectoryOf(parentPath));
        bool sameName    = (ToLowerStr(it->second.name) == ToLowerStr(pit->second.name));
        bool sameDir     = (!targetDir.empty() && !parentDir.empty() &&
                            (parentDir==targetDir ||
                             targetDir.rfind(parentDir,0)==0 ||
                             parentDir.rfind(targetDir,0)==0));
        std::wstring cName = ToLowerStr(it->second.name);
        std::wstring pName = ToLowerStr(pit->second.name);
        bool relatedName = (cName.size()>=5 && pName.size()>=5 &&
                            (cName.find(pName.substr(0,5))!=std::wstring::npos ||
                             pName.find(cName.substr(0,5))!=std::wstring::npos));

        if (sameName || sameDir || relatedName) {
            curr = ppid; outExeName = pit->second.name;
        } else break;
    }
    return curr;
}

bool GpuController::TerminateProcessByPid(DWORD pid) {
    if (pid == 0 || pid == GetCurrentProcessId()) return false;

    std::wstring exeName;
    DWORD rootPid = FindAppRootPid(pid, exeName);

    Logger::Info(L"GpuCtrl", L"Terminating PID " + std::to_wstring(pid) +
                              L" (root PID: " + std::to_wstring(rootPid) +
                              (!exeName.empty() ? (L", " + exeName) : L"") + L")");

    if (rootPid != pid && rootPid != 0) {
        HANDLE hRoot = OpenProcess(PROCESS_TERMINATE, FALSE, rootPid);
        if (hRoot) { TerminateProcess(hRoot, 1); CloseHandle(hRoot); }
    }

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess) { TerminateProcess(hProcess, 1); CloseHandle(hProcess); }

    wchar_t cmd[128];
    swprintf_s(cmd, L"taskkill.exe /F /T /PID %u", rootPid);
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 1000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }

    if (rootPid != pid) {
        swprintf_s(cmd, L"taskkill.exe /F /T /PID %u", pid);
        if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 500);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
    }
    return true;
}

void GpuController::TerminateAllRogueProcesses() {
    GpuSnapshot s = Query();
    for (const auto& p : s.processes)
        TerminateProcessByPid(p.pid);
}