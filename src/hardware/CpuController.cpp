#include "../../include/hardware/CpuController.hpp"
#include <powrprof.h>

static const GUID GUID_PROC_SETTINGS = { 0x54533251, 0x82be, 0x4824, { 0x96, 0xc1, 0x47, 0xb6, 0x0b, 0x74, 0x0d, 0x00 } };
static const GUID GUID_PROC_PERF_BOOST = { 0xbe337238, 0x0d82, 0x4146, { 0xa9, 0x60, 0x4f, 0x37, 0x49, 0xd4, 0x70, 0xc7 } };
static const GUID GUID_PROC_THROTTLE_MAX = { 0xbc5038f7, 0x23e0, 0x4960, { 0x96, 0xda, 0x33, 0xab, 0xaf, 0x59, 0x35, 0xec } };

CpuBoostMode CpuController::GetBoostMode(bool onBattery) {
    GUID* activeScheme = nullptr;
    if (PowerGetActiveScheme(NULL, &activeScheme) != ERROR_SUCCESS) {
        return CpuBoostMode::Unknown;
    }

    DWORD val = 0;
    DWORD status = onBattery ?
        PowerReadDCValueIndex(NULL, activeScheme, &GUID_PROC_SETTINGS, &GUID_PROC_PERF_BOOST, &val) :
        PowerReadACValueIndex(NULL, activeScheme, &GUID_PROC_SETTINGS, &GUID_PROC_PERF_BOOST, &val);

    LocalFree(activeScheme);
    if (status == ERROR_SUCCESS) {
        return static_cast<CpuBoostMode>(val);
    }
    return CpuBoostMode::Unknown;
}

bool CpuController::SetBoostMode(CpuBoostMode mode, bool onBattery) {
    GUID* activeScheme = nullptr;
    if (PowerGetActiveScheme(NULL, &activeScheme) != ERROR_SUCCESS) return false;

    DWORD val = static_cast<DWORD>(mode);
    DWORD status = onBattery ?
        PowerWriteDCValueIndex(NULL, activeScheme, &GUID_PROC_SETTINGS, &GUID_PROC_PERF_BOOST, val) :
        PowerWriteACValueIndex(NULL, activeScheme, &GUID_PROC_SETTINGS, &GUID_PROC_PERF_BOOST, val);

    PowerSetActiveScheme(NULL, activeScheme);
    LocalFree(activeScheme);
    return (status == ERROR_SUCCESS);
}

bool CpuController::ApplyEcoProfile() {
    GUID* activeScheme = nullptr;
    if (PowerGetActiveScheme(NULL, &activeScheme) != ERROR_SUCCESS) return false;

    PowerWriteDCValueIndex(NULL, activeScheme, &GUID_PROC_SETTINGS, &GUID_PROC_PERF_BOOST, 0); // Boost OFF
    PowerWriteDCValueIndex(NULL, activeScheme, &GUID_PROC_SETTINGS, &GUID_PROC_THROTTLE_MAX, 99);
    PowerSetActiveScheme(NULL, activeScheme);
    LocalFree(activeScheme);
    return true;
}

bool CpuController::ApplyPerformanceProfile() {
    GUID* activeScheme = nullptr;
    if (PowerGetActiveScheme(NULL, &activeScheme) != ERROR_SUCCESS) return false;

    PowerWriteACValueIndex(NULL, activeScheme, &GUID_PROC_SETTINGS, &GUID_PROC_PERF_BOOST, 2); // Aggressive
    PowerWriteACValueIndex(NULL, activeScheme, &GUID_PROC_SETTINGS, &GUID_PROC_THROTTLE_MAX, 100);
    PowerSetActiveScheme(NULL, activeScheme);
    LocalFree(activeScheme);
    return true;
}

std::wstring CpuController::ToString(CpuBoostMode mode) {
    switch (mode) {
        case CpuBoostMode::Disabled: return L"Вимкнено (~2.4 GHz)";
        case CpuBoostMode::Enabled: return L"Увімкнено (Стандартний)";
        case CpuBoostMode::Aggressive: return L"Агресивний (до 4.4 GHz)";
        case CpuBoostMode::Efficient: return L"Ефективний";
        default: return L"Невідомо";
    }
}
