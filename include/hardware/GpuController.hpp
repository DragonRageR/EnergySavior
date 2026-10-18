#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct GpuProcess {
    unsigned int pid = 0;
    std::wstring name;
    unsigned long long memoryMB = 0;
};

struct GpuSnapshot {
    bool isAvailable = false;
    std::wstring model = L"N/A";
    double powerWatts = 0.0;
    unsigned int temperatureC = 0;
    bool isAwake = false;
    std::vector<GpuProcess> processes;
};

class GpuController {
public:
    static GpuSnapshot Query();
    static bool TerminateProcessByPid(DWORD pid);
    static void TerminateAllRogueProcesses();
    static void Shutdown(); // Call once on app exit to release NVML
    static void RequestImmediateProbe(); // Force immediate NVML probe on next Query
};
