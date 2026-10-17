#include <windows.h>
#include <wbemidl.h>
#include <iostream>

int GetBrightnessWmi() {
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    IWbemLocator* pLoc = NULL;
    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hr)) return 50;

    IWbemServices* pSvc = NULL;
    BSTR ns = SysAllocString(L"ROOT\\WMI");
    hr = pLoc->ConnectServer(ns, NULL, NULL, 0, 0, 0, 0, &pSvc);
    SysFreeString(ns);
    if (FAILED(hr)) {
        pLoc->Release();
        return 50;
    }

    CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

    BSTR lang = SysAllocString(L"WQL");
    BSTR query = SysAllocString(L"SELECT CurrentBrightness FROM WmiMonitorBrightness");
    IEnumWbemClassObject* pEnumerator = NULL;
    hr = pSvc->ExecQuery(lang, query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
    SysFreeString(lang);
    SysFreeString(query);

    int brightness = 50;
    if (SUCCEEDED(hr) && pEnumerator) {
        IWbemClassObject* pclsObj = NULL;
        ULONG uReturn = 0;
        if (pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn) == S_OK && uReturn > 0) {
            VARIANT vtProp;
            VariantInit(&vtProp);
            if (SUCCEEDED(pclsObj->Get(L"CurrentBrightness", 0, &vtProp, 0, 0))) {
                brightness = vtProp.uiVal;
                VariantClear(&vtProp);
            }
            pclsObj->Release();
        }
        pEnumerator->Release();
    }

    pSvc->Release();
    pLoc->Release();
    return brightness;
}

bool SetBrightnessWmi(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    IWbemLocator* pLoc = NULL;
    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hr)) return false;

    IWbemServices* pSvc = NULL;
    BSTR ns = SysAllocString(L"ROOT\\WMI");
    hr = pLoc->ConnectServer(ns, NULL, NULL, 0, 0, 0, 0, &pSvc);
    SysFreeString(ns);
    if (FAILED(hr)) {
        pLoc->Release();
        return false;
    }

    CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

    BSTR lang = SysAllocString(L"WQL");
    BSTR query = SysAllocString(L"SELECT * FROM WmiMonitorBrightnessMethods");
    IEnumWbemClassObject* pEnumerator = NULL;
    hr = pSvc->ExecQuery(lang, query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
    SysFreeString(lang);
    SysFreeString(query);

    bool success = false;
    if (SUCCEEDED(hr) && pEnumerator) {
        IWbemClassObject* pclsObj = NULL;
        ULONG uReturn = 0;
        if (pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn) == S_OK && uReturn > 0) {
            VARIANT vtPath;
            VariantInit(&vtPath);
            if (SUCCEEDED(pclsObj->Get(L"__RELPATH", 0, &vtPath, 0, 0))) {
                IWbemClassObject* pClass = NULL;
                BSTR className = SysAllocString(L"WmiMonitorBrightnessMethods");
                hr = pSvc->GetObject(className, 0, NULL, &pClass, NULL);
                SysFreeString(className);

                if (SUCCEEDED(hr) && pClass) {
                    IWbemClassObject* pInParamsDef = NULL;
                    hr = pClass->GetMethod(L"WmiSetBrightness", 0, &pInParamsDef, NULL);
                    if (SUCCEEDED(hr) && pInParamsDef) {
                        IWbemClassObject* pInParams = NULL;
                        pInParamsDef->SpawnInstance(0, &pInParams);
                        if (pInParams) {
                            VARIANT vTimeout;
                            VariantInit(&vTimeout);
                            vTimeout.vt = VT_UI4;
                            vTimeout.ulVal = 1;
                            pInParams->Put(L"Timeout", 0, &vTimeout, 0);

                            VARIANT vBrightness;
                            VariantInit(&vBrightness);
                            vBrightness.vt = VT_UI1;
                            vBrightness.bVal = (BYTE)percent;
                            pInParams->Put(L"Brightness", 0, &vBrightness, 0);

                            IWbemClassObject* pOutParams = NULL;
                            hr = pSvc->ExecMethod(vtPath.bstrVal, BSTR(L"WmiSetBrightness"), 0, NULL, pInParams, &pOutParams, NULL);
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
    pLoc->Release();
    return success;
}

int main() {
    int b = GetBrightnessWmi();
    std::cout << "Original Brightness: " << b << "%" << std::endl;
    bool s = SetBrightnessWmi(b);
    std::cout << "SetBrightness result: " << (s ? "SUCCESS" : "FAILED") << std::endl;
    return 0;
}
