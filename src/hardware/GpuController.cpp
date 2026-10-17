#include "../../include/hardware/GpuController.hpp"
#include <iostream>

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

static std::wstring GetProcessName(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return L"PID " + std::to_wstring(pid);

    wchar_t buffer[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size)) {
        CloseHandle(hProcess);
        std::wstring fullPath(buffer);
        size_t slash = fullPath.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            return fullPath.substr(slash + 1);
        }
        return fullPath;
    }
    CloseHandle(hProcess);
    return L"PID " + std::to_wstring(pid);
}

GpuSnapshot GpuController::Query() {
    GpuSnapshot snap;

    HMODULE hNvml = LoadLibraryA("nvml.dll");
    if (!hNvml) return snap;

    nvmlInit_t nvmlInit = (nvmlInit_t)GetProcAddress(hNvml, "nvmlInit_v2");
    nvmlShutdown_t nvmlShutdown = (nvmlShutdown_t)GetProcAddress(hNvml, "nvmlShutdown");
    nvmlDeviceGetHandleByIndex_t getDevice = (nvmlDeviceGetHandleByIndex_t)GetProcAddress(hNvml, "nvmlDeviceGetHandleByIndex_v2");
    nvmlDeviceGetName_t getName = (nvmlDeviceGetName_t)GetProcAddress(hNvml, "nvmlDeviceGetName");
    nvmlDeviceGetPowerUsage_t getPower = (nvmlDeviceGetPowerUsage_t)GetProcAddress(hNvml, "nvmlDeviceGetPowerUsage");
    nvmlDeviceGetTemperature_t getTemp = (nvmlDeviceGetTemperature_t)GetProcAddress(hNvml, "nvmlDeviceGetTemperature");
    nvmlDeviceGetGraphicsRunningProcesses_t getGfx = (nvmlDeviceGetGraphicsRunningProcesses_t)GetProcAddress(hNvml, "nvmlDeviceGetGraphicsRunningProcesses_v3");
    nvmlDeviceGetComputeRunningProcesses_t getComp = (nvmlDeviceGetComputeRunningProcesses_t)GetProcAddress(hNvml, "nvmlDeviceGetComputeRunningProcesses_v3");

    if (!nvmlInit || nvmlInit() != 0) {
        FreeLibrary(hNvml);
        return snap;
    }

    void* dev = nullptr;
    if (getDevice && getDevice(0, &dev) == 0) {
        snap.isAvailable = true;

        char name[64] = { 0 };
        if (getName && getName(dev, name, sizeof(name)) == 0) {
            std::string n(name);
            snap.model = std::wstring(n.begin(), n.end());
        }

        unsigned int pwr = 0;
        if (getPower && getPower(dev, &pwr) == 0) {
            snap.powerWatts = pwr / 1000.0;
        }

        unsigned int temp = 0;
        if (getTemp && getTemp(dev, 0, &temp) == 0) {
            snap.temperatureC = temp;
        }

        // Query graphics processes
        unsigned int count = 0;
        if (getGfx) {
            getGfx(dev, &count, nullptr);
            if (count > 0) {
                std::vector<nvmlProcessInfo_raw> procs(count);
                if (getGfx(dev, &count, procs.data()) == 0) {
                    for (unsigned int i = 0; i < count; i++) {
                        GpuProcess gp;
                        gp.pid = procs[i].pid;
                        if (procs[i].usedGpuMemory != (unsigned long long)-1 && procs[i].usedGpuMemory < 64ULL * 1024 * 1024 * 1024) {
                            gp.memoryMB = procs[i].usedGpuMemory / (1024 * 1024);
                        }
                        gp.name = GetProcessName(gp.pid);
                        snap.processes.push_back(gp);
                    }
                }
            }
        }

        // Query compute processes
        unsigned int compCount = 0;
        if (getComp) {
            getComp(dev, &compCount, nullptr);
            if (compCount > 0) {
                std::vector<nvmlProcessInfo_raw> procs(compCount);
                if (getComp(dev, &compCount, procs.data()) == 0) {
                    for (unsigned int i = 0; i < compCount; i++) {
                        bool exists = false;
                        for (const auto& p : snap.processes) {
                            if (p.pid == procs[i].pid) { exists = true; break; }
                        }
                        if (!exists) {
                            GpuProcess gp;
                            gp.pid = procs[i].pid;
                            if (procs[i].usedGpuMemory != (unsigned long long)-1 && procs[i].usedGpuMemory < 64ULL * 1024 * 1024 * 1024) {
                                gp.memoryMB = procs[i].usedGpuMemory / (1024 * 1024);
                            }
                            gp.name = GetProcessName(gp.pid);
                            snap.processes.push_back(gp);
                        }
                    }
                }
            }
        }

        snap.isAwake = (snap.powerWatts > 1.5 || !snap.processes.empty());
    }

    if (nvmlShutdown) nvmlShutdown();
    FreeLibrary(hNvml);
    return snap;
}

bool GpuController::TerminateProcessByPid(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) return false;
    BOOL res = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return (res != FALSE);
}

void GpuController::TerminateAllRogueProcesses() {
    GpuSnapshot s = Query();
    for (const auto& p : s.processes) {
        TerminateProcessByPid(p.pid);
    }
}
