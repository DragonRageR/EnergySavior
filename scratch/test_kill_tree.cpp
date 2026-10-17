#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>

// Get parent PID and full image path
struct ProcDetails {
    DWORD pid = 0;
    DWORD parentPid = 0;
    std::wstring name;
    std::wstring fullPath;
};

ProcDetails GetProcDetails(DWORD pid) {
    ProcDetails details;
    details.pid = pid;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (pe.th32ProcessID == pid) {
                    details.parentPid = pe.th32ParentProcessID;
                    details.name = pe.szExeFile;
                    break;
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc) {
        wchar_t buf[MAX_PATH] = {0};
        DWORD sz = MAX_PATH;
        if (QueryFullProcessImageNameW(hProc, 0, buf, &sz)) {
            details.fullPath = buf;
        }
        CloseHandle(hProc);
    }

    return details;
}

int main() {
    auto d = GetProcDetails(6796);
    std::wcout << L"PID: " << d.pid << L" Parent: " << d.parentPid 
               << L" Name: " << d.name << L" Path: " << d.fullPath << L"\n";
    if (d.parentPid > 0) {
        auto pd = GetProcDetails(d.parentPid);
        std::wcout << L"Parent Name: " << pd.name << L" Parent Path: " << pd.fullPath << L"\n";
    }
    return 0;
}
