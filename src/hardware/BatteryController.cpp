#include "../../include/hardware/BatteryController.hpp"
#include <batclass.h>
#include <setupapi.h>
#include <vector>
#include <cmath>

static const GUID GUID_BATTERY_INTERFACE = { 0x72631e54, 0x78a4, 0x11d0, { 0xbc, 0xf7, 0x00, 0xaa, 0x00, 0xb7, 0xb3, 0x2a } };

BatterySnapshot BatteryController::Query() {
    BatterySnapshot info;

    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        info.isAcOnline = (sps.ACLineStatus == 1);
        info.percent = sps.BatteryLifePercent;
        if (sps.BatteryLifeTime != (DWORD)-1 && sps.BatteryLifeTime > 0) {
            info.estimatedMinutes = sps.BatteryLifeTime / 60;
        }
    }

    HDEVINFO hdev = SetupDiGetClassDevs(&GUID_BATTERY_INTERFACE, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hdev == INVALID_HANDLE_VALUE) return info;

    SP_DEVICE_INTERFACE_DATA did = { sizeof(did) };
    if (!SetupDiEnumDeviceInterfaces(hdev, 0, &GUID_BATTERY_INTERFACE, 0, &did)) {
        SetupDiDestroyDeviceInfoList(hdev);
        return info;
    }

    DWORD cbRequired = 0;
    SetupDiGetDeviceInterfaceDetail(hdev, &did, 0, 0, &cbRequired, 0);
    std::vector<BYTE> buf(cbRequired);
    PSP_DEVICE_INTERFACE_DETAIL_DATA pdid = (PSP_DEVICE_INTERFACE_DETAIL_DATA)buf.data();
    pdid->cbSize = sizeof(*pdid);

    if (!SetupDiGetDeviceInterfaceDetail(hdev, &did, pdid, cbRequired, &cbRequired, 0)) {
        SetupDiDestroyDeviceInfoList(hdev);
        return info;
    }

    HANDLE hBat = CreateFile(pdid->DevicePath, GENERIC_READ | GENERIC_WRITE, 
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    SetupDiDestroyDeviceInfoList(hdev);

    if (hBat == INVALID_HANDLE_VALUE) return info;

    info.isConnected = true;
    BATTERY_QUERY_INFORMATION bqi = { 0 };
    DWORD dwWait = 0, dwOut = 0;

    if (DeviceIoControl(hBat, IOCTL_BATTERY_QUERY_TAG, &dwWait, sizeof(dwWait), 
                        &bqi.BatteryTag, sizeof(bqi.BatteryTag), &dwOut, NULL)) {
        BATTERY_INFORMATION bi = { 0 };
        bqi.InformationLevel = BatteryInformation;
        if (DeviceIoControl(hBat, IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi), 
                            &bi, sizeof(bi), &dwOut, NULL)) {
            info.designCapacityMWh = bi.DesignedCapacity;
            info.fullCapacityMWh = bi.FullChargedCapacity;
            info.cycleCount = bi.CycleCount;
            if (info.designCapacityMWh > 0) {
                info.healthPercent = (double)info.fullCapacityMWh / info.designCapacityMWh * 100.0;
                info.wearPercent = (info.healthPercent < 100.0) ? (100.0 - info.healthPercent) : 0.0;
            }
        }

        BATTERY_WAIT_STATUS bws = { 0 };
        bws.BatteryTag = bqi.BatteryTag;
        BATTERY_STATUS bs = { 0 };
        if (DeviceIoControl(hBat, IOCTL_BATTERY_QUERY_STATUS, &bws, sizeof(bws), 
                            &bs, sizeof(bs), &dwOut, NULL)) {
            info.currentCapacityMWh = bs.Capacity;
            info.voltageMV = bs.Voltage;
            info.isDischarging = (bs.PowerState & BATTERY_DISCHARGING) != 0;
            info.isCharging = (bs.PowerState & BATTERY_CHARGING) != 0;

            if (bs.Rate != 0 && (long)bs.Rate != (long)BATTERY_UNKNOWN_RATE) {
                info.dischargeRateMW = std::abs((long)bs.Rate);
            }

            if (info.isDischarging && info.dischargeRateMW > 0 && info.currentCapacityMWh > 0) {
                info.estimatedMinutes = (int)((double)info.currentCapacityMWh / info.dischargeRateMW * 60.0);
            }
        }
    }

    CloseHandle(hBat);
    return info;
}
