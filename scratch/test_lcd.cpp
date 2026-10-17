#include <windows.h>
#include <ntddvdeo.h>
#include <iostream>

int main() {
    HANDLE hLcd = CreateFileW(L"\\\\.\\LCD", GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, 0, NULL);
    if (hLcd != INVALID_HANDLE_VALUE) {
        DISPLAY_BRIGHTNESS db = { 0 };
        DWORD bytesRet = 0;
        if (DeviceIoControl(hLcd, IOCTL_VIDEO_QUERY_DISPLAY_BRIGHTNESS, NULL, 0,
                            &db, sizeof(db), &bytesRet, NULL)) {
            std::cout << "Current Brightness: AC=" << (int)db.ucACBrightness << "%, DC=" << (int)db.ucDCBrightness << "%\n";
            
            // Test setting same brightness back
            DISPLAY_BRIGHTNESS newDb = db;
            if (DeviceIoControl(hLcd, IOCTL_VIDEO_SET_DISPLAY_BRIGHTNESS, &newDb, sizeof(newDb),
                                NULL, 0, &bytesRet, NULL)) {
                std::cout << "IOCTL_VIDEO_SET_DISPLAY_BRIGHTNESS success!\n";
            } else {
                std::cout << "Set failed: " << GetLastError() << "\n";
            }
        }
        CloseHandle(hLcd);
    }
    return 0;
}
