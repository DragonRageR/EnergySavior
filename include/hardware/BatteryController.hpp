#pragma once
#include <windows.h>

struct BatterySnapshot {
    bool isConnected = false;
    bool isAcOnline = false;
    bool isDischarging = false;
    bool isCharging = false;
    int percent = 0;
    DWORD currentCapacityMWh = 0;
    DWORD fullCapacityMWh = 0;
    DWORD designCapacityMWh = 0;
    long dischargeRateMW = 0;
    DWORD voltageMV = 0;
    DWORD cycleCount = 0;
    double healthPercent = 100.0;
    double wearPercent = 0.0;
    int estimatedMinutes = -1;
};

class BatteryController {
public:
    static BatterySnapshot Query();
};
