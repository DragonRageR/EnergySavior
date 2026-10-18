#pragma once
#include <windows.h>
#include <string>

#define APP_NAME L"EnergySavior"
#define APP_VERSION L"2026.10.4-alpha"
#define APP_STAGE L"Alpha"
#define APP_YEAR_MONTH L"2026.10"
#define APP_ISSUES_URL L"https://github.com/DragonRageR/EnergySavior/issues/new"
#define APP_UPDATE_HOST L"raw.githubusercontent.com"
#define APP_UPDATE_PATH L"/DragonRageR/EnergySavior/master/version.json"

class AppConfig {
private:
    static std::wstring GetConfigDir();
    static std::wstring GetConfigFile();

public:
    static std::wstring GetAppVersion() { return APP_VERSION; }
    static std::wstring GetAppStage() { return APP_STAGE; }
    static std::wstring GetAppYearMonth() { return APP_YEAR_MONTH; }

    static bool IsElevated();
    static bool IsAutoSwitchEnabled();
    static void SetAutoSwitchEnabled(bool enable);
    static bool IsAutoStartEnabled();
    static bool SetAutoStartEnabled(bool enable);
    static bool IsAutoUpdateCheckEnabled();
    static void SetAutoUpdateCheckEnabled(bool enable);
};
