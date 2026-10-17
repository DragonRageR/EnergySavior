#pragma once
#include <windows.h>
#include <vector>

class DisplayController {
public:
    static DWORD GetCurrentRefreshRate();
    static std::vector<DWORD> GetSupportedRefreshRates();
    static bool SetRefreshRate(DWORD hz);

    static int GetBrightness();
    static bool SetBrightness(int percent);
    static float CalculateDisplayWatts(DWORD hz, int brightnessPercent);
};
