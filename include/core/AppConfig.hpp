#pragma once
#include <windows.h>
#include <string>

#define APP_NAME L"Lenovo EnergyControl"
#define APP_VERSION L"0.2.0-alpha"
#define APP_STAGE L"Alpha (Pre-release)"

class AppConfig {
private:
    static std::wstring GetConfigDir();
    static std::wstring GetConfigFile();

public:
    static std::wstring GetAppVersion() { return APP_VERSION; }
    static std::wstring GetAppStage() { return APP_STAGE; }

    static bool IsElevated();
    static bool IsAutoSwitchEnabled();
    static void SetAutoSwitchEnabled(bool enable);
    static bool IsAutoStartEnabled();
    static bool SetAutoStartEnabled(bool enable);
};
