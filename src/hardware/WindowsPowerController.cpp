#include "../../include/hardware/WindowsPowerController.hpp"
#include "../../include/core/Logger.hpp"
#include <powrprof.h>

typedef DWORD (WINAPI *PowerGetUserConfiguredACPowerMode_t)(DWORD*);
typedef DWORD (WINAPI *PowerGetUserConfiguredDCPowerMode_t)(DWORD*);
typedef DWORD (WINAPI *PowerSetUserConfiguredACPowerMode_t)(DWORD);
typedef DWORD (WINAPI *PowerSetUserConfiguredDCPowerMode_t)(DWORD);

WindowsPowerStatus WindowsPowerController::Query() {
    WindowsPowerStatus st;

    // 1. Query Windows Battery Saver / Energy Saver status (bit 0 of SystemStatusFlag)
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        st.isEnergySaverActive = ((sps.SystemStatusFlag & 1) != 0);
    }

    // 2. Query Windows Power Mode (Overlay Modes: 0 = Power Efficiency, 1 = Balanced, 2 = Best Performance)
    HMODULE hPowr = LoadLibraryW(L"powrprof.dll");
    if (hPowr) {
        auto getAc = (PowerGetUserConfiguredACPowerMode_t)GetProcAddress(hPowr, "PowerGetUserConfiguredACPowerMode");
        auto getDc = (PowerGetUserConfiguredDCPowerMode_t)GetProcAddress(hPowr, "PowerGetUserConfiguredDCPowerMode");

        DWORD val = 0;
        if (getAc && getAc(&val) == ERROR_SUCCESS) {
            st.acPowerMode = static_cast<WindowsPowerMode>(val);
        }
        if (getDc && getDc(&val) == ERROR_SUCCESS) {
            st.dcPowerMode = static_cast<WindowsPowerMode>(val);
        }
        FreeLibrary(hPowr);
    }

    // 3. Query Active Scheme Friendly Name
    GUID* activeScheme = nullptr;
    if (PowerGetActiveScheme(NULL, &activeScheme) == ERROR_SUCCESS) {
        DWORD bufferSize = 256;
        UCHAR buffer[256] = { 0 };
        if (PowerReadFriendlyName(NULL, activeScheme, NULL, NULL, buffer, &bufferSize) == ERROR_SUCCESS) {
            st.activeSchemeName = reinterpret_cast<wchar_t*>(buffer);
        }
        LocalFree(activeScheme);
    }

    return st;
}

bool WindowsPowerController::SetPowerMode(WindowsPowerMode mode, bool onBattery) {
    HMODULE hPowr = LoadLibraryW(L"powrprof.dll");
    if (!hPowr) return false;

    DWORD res = ERROR_INVALID_FUNCTION;
    DWORD val = static_cast<DWORD>(mode);

    if (onBattery) {
        auto setDc = (PowerSetUserConfiguredDCPowerMode_t)GetProcAddress(hPowr, "PowerSetUserConfiguredDCPowerMode");
        if (setDc) res = setDc(val);
    } else {
        auto setAc = (PowerSetUserConfiguredACPowerMode_t)GetProcAddress(hPowr, "PowerSetUserConfiguredACPowerMode");
        if (setAc) res = setAc(val);
    }

    FreeLibrary(hPowr);

    if (res == ERROR_SUCCESS) {
        Logger::Info(L"WindowsPower", L"Windows Power Mode set to: " + ModeToString(mode) + 
                                       (onBattery ? L" (DC/Battery)" : L" (AC/Mains)"));
        return true;
    }
    return false;
}

bool WindowsPowerController::ApplyEcoProfile() {
    bool ok = true;
    // Set both DC and AC to Best Power Efficiency (0)
    ok &= SetPowerMode(WindowsPowerMode::PowerEfficiency, true);
    ok &= SetPowerMode(WindowsPowerMode::PowerEfficiency, false);
    Logger::Info(L"WindowsPower", L"Applied Windows Energy Saver profile (Best Power Efficiency)");
    return ok;
}

bool WindowsPowerController::ApplyPerformanceProfile() {
    bool ok = true;
    // Set both DC and AC to Best Performance (2)
    ok &= SetPowerMode(WindowsPowerMode::BestPerformance, false);
    ok &= SetPowerMode(WindowsPowerMode::BestPerformance, true);
    Logger::Info(L"WindowsPower", L"Applied Windows High Performance profile (Best Performance)");
    return ok;
}

std::wstring WindowsPowerController::ModeToString(WindowsPowerMode mode) {
    switch (mode) {
        case WindowsPowerMode::PowerEfficiency: return L"Енергоефективність";
        case WindowsPowerMode::Balanced:        return L"Збалансований";
        case WindowsPowerMode::BestPerformance:  return L"Найвища продуктивність";
        default:                                return L"Збалансований";
    }
}
