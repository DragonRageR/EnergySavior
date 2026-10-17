#pragma once
#include <windows.h>
#include <string>

enum class ThermalMode {
    Quiet = 1,       // Синій індикатор (Fn+Q)
    Balance = 2,     // Білий індикатор (Fn+Q)
    Performance = 3  // Червоний індикатор (Fn+Q)
};

enum class GpuMuxMode {
    Hybrid = 0,      // Dynamic (Optimus)
    iGpuOnly = 1,    // dGPU повністю знеструмлено для максимальної економії
    dGpuOnly = 2     // Тільки дискретна (для ігор)
};

enum class KbdBacklight {
    Off = 0,
    Low = 1,
    High = 2
};

class LenovoWmi {
private:
    static inline ThermalMode s_thermalMode = ThermalMode::Balance;
    static inline GpuMuxMode s_muxMode = GpuMuxMode::Hybrid;
    static inline KbdBacklight s_kbdLight = KbdBacklight::Low;

    static void ExecuteSilentWmi(const std::wstring& powershellBody);

public:
    static bool SetThermalMode(ThermalMode mode);
    static ThermalMode GetThermalMode();

    static bool SetMuxMode(GpuMuxMode mode);
    static GpuMuxMode GetMuxMode();

    static bool SetKeyboardLight(KbdBacklight light);
    static KbdBacklight GetKeyboardLight();
    static void ToggleKeyboardLight();

    static std::wstring ThermalToString(ThermalMode mode);
    static std::wstring MuxToString(GpuMuxMode mode);
    static std::wstring KbdToString(KbdBacklight light);
};
