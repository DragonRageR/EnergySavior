#pragma once
#include <windows.h>
#include <string>

class AppConfig {
private:
    static std::wstring GetConfigDir();
    static std::wstring GetConfigFile();

public:
    static bool IsElevated();
    static bool IsAutoSwitchEnabled();
    static void SetAutoSwitchEnabled(bool enable);
    static bool IsAutoStartEnabled();
    static bool SetAutoStartEnabled(bool enable);
};
