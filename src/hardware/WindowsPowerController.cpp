#include "../../include/hardware/WindowsPowerController.hpp"
#include "../../include/core/Logger.hpp"
#include <powrprof.h>

static const GUID SCHEME_POWER_SAVINGS = { 0xa1841308, 0x3541, 0x4fab, { 0xbc, 0x81, 0xf7, 0x15, 0x56, 0xf2, 0x0b, 0x4a } };
static const GUID SCHEME_BALANCED = { 0x381b4222, 0xf694, 0x41f0, { 0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e } };

WindowsPowerStatus WindowsPowerController::Query() {
    WindowsPowerStatus st;

    // 1. Query Windows Battery Saver / Energy Saver status (bit 0 of SystemStatusFlag)
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        st.isEnergySaverActive = ((sps.SystemStatusFlag & 1) != 0);
    }

    // 2. Query Active Scheme Friendly Name safely
    GUID* activeScheme = nullptr;
    if (PowerGetActiveScheme(NULL, &activeScheme) == ERROR_SUCCESS) {
        wchar_t buffer[128] = { 0 };
        DWORD bufferSize = sizeof(buffer);
        if (PowerReadFriendlyName(NULL, activeScheme, NULL, NULL, reinterpret_cast<UCHAR*>(buffer), &bufferSize) == ERROR_SUCCESS) {
            st.activeSchemeName = buffer;
        }
        LocalFree(activeScheme);
    }

    return st;
}

bool WindowsPowerController::SetPowerMode(WindowsPowerMode mode, bool /*onBattery*/) {
    // Safely switch Windows power scheme according to mode
    if (mode == WindowsPowerMode::PowerEfficiency) {
        return ApplyEcoProfile();
    } else {
        return ApplyPerformanceProfile();
    }
}

bool WindowsPowerController::ApplyEcoProfile() {
    DWORD res = PowerSetActiveScheme(NULL, &SCHEME_POWER_SAVINGS);
    if (res != ERROR_SUCCESS) {
        // Fallback to balanced if power saver scheme is not installed
        PowerSetActiveScheme(NULL, &SCHEME_BALANCED);
    }
    Logger::Info(L"WindowsPower", L"Switched to Windows Power Saver scheme");
    return true;
}

bool WindowsPowerController::ApplyPerformanceProfile() {
    PowerSetActiveScheme(NULL, &SCHEME_BALANCED);
    Logger::Info(L"WindowsPower", L"Switched to Windows Balanced / High Performance scheme");
    return true;
}

std::wstring WindowsPowerController::ModeToString(WindowsPowerMode mode) {
    switch (mode) {
        case WindowsPowerMode::PowerEfficiency: return L"Енергоефективність";
        case WindowsPowerMode::Balanced:        return L"Збалансований";
        case WindowsPowerMode::BestPerformance:  return L"Найвища продуктивність";
        default:                                return L"Збалансований";
    }
}
