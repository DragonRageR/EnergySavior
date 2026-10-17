#include "../../include/hardware/DisplayController.hpp"
#include <set>

DWORD DisplayController::GetCurrentRefreshRate() {
    DEVMODE dm = { 0 };
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
        return dm.dmDisplayFrequency;
    }
    return 60;
}

std::vector<DWORD> DisplayController::GetSupportedRefreshRates() {
    std::set<DWORD> freqs;
    DEVMODE current = { 0 };
    current.dmSize = sizeof(current);
    if (!EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &current)) {
        return { 60, 144 };
    }

    DEVMODE dm = { 0 };
    dm.dmSize = sizeof(dm);
    DWORD modeNum = 0;
    while (EnumDisplaySettings(NULL, modeNum++, &dm)) {
        if (dm.dmPelsWidth == current.dmPelsWidth && dm.dmPelsHeight == current.dmPelsHeight) {
            if (dm.dmDisplayFrequency >= 30) {
                freqs.insert(dm.dmDisplayFrequency);
            }
        }
    }
    return std::vector<DWORD>(freqs.begin(), freqs.end());
}

bool DisplayController::SetRefreshRate(DWORD hz) {
    DEVMODE dm = { 0 };
    dm.dmSize = sizeof(dm);
    if (!EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
        return false;
    }

    if (dm.dmDisplayFrequency == hz) {
        return true;
    }

    dm.dmDisplayFrequency = hz;
    dm.dmFields = DM_DISPLAYFREQUENCY;

    LONG res = ChangeDisplaySettings(&dm, CDS_UPDATEREGISTRY);
    if (res == DISP_CHANGE_SUCCESSFUL) {
        return true;
    }
    return (ChangeDisplaySettings(&dm, 0) == DISP_CHANGE_SUCCESSFUL);
}
