#include "../../include/hardware/WindowsPowerController.hpp"
#include "../../include/core/Logger.hpp"
#include <powrprof.h>

static const GUID SCHEME_POWER_SAVINGS = { 0xa1841308, 0x3541, 0x4fab, { 0xbc, 0x81, 0xf7, 0x15, 0x56, 0xf2, 0x0b, 0x4a } };
static const GUID SCHEME_BALANCED = { 0x381b4222, 0xf694, 0x41f0, { 0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e } };

typedef DWORD(WINAPI* pfnPowerSetActiveOverlayScheme)(GUID* OverlaySchemeGuid);
typedef DWORD(WINAPI* pfnPowerGetActualOverlayScheme)(GUID* OverlaySchemeGuid);

static const GUID GUID_OVERLAY_ECO = { 0x961cc777, 0x2547, 0x4f9d, { 0x81, 0x74, 0x7d, 0x86, 0x18, 0x1b, 0x8a, 0x7a } };
static const GUID GUID_OVERLAY_BALANCED = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

static bool s_energySaverActive = false;
static bool s_energySaverInitialized = false;

#include "../../include/hardware/DisplayController.hpp"

static const GUID GUID_SUB_ENERGYSAVER = { 0xde830923, 0xa562, 0x41af, { 0xa0, 0x86, 0xe3, 0xa2, 0xc6, 0xba, 0xd2, 0xda } };
static const GUID GUID_ESBATTTHRESHOLD  = { 0xe69653ca, 0xcf7f, 0x4f05, { 0xaa, 0x73, 0xcb, 0x83, 0x3f, 0xa9, 0x0a, 0xd4 } };
static const GUID GUID_ESBRIGHTNESS     = { 0x13d09884, 0xf74e, 0x474a, { 0xa8, 0x52, 0xb6, 0xbd, 0xe8, 0xad, 0x03, 0xa8 } };

WindowsPowerStatus WindowsPowerController::Query() {
    WindowsPowerStatus st;

    // 1. Перевірка статусу Battery Saver / Energy Saver із системного прапорця
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        if ((sps.SystemStatusFlag & 1) != 0) {
            st.isEnergySaverActive = true;
        }
    }

    // 2. Запит поточної оверлейної схеми Windows 11 (Power Mode)
    HMODULE hPowr = GetModuleHandleW(L"powrprof.dll");
    if (!hPowr) hPowr = LoadLibraryW(L"powrprof.dll");
    if (hPowr) {
        auto pfnGet = (pfnPowerGetActualOverlayScheme)GetProcAddress(hPowr, "PowerGetActualOverlayScheme");
        if (pfnGet) {
            GUID curOverlay = { 0 };
            if (pfnGet(&curOverlay) == ERROR_SUCCESS) {
                if (IsEqualGUID(curOverlay, GUID_OVERLAY_ECO)) {
                    st.isEnergySaverActive = true;
                }
            }
        }
    }

    // 3. Перевірка порогу SUB_ENERGYSAVER у політиці живлення
    DWORD acThresh = 0;
    if (PowerReadACValueIndex(NULL, &SCHEME_BALANCED, &GUID_SUB_ENERGYSAVER, &GUID_ESBATTTHRESHOLD, &acThresh) == ERROR_SUCCESS) {
        if (acThresh == 100) {
            st.isEnergySaverActive = true;
        }
    }

    if (s_energySaverInitialized) {
        st.isEnergySaverActive = s_energySaverActive;
    } else {
        s_energySaverActive = st.isEnergySaverActive;
        s_energySaverInitialized = true;
    }

    // 4. Безпечний запит назви активної схеми живлення
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
    if (mode == WindowsPowerMode::PowerEfficiency) {
        return ApplyEcoProfile();
    } else {
        return ApplyPerformanceProfile();
    }
}

bool WindowsPowerController::ToggleEnergySaver() {
    int curBrightness = DisplayController::GetBrightness();

    s_energySaverActive = !s_energySaverActive;
    bool makeActive = s_energySaverActive;
    s_energySaverInitialized = true;

    HMODULE hPowr = GetModuleHandleW(L"powrprof.dll");
    if (!hPowr) hPowr = LoadLibraryW(L"powrprof.dll");

    // 1. Перемикання оверлейного профілю енергоефективності Windows 11
    if (hPowr) {
        auto pfnSet = (pfnPowerSetActiveOverlayScheme)GetProcAddress(hPowr, "PowerSetActiveOverlayScheme");
        if (pfnSet) {
            GUID target = makeActive ? GUID_OVERLAY_ECO : GUID_OVERLAY_BALANCED;
            pfnSet(&target);
        }
    }

    // 2. Встановлення порогу активації Energy Saver (100% = примусово ввімкнено)
    DWORD acVal = makeActive ? 100 : 0;
    DWORD dcVal = makeActive ? 100 : 20;
    PowerWriteACValueIndex(NULL, &SCHEME_BALANCED, &GUID_SUB_ENERGYSAVER, &GUID_ESBATTTHRESHOLD, acVal);
    PowerWriteDCValueIndex(NULL, &SCHEME_BALANCED, &GUID_SUB_ENERGYSAVER, &GUID_ESBATTTHRESHOLD, dcVal);

    // 3. Відновлення вихідної яскравості екрана (захист від підстрибування яскравості до 70%)
    DisplayController::SetBrightness(curBrightness);

    Logger::Info(L"WindowsPower", makeActive ? (L"Режим Windows Energy Saver УВІМКНЕНО (яскравість зафіксовано на " + std::to_wstring(curBrightness) + L"%)")
                                            : L"Режим Windows Energy Saver ВИМКНЕНО (збалансований режим)");

    return makeActive;
}

bool WindowsPowerController::ApplyEcoProfile() {
    int curBrightness = DisplayController::GetBrightness();

    PowerWriteACValueIndex(NULL, &SCHEME_BALANCED, &GUID_SUB_ENERGYSAVER, &GUID_ESBATTTHRESHOLD, 100);
    PowerWriteDCValueIndex(NULL, &SCHEME_BALANCED, &GUID_SUB_ENERGYSAVER, &GUID_ESBATTTHRESHOLD, 100);

    HMODULE hPowr = GetModuleHandleW(L"powrprof.dll");
    if (!hPowr) hPowr = LoadLibraryW(L"powrprof.dll");
    if (hPowr) {
        auto pfnSet = (pfnPowerSetActiveOverlayScheme)GetProcAddress(hPowr, "PowerSetActiveOverlayScheme");
        if (pfnSet) {
            GUID target = GUID_OVERLAY_ECO;
            pfnSet(&target);
        }
    }

    DisplayController::SetBrightness(curBrightness);

    s_energySaverActive = true;
    s_energySaverInitialized = true;
    Logger::Info(L"WindowsPower", L"Активовано профіль Windows Енергозбереження із захистом яскравості");
    return true;
}

bool WindowsPowerController::ApplyPerformanceProfile() {
    int curBrightness = DisplayController::GetBrightness();

    PowerWriteACValueIndex(NULL, &SCHEME_BALANCED, &GUID_SUB_ENERGYSAVER, &GUID_ESBATTTHRESHOLD, 0);
    PowerWriteDCValueIndex(NULL, &SCHEME_BALANCED, &GUID_SUB_ENERGYSAVER, &GUID_ESBATTTHRESHOLD, 20);

    HMODULE hPowr = GetModuleHandleW(L"powrprof.dll");
    if (!hPowr) hPowr = LoadLibraryW(L"powrprof.dll");
    if (hPowr) {
        auto pfnSet = (pfnPowerSetActiveOverlayScheme)GetProcAddress(hPowr, "PowerSetActiveOverlayScheme");
        if (pfnSet) {
            GUID target = GUID_OVERLAY_BALANCED;
            pfnSet(&target);
        }
    }

    DisplayController::SetBrightness(curBrightness);

    s_energySaverActive = false;
    s_energySaverInitialized = true;
    Logger::Info(L"WindowsPower", L"Активовано збалансований профіль Windows із захистом яскравості");
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
