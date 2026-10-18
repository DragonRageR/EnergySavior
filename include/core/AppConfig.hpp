#pragma once
#include <windows.h>
#include <string>

#define APP_NAME L"EnergySavior"
#define APP_VERSION L"2026.10.5-alpha"
#define APP_STAGE L"Alpha"
#define APP_YEAR_MONTH L"2026.10"
#define APP_ISSUES_URL L"https://github.com/DragonRageR/EnergySavior/issues/new"
#define APP_UPDATE_HOST L"raw.githubusercontent.com"
#define APP_UPDATE_PATH L"/DragonRageR/EnergySavior/master/version.json"

enum class AppPowerProfile {
    Savior = 0,
    Eco = 1,
    Max = 2,
    Custom = 3
};

enum class TrayLeftClickAction {
    CycleProfiles = 0,
    ToggleWindow = 1,
    ShowMenu = 2
};

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

    // Tray Settings
    static TrayLeftClickAction GetTrayLeftClickAction();
    static void SetTrayLeftClickAction(TrayLeftClickAction action);

    // Custom Profile Settings
    static DWORD GetCustomRefreshRate();
    static void SetCustomRefreshRate(DWORD hz);
    static int GetCustomCpuBoost();
    static void SetCustomCpuBoost(int mode);
    static int GetCustomThermalMode();
    static void SetCustomThermalMode(int mode);
    static int GetCustomKbdBacklight();
    static void SetCustomKbdBacklight(int light);
    static bool GetCustomWinEnergySaver();
    static void SetCustomWinEnergySaver(bool enable);
};
