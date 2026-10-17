#include "../../include/hardware/LenovoWmi.hpp"
#include "../../include/core/Logger.hpp"
#include <wincrypt.h>
#include <thread>
#include <mutex>

static std::mutex s_wmiMutex;

static std::wstring EncodePowerShellScript(const std::wstring& script) {
    DWORD size = 0;
    const BYTE* bytes = reinterpret_cast<const BYTE*>(script.data());
    DWORD byteLen = static_cast<DWORD>(script.size() * sizeof(wchar_t));
    if (CryptBinaryToStringW(bytes, byteLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &size)) {
        std::wstring out(size, 0);
        if (CryptBinaryToStringW(bytes, byteLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0], &size)) {
            while (!out.empty() && (out.back() == 0 || out.back() == L'\r' || out.back() == L'\n')) {
                out.pop_back();
            }
            return out;
        }
    }
    return L"";
}

void LenovoWmi::ExecuteSilentWmi(const std::wstring& powershellBody) {
    std::thread([powershellBody]() {
        std::lock_guard<std::mutex> lock(s_wmiMutex);

        std::wstring b64 = EncodePowerShellScript(powershellBody);
        if (b64.empty()) return;

        std::wstring cmd = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand " + b64;

        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = { 0 };

        std::wstring cmdCopy = cmd;
        if (CreateProcessW(NULL, cmdCopy.data(), NULL, NULL, FALSE, 
                           CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 4000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }).detach();
}

bool LenovoWmi::SetThermalMode(ThermalMode mode) {
    s_thermalMode = mode;
    int val = static_cast<int>(mode);

    std::wstring script = L"$i = Get-CimInstance -Namespace root/wmi -ClassName LENOVO_GAMEZONE_DATA -ErrorAction SilentlyContinue; "
                         L"if ($i) { Invoke-CimMethod -InputObject $i -MethodName SetSmartFanMode -Arguments @{Data = [uint32]" + 
                         std::to_wstring(val) + L"} -ErrorAction SilentlyContinue }";

    ExecuteSilentWmi(script);
    Logger::Info(L"LenovoWmi", L"SetThermalMode sent: " + ThermalToString(mode));
    return true;
}

ThermalMode LenovoWmi::GetThermalMode() {
    return s_thermalMode;
}

bool LenovoWmi::SetMuxMode(GpuMuxMode mode) {
    s_muxMode = mode;
    int val = static_cast<int>(mode);

    std::wstring script = L"$i = Get-CimInstance -Namespace root/wmi -ClassName LENOVO_GAMEZONE_DATA -ErrorAction SilentlyContinue; "
                         L"if ($i) { Invoke-CimMethod -InputObject $i -MethodName SetIGPUModeStatus -Arguments @{mode = [uint32]" + 
                         std::to_wstring(val) + L"; Data = [uint32]0} -ErrorAction SilentlyContinue }";

    ExecuteSilentWmi(script);
    Logger::Info(L"LenovoWmi", L"SetMuxMode sent: " + MuxToString(mode) + L" (Requires restart to apply)");
    return true;
}

GpuMuxMode LenovoWmi::GetMuxMode() {
    return s_muxMode;
}

bool LenovoWmi::SetKeyboardLight(KbdBacklight light) {
    s_kbdLight = light;
    int val = static_cast<int>(light);

    // Try both LENOVO_GAMEZONE_DATA and LENOVO_LIGHTING_METHOD to support all LOQ/Legion firmware
    std::wstring script = L"$g = Get-CimInstance -Namespace root/wmi -ClassName LENOVO_GAMEZONE_DATA -ErrorAction SilentlyContinue; "
                         L"if ($g) { Invoke-CimMethod -InputObject $g -MethodName SetKeyboardLight -Arguments @{Data = [uint32]" + 
                         std::to_wstring(val) + L"} -ErrorAction SilentlyContinue }; "
                         L"$l = Get-CimInstance -Namespace root/wmi -ClassName LENOVO_LIGHTING_METHOD -ErrorAction SilentlyContinue; "
                         L"if ($l) { Invoke-CimMethod -InputObject $l -MethodName Set_Lighting_Current_Status -Arguments @{Lighting_ID = [byte]1; Current_Brightness_Level = [byte]" +
                         std::to_wstring(val) + L"; Current_State_Type = [byte]1} -ErrorAction SilentlyContinue }";

    ExecuteSilentWmi(script);
    Logger::Info(L"LenovoWmi", L"SetKeyboardLight sent: " + KbdToString(light));
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
