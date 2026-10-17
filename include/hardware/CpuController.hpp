#pragma once
#include <windows.h>
#include <string>

enum class CpuBoostMode : DWORD {
    Disabled = 0,
    Enabled = 1,
    Aggressive = 2,
    Efficient = 3,
    Unknown = 999
};

class CpuController {
public:
    static CpuBoostMode GetBoostMode(bool onBattery);
    static bool SetBoostMode(CpuBoostMode mode, bool onBattery);
    static bool ApplyEcoProfile();
    static bool ApplyPerformanceProfile();
    static std::wstring ToString(CpuBoostMode mode);
};
