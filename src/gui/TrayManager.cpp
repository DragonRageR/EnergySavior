#include "../../include/gui/TrayManager.hpp"
#include "../../include/core/TelemetryThread.hpp"
#include "../../include/core/AppConfig.hpp"
#include <shellapi.h>

HICON TrayManager::GenerateBadgeIcon(COLORREF bgColor, COLORREF textColor, const wchar_t* label) {
    int sz = GetSystemMetrics(SM_CXSMICON);
    if (sz < 16) sz = 16;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmColor = CreateCompatibleBitmap(hdcScreen, sz, sz);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmColor);

    RECT rc = { 0, 0, sz, sz };
    HBRUSH hBg = CreateSolidBrush(bgColor);
    FillRect(hdcMem, &rc, hBg);
    DeleteObject(hBg);

    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HPEN hOldPen = (HPEN)SelectObject(hdcMem, hPen);
    SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
    RoundRect(hdcMem, 0, 0, sz, sz, 4, 4);
    SelectObject(hdcMem, hOldPen);
    DeleteObject(hPen);

    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, textColor);
    int fontH = sz > 16 ? 12 : 9;
    HFONT hFont = CreateFontW(fontH, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, 0, 0, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);

    DrawTextW(hdcMem, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcMem, hOldFont);
    DeleteObject(hFont);

    HBITMAP hbmMask = CreateBitmap(sz, sz, 1, 1, NULL);

    SelectObject(hdcMem, hbmOld);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    ICONINFO ii = { 0 };
    ii.fIcon = TRUE;
    ii.hbmColor = hbmColor;
    ii.hbmMask = hbmMask;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);

    return hIcon;
}

void TrayManager::Init(HWND hWnd) {
    s_hWnd = hWnd;

    s_hIconSavior = GenerateBadgeIcon(RGB(5, 150, 105), RGB(255, 255, 255), L"SAV");
    s_hIconEco = GenerateBadgeIcon(RGB(16, 185, 129), RGB(255, 255, 255), L"ECO");
    s_hIconPerf = GenerateBadgeIcon(RGB(14, 165, 233), RGB(255, 255, 255), L"MAX");
    s_hIconCustom = GenerateBadgeIcon(RGB(168, 85, 247), RGB(255, 255, 255), L"CST");

    ZeroMemory(&s_nid, sizeof(s_nid));
    s_nid.cbSize = sizeof(s_nid);
    s_nid.hWnd = hWnd;
    s_nid.uID = 1001;
    s_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    s_nid.uCallbackMessage = WM_TRAYICON;
    s_nid.hIcon = s_hIconPerf;
    wcscpy_s(s_nid.szTip, L"EnergySavior (Клік — дія, ПКМ — меню)");

    Shell_NotifyIconW(NIM_ADD, &s_nid);
}

void TrayManager::UpdateIcon(bool isEco) {
    UpdateIcon(isEco ? AppPowerProfile::Eco : AppPowerProfile::Max);
}

void TrayManager::UpdateIcon(AppPowerProfile profile) {
    s_nid.uFlags = NIF_ICON | NIF_TIP;
    switch (profile) {
        case AppPowerProfile::Savior:
            s_nid.hIcon = s_hIconSavior ? s_hIconSavior : s_hIconEco;
            wcscpy_s(s_nid.szTip, L"EnergySavior • [SAVIOR] Екстремальна економія (60Hz, Boost OFF, Eco-throttle)");
            break;
        case AppPowerProfile::Eco:
            s_nid.hIcon = s_hIconEco;
            wcscpy_s(s_nid.szTip, L"EnergySavior • [ECO] Енергозбереження (60Hz, Boost OFF, Quiet)");
            break;
        case AppPowerProfile::Max:
            s_nid.hIcon = s_hIconPerf;
            wcscpy_s(s_nid.szTip, L"EnergySavior • [MAX] Продуктивність (144Hz, Boost ON)");
            break;
        case AppPowerProfile::Custom:
            s_nid.hIcon = s_hIconCustom ? s_hIconCustom : s_hIconPerf;
            wcscpy_s(s_nid.szTip, L"EnergySavior • [CUSTOM] Власний профіль налаштувань");
            break;
    }
    Shell_NotifyIconW(NIM_MODIFY, &s_nid);
}

void TrayManager::ShowBalloon(const std::wstring& /*title*/, const std::wstring& /*msg*/) {
    // Windows OS notifications are completely disabled globally per user request
    return;
}

void TrayManager::ShowContextMenu() {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN, L"Відкрити вікно EnergySavior");
    SetMenuDefaultItem(hMenu, ID_TRAY_OPEN, FALSE);
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    TelemetryData td = TelemetryThread::GetData();
    AppendMenuW(hMenu, MF_STRING | (td.powerProfile == AppPowerProfile::Savior ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_SAVIOR, L"[SAVIOR] Режим 'Ультра-економія' (Savior)");
    AppendMenuW(hMenu, MF_STRING | (td.powerProfile == AppPowerProfile::Eco ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_ECO, L"[ECO] Режим 'Енергозбереження' (60 Hz)");
    AppendMenuW(hMenu, MF_STRING | (td.powerProfile == AppPowerProfile::Max ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_PERF, L"[MAX] Режим 'Продуктивність' (144 Hz)");
    AppendMenuW(hMenu, MF_STRING | (td.powerProfile == AppPowerProfile::Custom ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_CUSTOM, L"[CUSTOM] Користувацький профіль");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"[⚙] Налаштування програми...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    bool autoSwitch = AppConfig::IsAutoSwitchEnabled();
    AppendMenuW(hMenu, MF_STRING | (autoSwitch ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_AUTOSWITCH, L"Авто-перемикач AC/DC");

    bool autoStart = AppConfig::IsAutoStartEnabled();
    AppendMenuW(hMenu, MF_STRING | (autoStart ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_AUTOSTART, L"Автозапуск з Windows");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_CHECK_UPDATE, L"[~] Перевірити оновлення...");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN_LOG, L"[!] Відкрити журнал подій (Лог)");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_REPORT_BUG, L"[!] Повідомити про баг (GitHub)...");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Вихід з програми");

    SetForegroundWindow(s_hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, s_hWnd, NULL);
    PostMessage(s_hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

void TrayManager::HandleTrayMessage(LPARAM lParam) {
    switch (lParam) {
        case WM_LBUTTONUP:
        case NIN_SELECT: {
            TrayLeftClickAction act = AppConfig::GetTrayLeftClickAction();
            if (act == TrayLeftClickAction::CycleProfiles) {
                TelemetryThread::CycleProfile();
            } else if (act == TrayLeftClickAction::ToggleWindow) {
                if (IsWindowVisible(s_hWnd)) {
                    ShowWindow(s_hWnd, SW_HIDE);
                } else {
                    ShowWindow(s_hWnd, SW_SHOW);
                    SetForegroundWindow(s_hWnd);
                }
            } else if (act == TrayLeftClickAction::ShowMenu) {
                ShowContextMenu();
            }
            break;
        }
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU: {
            ShowContextMenu();
            break;
        }
        case WM_LBUTTONDBLCLK: {
            ShowWindow(s_hWnd, SW_SHOW);
            SetForegroundWindow(s_hWnd);
            break;
        }
    }
}

void TrayManager::Destroy() {
    Shell_NotifyIconW(NIM_DELETE, &s_nid);
    if (s_hIconSavior) DestroyIcon(s_hIconSavior);
    if (s_hIconEco) DestroyIcon(s_hIconEco);
    if (s_hIconPerf) DestroyIcon(s_hIconPerf);
    if (s_hIconCustom) DestroyIcon(s_hIconCustom);
}