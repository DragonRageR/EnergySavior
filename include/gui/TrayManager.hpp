#pragma once
#include <windows.h>
#include <string>

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_OPEN 2001
#define ID_TRAY_SAVIOR 2011
#define ID_TRAY_ECO 2002
#define ID_TRAY_PERF 2003
#define ID_TRAY_CUSTOM 2012
#define ID_TRAY_SETTINGS 2010
#define ID_TRAY_AUTOSWITCH 2004
#define ID_TRAY_AUTOSTART 2005
#define ID_TRAY_OPEN_LOG 2006
#define ID_TRAY_CHECK_UPDATE 2007
#define ID_TRAY_REPORT_BUG 2009
#define ID_TRAY_EXIT 2008

#include "../core/AppConfig.hpp"

class TrayManager {
private:
    static inline HWND s_hWnd = NULL;
    static inline NOTIFYICONDATAW s_nid = { 0 };
    static inline HICON s_hIconSavior = NULL;
    static inline HICON s_hIconEco = NULL;
    static inline HICON s_hIconPerf = NULL;
    static inline HICON s_hIconCustom = NULL;

    static HICON GenerateBadgeIcon(COLORREF bgColor, COLORREF textColor, const wchar_t* label);

public:
    static void Init(HWND hWnd);
    static void UpdateIcon(bool isEco);
    static void UpdateIcon(AppPowerProfile profile);
    static void ShowBalloon(const std::wstring& title, const std::wstring& msg);
    static void ShowContextMenu();
    static void HandleTrayMessage(LPARAM lParam);
    static void Destroy();
};
