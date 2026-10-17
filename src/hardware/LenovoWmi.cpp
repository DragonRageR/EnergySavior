#include "../../include/hardware/LenovoWmi.hpp"
#include "../../include/core/AppConfig.hpp"
#include <thread>

void LenovoWmi::ExecuteSilentWmi(const std::wstring& powershellBody) {
    std::thread([powershellBody]() {
        std::wstring cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"" + powershellBody + L"\"";

        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = { 0 };

        std::wstring cmdCopy = cmd;
        if (CreateProcessW(NULL, cmdCopy.data(), NULL, NULL, FALSE, 
                           CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 3500);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }).detach();
}

bool LenovoWmi::SetThermalMode(ThermalMode mode) {
    s_thermalMode = mode;
    int val = static_cast<int>(mode);

    std::wstring body = L"$i = Get-CimInstance -Namespace root/wmi -ClassName LENOVO_GAMEZONE_DATA -ErrorAction SilentlyContinue; "
                        L"if ($i) { Invoke-CimMethod -InputObject $i -MethodName SetSmartFanMode -Arguments @{Data = [uint32]" + 
                        std::to_wstring(val) + L"} }";
    ExecuteSilentWmi(body);
    return true;
}

ThermalMode LenovoWmi::GetThermalMode() {
    return s_thermalMode;
}

bool LenovoWmi::SetMuxMode(GpuMuxMode mode) {
    s_muxMode = mode;
    int val = static_cast<int>(mode);

    std::wstring body = L"$i = Get-CimInstance -Namespace root/wmi -ClassName LENOVO_GAMEZONE_DATA -ErrorAction SilentlyContinue; "
                        L"if ($i) { Invoke-CimMethod -InputObject $i -MethodName SetIGPUModeStatus -Arguments @{mode = [uint32]" + 
                        std::to_wstring(val) + L"} }";
    ExecuteSilentWmi(body);
    return true;
}

GpuMuxMode LenovoWmi::GetMuxMode() {
    return s_muxMode;
}

bool LenovoWmi::SetKeyboardLight(KbdBacklight light) {
    s_kbdLight = light;
    int val = static_cast<int>(light);

    std::wstring body = L"$i = Get-CimInstance -Namespace root/wmi -ClassName LENOVO_GAMEZONE_DATA -ErrorAction SilentlyContinue; "
                        L"if ($i) { Invoke-CimMethod -InputObject $i -MethodName SetKeyboardLight -Arguments @{Data = [uint32]" + 
                        std::to_wstring(val) + L"} }";
    ExecuteSilentWmi(body);
    return true;
}

KbdBacklight LenovoWmi::GetKeyboardLight() {
    return s_kbdLight;
}

void LenovoWmi::ToggleKeyboardLight() {
    int next = (static_cast<int>(s_kbdLight) + 1) % 3;
    SetKeyboardLight(static_cast<KbdBacklight>(next));
}

std::wstring LenovoWmi::ThermalToString(ThermalMode mode) {
    switch (mode) {
        case ThermalMode::Quiet: return L"Quiet (Синій Fn+Q)";
        case ThermalMode::Balance: return L"Balance (Білий)";
        case ThermalMode::Performance: return L"Performance (Червоний)";
        default: return L"Balance";
    }
}

std::wstring LenovoWmi::MuxToString(GpuMuxMode mode) {
    switch (mode) {
        case GpuMuxMode::Hybrid: return L"Hybrid (Динамічний)";
        case GpuMuxMode::iGpuOnly: return L"iGPU Only (Енергозбереження)";
        case GpuMuxMode::dGpuOnly: return L"dGPU Only (Ігровий MUX)";
        default: return L"Hybrid";
    }
}

std::wstring LenovoWmi::KbdToString(KbdBacklight light) {
    switch (light) {
        case KbdBacklight::Off: return L"Вимкнено";
        case KbdBacklight::Low: return L"Низька";
        case KbdBacklight::High: return L"Висока";
        default: return L"Низька";
    }
}
