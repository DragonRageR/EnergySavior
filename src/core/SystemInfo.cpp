#include "../../include/core/SystemInfo.hpp"
#include <algorithm>
#include <tlhelp32.h>
#include <psapi.h>


std::wstring SystemInfo::ReadRegistryString(HKEY root, const wchar_t* subKey, const wchar_t* valName) {
    HKEY hKey;
    if (RegOpenKeyExW(root, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[256] = { 0 };
        DWORD dwSize = sizeof(buf);
        DWORD dwType = 0;
        if (RegQueryValueExW(hKey, valName, NULL, &dwType, (LPBYTE)buf, &dwSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::wstring(buf);
        }
        RegCloseKey(hKey);
    }
    return L"";
}

SystemDetails SystemInfo::Detect() {
    SystemDetails det;

    std::wstring mfg = ReadRegistryString(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemManufacturer");
    if (!mfg.empty()) det.manufacturer = mfg;

    std::wstring prod = ReadRegistryString(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemProductName");
    if (!prod.empty()) det.model = prod;

    std::wstring bios = ReadRegistryString(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BIOSVersion");
    if (!bios.empty()) det.biosVersion = bios;

    std::wstring cpu = ReadRegistryString(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString");
    if (!cpu.empty()) {
        // Trim leading spaces
        size_t first = cpu.find_first_not_of(L" ");
        if (first != std::wstring::npos) cpu = cpu.substr(first);
        det.cpuName = cpu;
    }

    // Check if Lenovo
    std::wstring mfgLower = det.manufacturer;
    std::transform(mfgLower.begin(), mfgLower.end(), mfgLower.begin(), ::towlower);
    det.isLenovo = (mfgLower.find(L"lenovo") != std::wstring::npos);

    // Check monitors count
    int monitors = GetSystemMetrics(SM_CMONITORS);
    det.externalDisplayCount = (monitors > 1) ? (monitors - 1) : 0;
    det.hasExternalDisplay = (monitors > 1);

    return det;
}

std::vector<TopProcessInfo> SystemInfo::GetTopMemoryProcesses(size_t maxCount) {
    std::vector<TopProcessInfo> list;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return list;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID <= 4) continue;
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (hProc) {
                PROCESS_MEMORY_COUNTERS pmc = { 0 };
                if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
                    TopProcessInfo info;
                    info.pid = pe.th32ProcessID;
                    info.name = pe.szExeFile;
                    info.memMB = pmc.WorkingSetSize / (1024 * 1024);
                    list.push_back(info);
                }
                CloseHandle(hProc);
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    std::sort(list.begin(), list.end(), [](const TopProcessInfo& a, const TopProcessInfo& b) {
        return a.memMB > b.memMB;
    });

    if (list.size() > maxCount) {
        list.resize(maxCount);
    }
    return list;
}

