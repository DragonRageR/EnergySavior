#include "../../include/hardware/GpuController.hpp"
#include "../../include/core/Logger.hpp"
#include <iostream>
#include <tlhelp32.h>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <mutex>
#include <setupapi.h>
#include <setjmp.h>

// NVML function pointer typedefs
typedef int (*nvmlInit_t)();
typedef int (*nvmlShutdown_t)();
typedef int (*nvmlDeviceGetHandleByIndex_t)(unsigned int, void**);
typedef int (*nvmlDeviceGetName_t)(void*, char*, unsigned int);
typedef int (*nvmlDeviceGetPowerUsage_t)(void*, unsigned int*);
typedef int (*nvmlDeviceGetTemperature_t)(void*, int, unsigned int*);

// NVIDIA NVML Process Info v3 (24 bytes on x64)
// pid (4) + padding (4) + usedGpuMemory (8) + gpuInstanceId (4) + computeInstanceId (4) = 24 bytes
struct nvmlProcessInfo_v3_st {
    unsigned int pid = 0;
    unsigned long long usedGpuMemory = 0;
    unsigned int gpuInstanceId = 0;
    unsigned int computeInstanceId = 0;
};

typedef int (*nvmlDeviceGetGraphicsRunningProcesses_t)(void*, unsigned int*, nvmlProcessInfo_v3_st*);
typedef int (*nvmlDeviceGetComputeRunningProcesses_t)(void*, unsigned int*, nvmlProcessInfo_v3_st*);

// Static GUID for Display Adapter class {4d36e968-e325-11ce-bfc1-08002be10318}
static const GUID GUID_DEVCLASS_DISPLAY_VAL = { 0x4d36e968, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };

// ============================================================
// Vectored Exception Handling (VEH) Protection for NVML Calls
// Intercepts and isolates Access Violations (0xC0000005) or hardware
// faults arising from inside proprietary NVIDIA drivers (nvml.dll)
// during PCIe Runtime D3 / D3Cold power transitions.
// ============================================================
static thread_local bool    t_inNvmlCall = false;
static thread_local jmp_buf t_nvmlJmpBuf;
static PVOID                s_nvmlVehHandler = nullptr;

static LONG WINAPI NvmlVehFilter(PEXCEPTION_POINTERS ep) {
    if (t_inNvmlCall && ep && ep->ExceptionRecord) {
        DWORD code = ep->ExceptionRecord->ExceptionCode;
        if (code == EXCEPTION_ACCESS_VIOLATION ||
            code == EXCEPTION_ILLEGAL_INSTRUCTION ||
            code == EXCEPTION_DATATYPE_MISALIGNMENT ||
            code == EXCEPTION_IN_PAGE_ERROR ||
            code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED) {
            t_inNvmlCall = false;
            longjmp(t_nvmlJmpBuf, 1);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void EnsureNvmlVehInstalled() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        s_nvmlVehHandler = AddVectoredExceptionHandler(1, NvmlVehFilter);
    });
}

template<typename Func>
static bool SafeNvmlCall(Func&& func) {
    EnsureNvmlVehInstalled();
    t_inNvmlCall = true;
    if (setjmp(t_nvmlJmpBuf) == 0) {
        func();
        t_inNvmlCall = false;
        return true;
    } else {
        t_inNvmlCall = false;
        return false;
    }
}

// ============================================================
// Adaptive NVML session:
//   - Loaded on first Query() or when activity is detected
//   - Automatically unloaded after 2 consecutive idle queries (0 processes)
//     allowing the NVIDIA driver to enter D3Cold sleep (0 Watts)
//   - Re-checked periodically (every ~30s) or on demand
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
static const int    GPU_SLEEP_RECHECK_CYCLES = 2;  // ~4s recheck period (fast wake-up detection)
static int          s_sleepCounter           = 0;
static bool         s_firstQuery             = true;

static HMODULE LoadNvmlLibrary() {
    HMODULE h = LoadLibraryA("nvml.dll");
    if (h) return h;
    h = LoadLibraryW(L"C:\\Windows\\System32\\nvml.dll");
    if (h) return h;
    h = LoadLibraryW(L"C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll");
    if (h) return h;
    return nullptr;
}

static void EnsureCachedGpuName() {
    if (!s_cachedGpuName.empty()) return;

    // 1. Fast check via SetupAPI (enumerates all display adapters, including sleeping D3Cold dGPUs, 0 wake-up, 0 latency)
    HDEVINFO hDevInfo = SetupDiGetClassDevsW(&GUID_DEVCLASS_DISPLAY_VAL, NULL, NULL, 0);
    if (hDevInfo != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA devInfoData = { sizeof(SP_DEVINFO_DATA) };
        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
            wchar_t buf[256] = { 0 };
            if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, SPDRP_DEVICEDESC, NULL, (PBYTE)buf, sizeof(buf), NULL) ||
                SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME, NULL, (PBYTE)buf, sizeof(buf), NULL)) {
                if (wcsstr(buf, L"NVIDIA") || wcsstr(buf, L"GeForce")) {
                    s_cachedGpuName = buf;
                    s_nvmlHasGpu = true;
                    SetupDiDestroyDeviceInfoList(hDevInfo);
                    return;
                }
            }
        }
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }

    // 2. Secondary check via Windows Display Devices (when attached to active display)
    DISPLAY_DEVICEW dd = { sizeof(dd) };
    for (DWORD i = 0; EnumDisplayDevicesW(NULL, i, &dd, 0); i++) {
        if (wcsstr(dd.DeviceString, L"NVIDIA") || wcsstr(dd.DeviceString, L"GeForce")) {
            s_cachedGpuName = dd.DeviceString;
            s_nvmlHasGpu = true;
            return;
        }
    }
}

// Quick check: does nvml.dll exist on this system?
static bool NvmlDllExists() {
    EnsureCachedGpuName();
    if (s_nvmlHasGpu) return true;
    HMODULE h = LoadNvmlLibrary();
    if (!h) return false;
    FreeLibrary(h);
    s_nvmlHasGpu = true;
    return true;
}

static bool NvmlDoInit() {
    if (s_nvmlInited) return true;
    EnsureCachedGpuName();
    if (!NvmlDllExists()) return false;

    s_hNvml = LoadNvmlLibrary();
    if (!s_hNvml) return false;

    s_nvmlInit      = (nvmlInit_t)                             GetProcAddress(s_hNvml, "nvmlInit_v2");
    if (!s_nvmlInit) s_nvmlInit = (nvmlInit_t)                 GetProcAddress(s_hNvml, "nvmlInit");

    s_nvmlShutdown  = (nvmlShutdown_t)                         GetProcAddress(s_hNvml, "nvmlShutdown");

    s_nvmlGetDevice = (nvmlDeviceGetHandleByIndex_t)           GetProcAddress(s_hNvml, "nvmlDeviceGetHandleByIndex_v2");
    if (!s_nvmlGetDevice) s_nvmlGetDevice = (nvmlDeviceGetHandleByIndex_t)GetProcAddress(s_hNvml, "nvmlDeviceGetHandleByIndex");

    s_nvmlGetName   = (nvmlDeviceGetName_t)                    GetProcAddress(s_hNvml, "nvmlDeviceGetName");
    s_nvmlGetPower  = (nvmlDeviceGetPowerUsage_t)              GetProcAddress(s_hNvml, "nvmlDeviceGetPowerUsage");
    s_nvmlGetTemp   = (nvmlDeviceGetTemperature_t)             GetProcAddress(s_hNvml, "nvmlDeviceGetTemperature");

    s_nvmlGetGfx    = (nvmlDeviceGetGraphicsRunningProcesses_t)GetProcAddress(s_hNvml, "nvmlDeviceGetGraphicsRunningProcesses_v3");
    if (!s_nvmlGetGfx) s_nvmlGetGfx = (nvmlDeviceGetGraphicsRunningProcesses_t)GetProcAddress(s_hNvml, "nvmlDeviceGetGraphicsRunningProcesses_v2");
    if (!s_nvmlGetGfx) s_nvmlGetGfx = (nvmlDeviceGetGraphicsRunningProcesses_t)GetProcAddress(s_hNvml, "nvmlDeviceGetGraphicsRunningProcesses");

    s_nvmlGetComp   = (nvmlDeviceGetComputeRunningProcesses_t) GetProcAddress(s_hNvml, "nvmlDeviceGetComputeRunningProcesses_v3");
    if (!s_nvmlGetComp) s_nvmlGetComp = (nvmlDeviceGetComputeRunningProcesses_t)GetProcAddress(s_hNvml, "nvmlDeviceGetComputeRunningProcesses_v2");
    if (!s_nvmlGetComp) s_nvmlGetComp = (nvmlDeviceGetComputeRunningProcesses_t)GetProcAddress(s_hNvml, "nvmlDeviceGetComputeRunningProcesses");

    int initRes = -1;
    bool initOk = SafeNvmlCall([&]() {
        if (s_nvmlInit) initRes = s_nvmlInit();
    });

    if (!initOk || initRes != 0) {
        FreeLibrary(s_hNvml); s_hNvml = nullptr;
        return false;
    }

    int devRes = -1;
    bool devOk = SafeNvmlCall([&]() {
        if (s_nvmlGetDevice) devRes = s_nvmlGetDevice(0, &s_nvmlDev);
    });

    if (!devOk || devRes != 0 || !s_nvmlDev) {
        if (s_nvmlShutdown) {
            SafeNvmlCall([&]() { s_nvmlShutdown(); });
        }
        FreeLibrary(s_hNvml); s_hNvml = nullptr;
        s_nvmlDev = nullptr;
        return false;
    }

    // Cache model name from NVML if not yet cached
    if (s_cachedGpuName.empty() && s_nvmlGetName) {
        SafeNvmlCall([&]() {
            char name[64] = { 0 };
            if (s_nvmlGetName(s_nvmlDev, name, sizeof(name)) == 0) {
                std::string n(name);
                s_cachedGpuName = std::wstring(n.begin(), n.end());
            }
        });
    }

    s_nvmlInited = true;
    s_idleCount  = 0;
    return true;
}

static void NvmlDoShutdown() {
    if (!s_nvmlInited && !s_hNvml) return;
    if (s_nvmlShutdown) {
        SafeNvmlCall([&]() { s_nvmlShutdown(); });
    }
    if (s_hNvml) { FreeLibrary(s_hNvml); s_hNvml = nullptr; }
    s_nvmlInited = false;
    s_nvmlDev    = nullptr;
}

// ============================================================
// Helpers
// ============================================================
static std::wstring GetProcessName(DWORD pid) {
    if (pid == 0) return L"System";
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
    snap.model = !s_cachedGpuName.empty() ? s_cachedGpuName : L"NVIDIA GeForce GPU";
    snap.isAvailable = s_nvmlHasGpu;

    // On the very first query after startup, probe NVML immediately (no startup delay)
    if (s_firstQuery) {
        s_firstQuery = false;
        s_sleepCounter = 0;
        if (!NvmlDoInit()) {
            snap.powerWatts   = 0.0;
            snap.temperatureC = 0;
            snap.isAwake      = false;
            return snap;
        }
    } else if (!s_nvmlInited) {
        s_sleepCounter++;
        if (s_sleepCounter < GPU_SLEEP_RECHECK_CYCLES) {
            snap.powerWatts   = 0.0;
            snap.temperatureC = 0;
            snap.isAwake      = false;
            return snap;
        }

        // Time to recheck: probe NVML
        s_sleepCounter = 0;
        if (!NvmlDoInit()) {
            snap.powerWatts   = 0.0;
            snap.temperatureC = 0;
            snap.isAwake      = false;
            return snap;
        }
    }

    if (!s_nvmlInited && !NvmlDoInit()) {
        snap.powerWatts   = 0.0;
        snap.temperatureC = 0;
        snap.isAwake      = false;
        return snap;
    }

    snap.model = s_cachedGpuName;

    // 1. First, check active graphics and compute processes with SafeNvmlCall.
    // Querying processes is non-invasive and does not access the volatile RUSD power telemetry buffer.
    unsigned int count = 0;
    bool gfxCallOk = SafeNvmlCall([&]() {
        if (s_nvmlGetGfx) {
            s_nvmlGetGfx(s_nvmlDev, &count, nullptr);
        }
    });

    if (!gfxCallOk) {
        // NVML threw hardware fault / Access Violation -> GPU entered D3Cold or driver unmapped memory
        Logger::Warn(L"GpuController", L"NVML graphics process query triggered hardware fault (D3Cold transition). Safely resetting.");
        NvmlDoShutdown();
        s_sleepCounter    = 0;
        s_idleCount       = GPU_IDLE_UNLOAD_CYCLES;
        snap.isAwake      = false;
        snap.powerWatts   = 0.0;
        snap.temperatureC = 0;
        return snap;
    }

    if (count > 0 && count < 1000) {
        unsigned int allocCount = count + 8;
        std::vector<nvmlProcessInfo_v3_st> procs(allocCount);
        bool fetchOk = SafeNvmlCall([&]() {
            s_nvmlGetGfx(s_nvmlDev, &allocCount, procs.data());
        });
        if (fetchOk) {
            for (unsigned int i = 0; i < allocCount && i < procs.size(); i++) {
                if (procs[i].pid <= 4) continue;
                GpuProcess gp;
                gp.pid = procs[i].pid;
                if (procs[i].usedGpuMemory != (unsigned long long)-1 &&
                    procs[i].usedGpuMemory < 64ULL * 1024 * 1024 * 1024) {
                    gp.memoryMB = procs[i].usedGpuMemory / (1024 * 1024);
                }
                gp.name = GetProcessName(gp.pid);
                snap.processes.push_back(gp);
            }
        }
    }

    // 2. Compute processes (de-duplicate)
    unsigned int compCount = 0;
    bool compCallOk = SafeNvmlCall([&]() {
        if (s_nvmlGetComp) {
            s_nvmlGetComp(s_nvmlDev, &compCount, nullptr);
        }
    });

    if (compCallOk && compCount > 0 && compCount < 1000) {
        unsigned int allocComp = compCount + 8;
        std::vector<nvmlProcessInfo_v3_st> procs(allocComp);
        bool fetchCompOk = SafeNvmlCall([&]() {
            s_nvmlGetComp(s_nvmlDev, &allocComp, procs.data());
        });
        if (fetchCompOk) {
            for (unsigned int i = 0; i < allocComp && i < procs.size(); i++) {
                if (procs[i].pid <= 4) continue;
                bool exists = false;
                for (const auto& p : snap.processes) {
                    if (p.pid == procs[i].pid) { exists = true; break; }
                }
                if (!exists) {
                    GpuProcess gp;
                    gp.pid = procs[i].pid;
                    if (procs[i].usedGpuMemory != (unsigned long long)-1 &&
                        procs[i].usedGpuMemory < 64ULL * 1024 * 1024 * 1024) {
                        gp.memoryMB = procs[i].usedGpuMemory / (1024 * 1024);
                    }
                    gp.name = GetProcessName(gp.pid);
                    snap.processes.push_back(gp);
                }
            }
        }
    }

    // 3. Only query power and temperature if the GPU is actually active with processes!
    // When 0 processes are active, the GPU is idle / sleeping in D3Cold (0.0 W, 0 C).
    // Avoiding power query during idle completely eliminates the driver's RUSD telemetry crash (0xC0000005).
    if (!snap.processes.empty()) {
        unsigned int pwr = 0;
        bool pwrOk = SafeNvmlCall([&]() {
            if (s_nvmlGetPower) {
                s_nvmlGetPower(s_nvmlDev, &pwr);
            }
        });
        if (pwrOk) {
            if (pwr > 125000 || pwr == 590006) {
                snap.powerWatts = 0.0;
            } else {
                snap.powerWatts = pwr / 1000.0;
            }
        } else {
            snap.powerWatts = 0.0;
        }

        unsigned int temp = 0;
        bool tempOk = SafeNvmlCall([&]() {
            if (s_nvmlGetTemp) {
                s_nvmlGetTemp(s_nvmlDev, 0, &temp);
            }
        });
        if (tempOk && temp <= 115) {
            snap.temperatureC = temp;
        } else {
            snap.temperatureC = 0;
        }
    } else {
        snap.powerWatts   = 0.0;
        snap.temperatureC = 0;
    }

    snap.isAwake = (snap.powerWatts > 2.0 || !snap.processes.empty());

    // ---- Adaptive idle shutdown ----
    // When no processes are using the dGPU, unload NVML after 2 consecutive cycles (~4s).
    // Releasing NVML is MANDATORY for the NVIDIA driver to put the GPU into D3Cold sleep (0W).
    if (snap.processes.empty()) {
        s_idleCount++;
        if (s_idleCount >= GPU_IDLE_UNLOAD_CYCLES) {
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

void GpuController::RequestImmediateProbe() {
    std::lock_guard<std::mutex> lock(s_nvmlMutex);
    s_sleepCounter = GPU_SLEEP_RECHECK_CYCLES;
    s_firstQuery = true;
}

void GpuController::Shutdown() {
    std::lock_guard<std::mutex> lock(s_nvmlMutex);
    NvmlDoShutdown();
    s_firstQuery = true;
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
    while (true) {
        auto it = procs.find(curr);
        if (it == procs.end()) break;
        DWORD ppid = it->second.parentPid;
        if (ppid == 0 || ppid == curr) break;

        auto pit = procs.find(ppid);
        if (pit == procs.end()) break;

        std::wstring pName = ToLowerStr(pit->second.name);
        if (pName == L"explorer.exe" || pName == L"services.exe" ||
            pName == L"svchost.exe"  || pName == L"wininit.exe"  ||
            pName == L"csrss.exe"    || pName == L"lsass.exe") {
            break;
        }

        std::wstring cName = ToLowerStr(it->second.name);
        bool sameName = (cName == pName);

        std::wstring cPath = getFullPath(curr);
        std::wstring pPath = getFullPath(ppid);
        bool sameDir = (!cPath.empty() && !pPath.empty() &&
                        ToLowerStr(GetDirectoryOf(cPath)) == ToLowerStr(GetDirectoryOf(pPath)));

        bool relatedName = (!cName.empty() && !pName.empty() &&
                            (cName.find(pName.substr(0, std::min<size_t>(5, pName.size()))) != std::wstring::npos ||
                             pName.find(cName.substr(0, std::min<size_t>(5, cName.size()))) != std::wstring::npos));

        if (sameName || sameDir || relatedName) {
            curr = ppid;
            outExeName = pit->second.name;
        } else {
            break;
        }
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
    for (const auto& p : s.processes) {
        TerminateProcessByPid(p.pid);
    }
}