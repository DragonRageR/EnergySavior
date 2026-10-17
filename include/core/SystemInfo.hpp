#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

struct SystemDetails {
    std::wstring manufacturer = L"Universal PC";
    std::wstring model = L"Laptop / Desktop";
    std::wstring biosVersion = L"";
    std::wstring cpuName = L"Intel/AMD Processor";
    std::wstring gpuName = L"Dedicated Graphics";
    std::wstring iGpuName = L"Integrated Graphics";
    bool isLenovo = false;
    bool hasExternalDisplay = false;
    int externalDisplayCount = 0;
};

struct TopProcessInfo {
    DWORD pid = 0;
    std::wstring name;
    unsigned long long memMB = 0;
    float cpuPercent = 0.0f;
    float estimatedWatts = 0.0f;
    std::wstring role;
};

class SystemInfo {
private:
    static std::wstring ReadRegistryString(HKEY root, const wchar_t* subKey, const wchar_t* valName);

public:
    static SystemDetails Detect();
    static std::wstring DetectIGpuName();
    static float GetSystemCpuUsagePercent();
    static std::vector<TopProcessInfo> GetTopMemoryProcesses(size_t maxCount = 6);
    static std::vector<TopProcessInfo> GetTopCpuProcesses(size_t maxCount = 12, float totalCpuWatts = 15.0f);
    static std::vector<TopProcessInfo> GetIGpuProcesses(const std::vector<DWORD>& dGpuPids, size_t maxCount = 12);
    static bool TerminateProcessByPid(DWORD pid);
};
