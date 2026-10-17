#pragma once
#include <windows.h>
#include <mutex>
#include <atomic>
#include <string>
#include "../hardware/BatteryController.hpp"
#include "../hardware/DisplayController.hpp"
#include "../hardware/CpuController.hpp"
#include "../hardware/GpuController.hpp"
#include "../hardware/LenovoWmi.hpp"
#include "../hardware/WindowsPowerController.hpp"
#include "SystemInfo.hpp"

struct TelemetryData {
    BatterySnapshot battery;
    DWORD refreshRate = 144;
    CpuBoostMode cpuBoost = CpuBoostMode::Aggressive;
    GpuSnapshot gpu;
    ThermalMode thermalMode = ThermalMode::Balance;
    GpuMuxMode muxMode = GpuMuxMode::Hybrid;
    KbdBacklight kbdLight = KbdBacklight::Low;
    WindowsPowerStatus winPower;
    SystemDetails system;
    bool isEcoMode = false;
    bool isElevated = false;
};

class TelemetryThread {
private:
    static inline std::atomic<bool> s_running{false};
    static inline std::mutex s_mutex;
    static inline TelemetryData s_data;
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

    static void ApplyEcoProfile();
    static void ApplyPerformanceProfile();
    static void ToggleMode();

    static void SetRefreshRate(DWORD hz);
    static void SetCpuBoost(CpuBoostMode mode);
    static void SetThermalMode(ThermalMode mode);
    static void SetMuxMode(GpuMuxMode mode);
    static void SetKeyboardLight(KbdBacklight light);

    static bool CanNotify();
    static void MarkNotificationSent();
};
