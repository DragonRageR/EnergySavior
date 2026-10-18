#pragma once
#include <windows.h>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include "../hardware/BatteryController.hpp"
#include "../hardware/DisplayController.hpp"
#include "../hardware/CpuController.hpp"
#include "../hardware/GpuController.hpp"
#include "../hardware/LenovoWmi.hpp"
#include "../hardware/WindowsPowerController.hpp"
#include "SystemInfo.hpp"
#include "AppConfig.hpp"

struct TelemetryData {
    BatterySnapshot battery;
    DWORD refreshRate = 144;
    int brightness = 50;
    float displayWatts = 6.0f;
    CpuBoostMode cpuBoost = CpuBoostMode::Aggressive;
    float cpuUsagePercent = 0.0f;
    float cpuWatts = 8.0f;
    std::wstring iGpuName = L"Intel(R) UHD Graphics";
    float iGpuWatts = 1.5f;
    float iGpuUsagePercent = 0.0f;
    GpuSnapshot gpu;
    ThermalMode thermalMode = ThermalMode::Balance;
    GpuMuxMode muxMode = GpuMuxMode::Hybrid;
    KbdBacklight kbdLight = KbdBacklight::Low;
    WindowsPowerStatus winPower;
    SystemDetails system;
    std::vector<TopProcessInfo> topProcesses;
    std::vector<TopProcessInfo> cpuProcesses;
    std::vector<TopProcessInfo> iGpuProcesses;
    AppPowerProfile powerProfile = AppPowerProfile::Max;
    bool isEcoMode = false;
    bool isElevated = false;
};

class TelemetryThread {
private:
    static inline std::atomic<bool> s_running{false};
    static inline std::mutex s_mutex;
    static inline TelemetryData s_data;
    static inline std::vector<float> s_powerHistory;
    static inline HWND s_notifyHwnd = NULL;
    static inline DWORD s_lastNotificationTime = 0;
    static inline int s_acStateCounter = 0;
    static inline bool s_debouncedAcState = true;
    static inline std::atomic<bool> s_isSwitching{false};

    static void WorkerLoop();

public:
    static void Start(HWND notifyHwnd);
    static void Stop();
    static TelemetryData GetData();
    static std::vector<float> GetPowerHistory(size_t maxSeconds = 60);

    static void ApplySaviorProfile();
    static void ApplyEcoProfile();
    static void ApplyPerformanceProfile();
    static void ApplyCustomProfile();
    static void SetProfile(AppPowerProfile profile);
    static void CycleProfile();
    static void ToggleMode();

    static void SetRefreshRate(DWORD hz);
    static void SetCpuBoost(CpuBoostMode mode);
    static void SetThermalMode(ThermalMode mode);
    static void SetMuxMode(GpuMuxMode mode);
    static void SetKeyboardLight(KbdBacklight light);

    static bool CanNotify();
    static void MarkNotificationSent();
    static void RefreshPowerStatus();
};
