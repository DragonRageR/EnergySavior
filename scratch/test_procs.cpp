#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <algorithm>

struct ProcEntry {
    DWORD pid;
    std::wstring name;
    unsigned long long memMB;
};

int main() {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 1;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    std::vector<ProcEntry> procs;

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID <= 4) continue;
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            unsigned long long mem = 0;
            if (hProcess) {
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    mem = pmc.WorkingSetSize / (1024 * 1024);
                }
                CloseHandle(hProcess);
            }
            procs.push_back({ pe.th32ProcessID, pe.szExeFile, mem });
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    std::sort(procs.begin(), procs.end(), [](const ProcEntry& a, const ProcEntry& b) {
        return a.memMB > b.memMB;
    });

    std::cout << "Top 5 RAM Processes:\n";
    for (size_t i = 0; i < procs.size() && i < 5; i++) {
        std::wcout << (i + 1) << L". " << procs[i].name << L" (PID " << procs[i].pid << L") - " << procs[i].memMB << L" MB\n";
    }
    return 0;
}
