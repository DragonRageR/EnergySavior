#include "../../include/hardware/DisplayController.hpp"
#include <set>
#include <wbemidl.h>

#ifndef IOCTL_VIDEO_QUERY_DISPLAY_BRIGHTNESS
#define IOCTL_VIDEO_QUERY_DISPLAY_BRIGHTNESS CTL_CODE(FILE_DEVICE_VIDEO, 0x126, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif
#ifndef IOCTL_VIDEO_SET_DISPLAY_BRIGHTNESS
#define IOCTL_VIDEO_SET_DISPLAY_BRIGHTNESS   CTL_CODE(FILE_DEVICE_VIDEO, 0x127, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

typedef struct _LOCAL_DISPLAY_BRIGHTNESS {
    UCHAR ucDisplayPolicy;
    UCHAR ucACBrightness;
    UCHAR ucDCBrightness;
} LOCAL_DISPLAY_BRIGHTNESS;

struct ComInitScope {
    HRESULT hr;
    ComInitScope() {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    }
    ~ComInitScope() {
        if (hr == S_OK) {
            CoUninitialize();
        }
    }
};

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

int DisplayController::GetBrightness() {
    ComInitScope comScope;

    IWbemLocator* pLoc = NULL;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (SUCCEEDED(hr) && pLoc) {
        IWbemServices* pSvc = NULL;
        BSTR ns = SysAllocString(L"ROOT\\WMI");
        hr = pLoc->ConnectServer(ns, NULL, NULL, 0, 0, 0, 0, &pSvc);
        SysFreeString(ns);
        if (SUCCEEDED(hr) && pSvc) {
            CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                              RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

            BSTR lang = SysAllocString(L"WQL");
            BSTR query = SysAllocString(L"SELECT CurrentBrightness FROM WmiMonitorBrightness");
            IEnumWbemClassObject* pEnumerator = NULL;
            hr = pSvc->ExecQuery(lang, query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
            SysFreeString(lang);
            SysFreeString(query);

            if (SUCCEEDED(hr) && pEnumerator) {
                IWbemClassObject* pclsObj = NULL;
                ULONG uReturn = 0;
                if (pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn) == S_OK && uReturn > 0) {
                    VARIANT vtProp;
                    VariantInit(&vtProp);
                    if (SUCCEEDED(pclsObj->Get(L"CurrentBrightness", 0, &vtProp, 0, 0))) {
                        int b = (int)vtProp.uiVal;
                        VariantClear(&vtProp);
                        pclsObj->Release();
                        pEnumerator->Release();
                        pSvc->Release();
                        pLoc->Release();
                        return b;
                    }
                    pclsObj->Release();
                }
                pEnumerator->Release();
            }
            pSvc->Release();
        }
        pLoc->Release();
    }

    // Fallback to IOCTL_VIDEO_QUERY_DISPLAY_BRIGHTNESS
    HANDLE hLcd = CreateFileW(L"\\\\.\\LCD", GENERIC_READ, 
                              FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, OPEN_EXISTING, 0, NULL);
    if (hLcd != INVALID_HANDLE_VALUE) {
        LOCAL_DISPLAY_BRIGHTNESS b = { 0 };
        DWORD bytes = 0;
        BOOL res = DeviceIoControl(hLcd, IOCTL_VIDEO_QUERY_DISPLAY_BRIGHTNESS,
                                   NULL, 0, &b, sizeof(b), &bytes, NULL);
        CloseHandle(hLcd);
        if (res) return (int)b.ucACBrightness;
    }

    return 50;
}

bool DisplayController::SetBrightness(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    bool success = false;
    ComInitScope comScope;

    IWbemLocator* pLoc = NULL;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (SUCCEEDED(hr) && pLoc) {
        IWbemServices* pSvc = NULL;
        BSTR ns = SysAllocString(L"ROOT\\WMI");
        hr = pLoc->ConnectServer(ns, NULL, NULL, 0, 0, 0, 0, &pSvc);
        SysFreeString(ns);
        if (SUCCEEDED(hr) && pSvc) {
            CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                              RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

            BSTR lang = SysAllocString(L"WQL");
            BSTR query = SysAllocString(L"SELECT * FROM WmiMonitorBrightnessMethods");
            IEnumWbemClassObject* pEnumerator = NULL;
            hr = pSvc->ExecQuery(lang, query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
            SysFreeString(lang);
            SysFreeString(query);

            if (SUCCEEDED(hr) && pEnumerator) {
                IWbemClassObject* pclsObj = NULL;
                ULONG uReturn = 0;
                if (pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn) == S_OK && uReturn > 0) {
                    VARIANT vtPath;
                    VariantInit(&vtPath);
                    if (SUCCEEDED(pclsObj->Get(L"__PATH", 0, &vtPath, 0, 0))) {
                        IWbemClassObject* pClass = NULL;
                        BSTR className = SysAllocString(L"WmiMonitorBrightnessMethods");
                        hr = pSvc->GetObject(className, 0, NULL, &pClass, NULL);
                        SysFreeString(className);

                        if (SUCCEEDED(hr) && pClass) {
                            IWbemClassObject* pInParamsDef = NULL;
                            hr = pClass->GetMethod(L"WmiSetBrightness", 0, &pInParamsDef, NULL);
                            if (SUCCEEDED(hr) && pInParamsDef) {
                                IWbemClassObject* pInParams = NULL;
                                hr = pInParamsDef->SpawnInstance(0, &pInParams);
                                if (SUCCEEDED(hr) && pInParams) {
                                    VARIANT varTimeout;
                                    VariantInit(&varTimeout);
                                    varTimeout.vt = VT_UI4;
                                    varTimeout.ulVal = 0;
                                    pInParams->Put(L"Timeout", 0, &varTimeout, 0);

                                    VARIANT varBrightness;
                                    VariantInit(&varBrightness);
                                    varBrightness.vt = VT_UI1;
                                    varBrightness.bVal = (BYTE)percent;
                                    pInParams->Put(L"Brightness", 0, &varBrightness, 0);

                                    IWbemClassObject* pOutParams = NULL;
                                    BSTR methName = SysAllocString(L"WmiSetBrightness");
                                    hr = pSvc->ExecMethod(vtPath.bstrVal, methName, 0, NULL, pInParams, &pOutParams, NULL);
                                    SysFreeString(methName);
                                    if (SUCCEEDED(hr)) {
                                        success = true;
                                    }
                                    if (pOutParams) pOutParams->Release();
                                    pInParams->Release();
                                }
                                pInParamsDef->Release();
                            }
                            pClass->Release();
                        }
                        VariantClear(&vtPath);
                    }
                    pclsObj->Release();
                }
                pEnumerator->Release();
            }
            pSvc->Release();
        }
        pLoc->Release();
    }

    if (success) return true;

    // Fallback to IOCTL_VIDEO_SET_DISPLAY_BRIGHTNESS
    HANDLE hLcd = CreateFileW(L"\\\\.\\LCD", GENERIC_READ | GENERIC_WRITE, 
                              FILE_SHARE_READ | FILE_SHARE_WRITE, 
                              NULL, OPEN_EXISTING, 0, NULL);
    if (hLcd != INVALID_HANDLE_VALUE) {
        LOCAL_DISPLAY_BRIGHTNESS b = { 0 };
        b.ucDisplayPolicy = 1 | 2; // DISPLAY_POLICY_AC | DISPLAY_POLICY_DC
        b.ucACBrightness = (UCHAR)percent;
        b.ucDCBrightness = (UCHAR)percent;

        DWORD bytes = 0;
        BOOL res = DeviceIoControl(hLcd, IOCTL_VIDEO_SET_DISPLAY_BRIGHTNESS,
                                   &b, sizeof(b), NULL, 0, &bytes, NULL);
        CloseHandle(hLcd);
        return (res != FALSE);
    }

    return false;
}

float DisplayController::CalculateDisplayWatts(DWORD hz, int brightnessPercent) {
    float basePanel = (hz >= 120) ? 3.8f : 1.8f;
    float bClamped = (float)(brightnessPercent < 0 ? 0 : (brightnessPercent > 100 ? 100 : brightnessPercent));
    float backlight = 0.5f + (bClamped / 100.0f) * 4.5f;
    return basePanel + backlight;
}