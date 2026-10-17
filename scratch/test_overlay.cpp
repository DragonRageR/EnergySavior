#include <windows.h>
#include <powrprof.h>
#include <iostream>

typedef DWORD(WINAPI* pfnPowerSetActiveOverlayScheme)(GUID* OverlaySchemeGuid);
typedef DWORD(WINAPI* pfnPowerGetActualOverlayScheme)(GUID* OverlaySchemeGuid);

static const GUID GUID_OVERLAY_ECO = { 0x961cc777, 0x2547, 0x4f9d, { 0x81, 0x74, 0x7d, 0x86, 0x18, 0x1b, 0x8a, 0x7a } };
static const GUID GUID_OVERLAY_BALANCED = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

int main() {
    HMODULE hPowr = LoadLibraryW(L"powrprof.dll");
    if (!hPowr) return 1;

    auto pfnGet = (pfnPowerGetActualOverlayScheme)GetProcAddress(hPowr, "PowerGetActualOverlayScheme");
    auto pfnSet = (pfnPowerSetActiveOverlayScheme)GetProcAddress(hPowr, "PowerSetActiveOverlayScheme");

    GUID cur = { 0 };
    if (pfnGet && pfnGet(&cur) == ERROR_SUCCESS) {
        bool isEco = IsEqualGUID(cur, GUID_OVERLAY_ECO);
        std::cout << "Current Overlay is Eco: " << (isEco ? "YES" : "NO") << std::endl;
    }

    return 0;
}
