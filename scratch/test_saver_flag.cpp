#include <windows.h>
#include <powrprof.h>
#include <iostream>

typedef DWORD(WINAPI* pfnPowerSetActiveOverlayScheme)(GUID* OverlaySchemeGuid);
typedef DWORD(WINAPI* pfnPowerGetActualOverlayScheme)(GUID* OverlaySchemeGuid);

static const GUID SCHEME_BALANCED = { 0x381b4222, 0xf694, 0x41f0, { 0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e } };
static const GUID GUID_SUB_ENERGYSAVER = { 0xde830923, 0xa562, 0x41af, { 0xa0, 0x86, 0xe3, 0xa2, 0xc6, 0xba, 0xd2, 0xda } };
static const GUID GUID_ESBATTTHRESHOLD  = { 0xe69653ca, 0xcf7f, 0x4f05, { 0xaa, 0x73, 0xcb, 0x83, 0x3f, 0xa9, 0x0a, 0xd4 } };
static const GUID GUID_OVERLAY_ECO = { 0x961cc777, 0x2547, 0x4f9d, { 0x81, 0x74, 0x7d, 0x86, 0x18, 0x1b, 0x8a, 0x7a } };
static const GUID GUID_OVERLAY_BALANCED = { 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

int main() {
    SYSTEM_POWER_STATUS sps;
    GetSystemPowerStatus(&sps);
    std::cout << "Initial SystemStatusFlag: " << (int)sps.SystemStatusFlag << " (Saver bit: " << (sps.SystemStatusFlag & 1) << ")" << std::endl;

    HMODULE hPowr = LoadLibraryW(L"powrprof.dll");
    auto pfnSet = (pfnPowerSetActiveOverlayScheme)GetProcAddress(hPowr, "PowerSetActiveOverlayScheme");

    // Toggle to ECO
    GUID eco = GUID_OVERLAY_ECO;
    if (pfnSet) pfnSet(&eco);
    PowerWriteACValueIndex(NULL, &SCHEME_BALANCED, &GUID_SUB_ENERGYSAVER, &GUID_ESBATTTHRESHOLD, 100);
    PowerWriteDCValueIndex(NULL, &SCHEME_BALANCED, &GUID_SUB_ENERGYSAVER, &GUID_ESBATTTHRESHOLD, 100);

    GetSystemPowerStatus(&sps);
    std::cout << "After setting Eco, Saver bit: " << (sps.SystemStatusFlag & 1) << std::endl;

    return 0;
}
