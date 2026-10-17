#include "../../include/core/SystemInfo.hpp"
#include <cstdint>
#include <algorithm>
#include <tlhelp32.h>
#include <psapi.h>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

static std::unordered_map<DWORD, uint64_t> s_prevProcTimes;
static uint64_t s_prevSysKernelTime = 0;
static uint64_t s_prevSysUserTime = 0;
static uint64_t s_prevSysIdleTime = 0;

static uint64_t FileTimeToU64(const FILETIME& ft) {
    ULARGE_INTEGER li;
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    return li.QuadPart;
}

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
        size_t first = cpu.find_first_not_of(L" ");
        if (first != std::wstring::npos) cpu = cpu.substr(first);
        det.cpuName = cpu;
    }

    std::wstring mfgLower = det.manufacturer;
    std::transform(mfgLower.begin(), mfgLower.end(), mfgLower.begin(), ::towlower);
    det.isLenovo = (mfgLower.find(L"lenovo") != std::wstring::npos);

    det.iGpuName = DetectIGpuName();

    int monitors = GetSystemMetrics(SM_CMONITORS);
    det.externalDisplayCount = (monitors > 1) ? (monitors - 1) : 0;
    det.hasExternalDisplay = (monitors > 1);

    return det;
}

std::wstring SystemInfo::DetectIGpuName() {
    DISPLAY_DEVICEW dd = { sizeof(dd) };
    int i = 0;
    while (EnumDisplayDevicesW(NULL, i, &dd, 0)) {
        if (!(dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)) {
            std::wstring name(dd.DeviceString);
            std::wstring lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            if (lower.find(L"intel") != std::wstring::npos ||
                lower.find(L"uhd") != std::wstring::npos ||
                lower.find(L"iris") != std::wstring::npos ||
                (lower.find(L"amd") != std::wstring::npos && lower.find(L"radeon") != std::wstring::npos && lower.find(L"laptop gpu") == std::wstring::npos)) {
                return name;
            }
            if ((dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) && lower.find(L"nvidia") == std::wstring::npos) {
                return name;
            }
        }
        i++;
    }
    return L"Intel(R) UHD Graphics";
}

float SystemInfo::GetSystemCpuUsagePercent() {
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return 0.0f;

    uint64_t idle = FileTimeToU64(idleTime);
    uint64_t kernel = FileTimeToU64(kernelTime);
    uint64_t user = FileTimeToU64(userTime);

    if (s_prevSysKernelTime == 0) {
        s_prevSysIdleTime = idle;
        s_prevSysKernelTime = kernel;
        s_prevSysUserTime = user;
        return 6.0f;
    }

    uint64_t idleDelta = (idle > s_prevSysIdleTime) ? (idle - s_prevSysIdleTime) : 0;
    uint64_t totalDelta = ((kernel + user) > (s_prevSysKernelTime + s_prevSysUserTime)) 
                        ? ((kernel + user) - (s_prevSysKernelTime + s_prevSysUserTime)) : 0;

    s_prevSysIdleTime = idle;
    s_prevSysKernelTime = kernel;
    s_prevSysUserTime = user;

    if (totalDelta == 0) return 0.0f;
    float usage = 100.0f * (1.0f - (float)idleDelta / (float)totalDelta);
    if (usage < 0.0f) usage = 0.0f;
    if (usage > 100.0f) usage = 100.0f;
    return usage;
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

std::vector<TopProcessInfo> SystemInfo::GetTopCpuProcesses(size_t maxCount, float totalCpuWatts) {
    std::vector<TopProcessInfo> list;

    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return list;

    uint64_t curSysTime = FileTimeToU64(kernelTime) + FileTimeToU64(userTime);
    static uint64_t s_prevProcSysTime = 0;
    uint64_t sysDelta = (s_prevProcSysTime > 0 && curSysTime > s_prevProcSysTime) 
                        ? (curSysTime - s_prevProcSysTime) : 0;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return list;

    std::unordered_map<DWORD, uint64_t> currentTimes;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID <= 4) continue;
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (hProc) {
                FILETIME creation, exit, kTime, uTime;
                if (GetProcessTimes(hProc, &creation, &exit, &kTime, &uTime)) {
                    uint64_t procTime = FileTimeToU64(kTime) + FileTimeToU64(uTime);
                    currentTimes[pe.th32ProcessID] = procTime;

                    float cpu = 0.0f;
                    if (sysDelta > 0) {
                        auto it = s_prevProcTimes.find(pe.th32ProcessID);
                        if (it != s_prevProcTimes.end() && procTime > it->second) {
                            uint64_t procDelta = procTime - it->second;
                            cpu = (float)(procDelta * 100.0) / (float)sysDelta;
                            if (cpu > 100.0f) cpu = 100.0f;
                        }
                    }

                    PROCESS_MEMORY_COUNTERS pmc = { 0 };
                    unsigned long long mem = 0;
                    if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
                        mem = pmc.WorkingSetSize / (1024 * 1024);
                    }

                    TopProcessInfo info;
                    info.pid = pe.th32ProcessID;
                    info.name = pe.szExeFile;
                    info.memMB = mem;
                    info.cpuPercent = cpu;

                    float procWatts = 0.1f + (cpu / 100.0f) * (totalCpuWatts > 2.0f ? totalCpuWatts : 12.0f);
                    info.estimatedWatts = procWatts;

                    if (cpu > 15.0f) {
                        info.role = L"Високе навантаження";
                    } else if (cpu > 5.0f) {
                        info.role = L"Помірне навантаження";
                    } else if (mem > 600) {
                        info.role = L"Інтенсивна пам'ять";
                    } else {
                        info.role = L"Фоновий процес";
                    }

                    list.push_back(info);
                }
                CloseHandle(hProc);
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    s_prevProcTimes = currentTimes;
    s_prevProcSysTime = curSysTime;

    std::sort(list.begin(), list.end(), [](const TopProcessInfo& a, const TopProcessInfo& b) {
        if (std::abs(a.cpuPercent - b.cpuPercent) > 0.1f) return a.cpuPercent > b.cpuPercent;
        return a.memMB > b.memMB;
    });

    if (list.size() > maxCount) {
        list.resize(maxCount);
    }
    return list;
}

std::vector<TopProcessInfo> SystemInfo::GetIGpuProcesses(const std::vector<DWORD>& dGpuPids, size_t maxCount) {
    std::vector<TopProcessInfo> list;
    std::unordered_set<DWORD> dPids(dGpuPids.begin(), dGpuPids.end());
    std::unordered_set<DWORD> seenPids;

    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        if (IsWindowVisible(hwnd) && GetWindowTextLengthW(hwnd) > 0) {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid > 4) {
                auto* s = (std::unordered_set<DWORD>*)lp;
                s->insert(pid);
            }
        }
        return TRUE;
    }, (LPARAM)&seenPids);

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"dwm.exe") == 0 ||
                    _wcsicmp(pe.szExeFile, L"explorer.exe") == 0 ||
                    _wcsicmp(pe.szExeFile, L"ShellExperienceHost.exe") == 0 ||
                    _wcsicmp(pe.szExeFile, L"SearchHost.exe") == 0 ||
                    _wcsicmp(pe.szExeFile, L"StartMenuExperienceHost.exe") == 0) {
                    seenPids.insert(pe.th32ProcessID);
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    for (DWORD pid : seenPids) {
        if (dPids.find(pid) != dPids.end()) continue;

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProc) continue;

        wchar_t path[MAX_PATH] = { 0 };
        DWORD pathLen = MAX_PATH;
        std::wstring exeName;
        if (QueryFullProcessImageNameW(hProc, 0, path, &pathLen)) {
            std::wstring p(path);
            size_t slash = p.find_last_of(L"\\/");
            exeName = (slash != std::wstring::npos) ? p.substr(slash + 1) : p;
        }

        PROCESS_MEMORY_COUNTERS pmc = { 0 };
        unsigned long long mem = 0;
        if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
            mem = pmc.WorkingSetSize / (1024 * 1024);
        }
        CloseHandle(hProc);

        if (exeName.empty()) continue;

        TopProcessInfo info;
        info.pid = pid;
        info.name = exeName;
        info.memMB = mem;
        info.cpuPercent = 0.0f;
        info.estimatedWatts = 0.5f;

        std::wstring lower = exeName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

        if (lower == L"dwm.exe") {
            info.role = L"Оболонка Windows (DWM Композиція)";
        } else if (lower == L"explorer.exe") {
            info.role = L"Робочий стіл та Провідник";
        } else if (lower.find(L"firefox") != std::wstring::npos ||
                   lower.find(L"chrome") != std::wstring::npos ||
                   lower.find(L"msedge") != std::wstring::npos ||
                   lower.find(L"brave") != std::wstring::npos) {
            info.role = L"Апаратне Web-прискорення (iGPU)";
        } else if (lower.find(L"discord") != std::wstring::npos ||
                   lower.find(L"telegram") != std::wstring::npos ||
                   lower.find(L"slack") != std::wstring::npos ||
                   lower.find(L"spotify") != std::wstring::npos) {
            info.role = L"Апаратний рендеринг інтерфейсу";
        } else if (lower.find(L"energysavior") != std::wstring::npos) {
            info.role = L"Моніторинг живлення (GDI+ Direct)";
        } else if (lower.find(L"code") != std::wstring::npos || lower.find(L"antigravity") != std::wstring::npos) {
            info.role = L"Рендеринг IDE середовища розробки";
        } else {
            info.role = L"Віконний додаток (Еко-режим iGPU)";
        }

        list.push_back(info);
    }

    std::sort(list.begin(), list.end(), [](const TopProcessInfo& a, const TopProcessInfo& b) {
        return a.memMB > b.memMB;
    });

    if (list.size() > maxCount) {
        list.resize(maxCount);
    }
    return list;
}

bool SystemInfo::TerminateProcessByPid(DWORD pid) {
    if (pid <= 4) return false;
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return false;
    BOOL res = TerminateProcess(h, 1);
    CloseHandle(h);
    return res != FALSE;
}
