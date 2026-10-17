#include "../../include/core/AppConfig.hpp"
#include <shlobj.h>

std::wstring AppConfig::GetConfigDir() {
    wchar_t appData[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appData))) {
        std::wstring path = std::wstring(appData) + L"\\EnergyControl";
        CreateDirectoryW(path.c_str(), NULL);
        return path;
    }
    return L".";
}

std::wstring AppConfig::GetConfigFile() {
    return GetConfigDir() + L"\\config.ini";
}

bool AppConfig::IsElevated() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return (isAdmin != FALSE);
}

bool AppConfig::IsAutoSwitchEnabled() {
    std::wstring file = GetConfigFile();
    return (GetPrivateProfileIntW(L"Settings", L"AutoSwitch", 1, file.c_str()) == 1);
}

void AppConfig::SetAutoSwitchEnabled(bool enable) {
    std::wstring file = GetConfigFile();
    WritePrivateProfileStringW(L"Settings", L"AutoSwitch", enable ? L"1" : L"0", file.c_str());
}

bool AppConfig::IsAutoStartEnabled() {
    std::wstring file = GetConfigFile();
    return (GetPrivateProfileIntW(L"Settings", L"AutoStart", 0, file.c_str()) == 1);
}

bool AppConfig::SetAutoStartEnabled(bool enable) {
    std::wstring file = GetConfigFile();
    WritePrivateProfileStringW(L"Settings", L"AutoStart", enable ? L"1" : L"0", file.c_str());

    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    // 1. Register in HKCU Run Key (for Windows Task Manager Startup Tab visibility!)
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
                      0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            std::wstring runCmd = L"\"" + std::wstring(exePath) + L"\" --minimized";
            RegSetValueExW(hKey, L"LenovoEnergyControl", 0, REG_SZ, 
                           (const BYTE*)runCmd.c_str(), (DWORD)((runCmd.length() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, L"LenovoEnergyControl");
        }
        RegCloseKey(hKey);
    }

    // 2. Register elevated Task Scheduler task for UAC bypass
    if (enable) {
        std::wstring params = L"/create /tn \"LenovoEnergyControl\" /tr \"\\\"" + 
                              std::wstring(exePath) + L"\\\" --minimized\" /rl HIGHEST /sc ONLOGON /f";
        
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = L"schtasks.exe";
        sei.lpParameters = params.c_str();
        sei.nShow = SW_HIDE;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;

        if (ShellExecuteExW(&sei)) {
            if (sei.hProcess) {
                WaitForSingleObject(sei.hProcess, 4000);
                CloseHandle(sei.hProcess);
            }
            // Allow task to start on battery
            std::wstring psCmd = L"-NoProfile -Command \"Set-ScheduledTask -TaskName 'LenovoEnergyControl' -Settings (New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit 0)\"";
            ShellExecuteW(NULL, L"runas", L"powershell.exe", psCmd.c_str(), NULL, SW_HIDE);
            return true;
        }
    } else {
        std::wstring params = L"/delete /tn \"LenovoEnergyControl\" /f";
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = L"schtasks.exe";
        sei.lpParameters = params.c_str();
        sei.nShow = SW_HIDE;
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;

        if (ShellExecuteExW(&sei)) {
            if (sei.hProcess) {
                WaitForSingleObject(sei.hProcess, 4000);
                CloseHandle(sei.hProcess);
            }
            return true;
        }
    }
    return false;
}

bool AppConfig::IsAutoUpdateCheckEnabled() {
    std::wstring file = GetConfigFile();
    return (GetPrivateProfileIntW(L"Settings", L"AutoUpdateCheck", 1, file.c_str()) == 1);
}

void AppConfig::SetAutoUpdateCheckEnabled(bool enable) {
    std::wstring file = GetConfigFile();
    WritePrivateProfileStringW(L"Settings", L"AutoUpdateCheck", enable ? L"1" : L"0", file.c_str());
}
