#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <algorithm>

struct ProcNode {
    DWORD pid;
    DWORD parentPid;
    std::wstring name;
    std::wstring exePath;
};

std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}

std::wstring GetDir(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) return path.substr(0, pos);
    return L"";
}

DWORD FindApplicationRoot(DWORD targetPid) {
    // 1. Snapshot all processes
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return targetPid;

    std::unordered_map<DWORD, ProcNode> procs;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            ProcNode node;
            node.pid = pe.th32ProcessID;
            node.parentPid = pe.th32ParentProcessID;
            node.name = pe.szExeFile;
            procs[node.pid] = node;
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    if (procs.find(targetPid) == procs.end()) return targetPid;

    // Helper to get full path
    auto getPath = [](DWORD pid) -> std::wstring {
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!h) return L"";
        wchar_t buf[MAX_PATH] = {0};
        DWORD sz = MAX_PATH;
        if (QueryFullProcessImageNameW(h, 0, buf, &sz)) {
            CloseHandle(h);
            return buf;
        }
        CloseHandle(h);
        return L"";
    };

    DWORD curr = targetPid;
    std::wstring targetPath = getPath(targetPid);
    std::wstring targetDir = ToLower(GetDir(targetPath));

    // Known shell/system process names that should NEVER be treated as app root to kill
    auto isSystemShell = [](const std::wstring& name) {
        std::wstring n = ToLower(name);
        return (n == L"explorer.exe" || n == L"services.exe" || n == L"svchost.exe" || 
                n == L"winlogon.exe" || n == L"csrss.exe" || n == L"smss.exe" || 
                n == L"dwm.exe" || n == L"devenv.exe" || n == L"cmd.exe" || 
                n == L"powershell.exe" || n == L"windowsterminal.exe" ||
                n == L"energycontrol.exe");
    };

    while (true) {
        auto it = procs.find(curr);
        if (it == procs.end()) break;

        DWORD ppid = it->second.parentPid;
        if (ppid == 0 || ppid == curr) break;

        auto pit = procs.find(ppid);
        if (pit == procs.end()) break;

        // If parent is explorer, services, or system, stop
        if (isSystemShell(pit->second.name)) break;

        // Check if parent has same name OR same directory
        std::wstring parentPath = getPath(ppid);
        std::wstring parentDir = ToLower(GetDir(parentPath));

        bool sameName = (ToLower(it->second.name) == ToLower(pit->second.name));
        bool sameDir = (!targetDir.empty() && !parentDir.empty() && parentDir == targetDir);

        if (sameName || sameDir) {
            curr = ppid; // walk up
        } else {
            break;
        }
    }

    return curr;
}

int main() {
    DWORD root = FindApplicationRoot(6796);
    std::wcout << L"Target PID: 6796 -> App Root PID: " << root << L"\n";
    return 0;
}
