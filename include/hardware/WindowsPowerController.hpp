#pragma once
#include <windows.h>
#include <string>

enum class WindowsPowerMode : DWORD {
    PowerEfficiency = 0, // Найкраща енергоефективність
    Balanced = 1,        // Збалансований
    BestPerformance = 2, // Найвища продуктивність
    Unknown = 999
};

struct WindowsPowerStatus {
    bool isEnergySaverActive = false; // Режим енергозбереження Windows (Battery Saver / Energy Saver)
    WindowsPowerMode acPowerMode = WindowsPowerMode::Balanced;
    WindowsPowerMode dcPowerMode = WindowsPowerMode::Balanced;
    std::wstring activeSchemeName = L"Збалансований";
};

class WindowsPowerController {
public:
    static WindowsPowerStatus Query();
    static bool SetPowerMode(WindowsPowerMode mode, bool onBattery);
    static bool ToggleEnergySaver();
    static bool ApplyEcoProfile();
    static bool ApplyPerformanceProfile();
    static std::wstring ModeToString(WindowsPowerMode mode);
};
