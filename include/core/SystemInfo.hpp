#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct SystemDetails {
    std::wstring manufacturer = L"Generic";
    std::wstring model = L"PC Laptop";
    std::wstring biosVersion = L"Unknown";
    std::wstring cpuName = L"Intel/AMD Processor";
    std::wstring gpuName = L"Dedicated Graphics";
    bool isLenovo = false;
    bool hasExternalDisplay = false;
    int externalDisplayCount = 0;
};

struct TopProcessInfo {
    DWORD pid = 0;
    std::wstring name;
    unsigned long long memMB = 0;
};

class SystemInfo {
private:
    static std::wstring ReadRegistryString(HKEY root, const wchar_t* subKey, const wchar_t* valName);

public:
    static SystemDetails Detect();
    static std::vector<TopProcessInfo> GetTopMemoryProcesses(size_t maxCount = 6);
};
