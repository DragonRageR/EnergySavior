#pragma once
#include <windows.h>
#include <vector>

class DisplayController {
public:
    static DWORD GetCurrentRefreshRate();
    static std::vector<DWORD> GetSupportedRefreshRates();
    static bool SetRefreshRate(DWORD hz);
};
