#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <string>

#include "include/core/AppConfig.hpp"
#include "include/core/Logger.hpp"
#include "include/core/CrashHandler.hpp"
#include "include/core/UpdateManager.hpp"
#include "include/core/TelemetryThread.hpp"
#include "include/gui/GuiWindow.hpp"
#include "include/gui/TrayManager.hpp"

using namespace Gdiplus;

#ifdef _MSC_VER
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#endif

#define WINDOW_WIDTH 880
#define WINDOW_HEIGHT 720

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            GuiWindow::InitButtons();
            TelemetryThread::Start(hWnd);
            UpdateManager::Init(hWnd);
            if (AppConfig::IsAutoUpdateCheckEnabled()) {
                UpdateManager::CheckForUpdatesAsync();
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            GuiWindow::Paint(hWnd, hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Prevent flicker with double-buffering

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            // Allow dragging window by header area (excluding minimize/close buttons)
            if (y < 60 && x < 780) {
                ReleaseCapture();
                SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }

            GuiWindow::HandleClick(hWnd, x, y);
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            GuiWindow::HandleMouseMove(hWnd, x, y);
            return 0;
        }

        case WM_USER + 2: { // Telemetry worker thread update message
            if (IsWindowVisible(hWnd)) {
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_TRAYICON: {
            TrayManager::HandleTrayMessage(lParam);
            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_TRAY_OPEN:
                    ShowWindow(hWnd, SW_SHOW);
                    SetForegroundWindow(hWnd);
                    break;
                case ID_TRAY_ECO:
                    TelemetryThread::ApplyEcoProfile();
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                case ID_TRAY_PERF:
                    TelemetryThread::ApplyPerformanceProfile();
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                case ID_TRAY_AUTOSWITCH: {
                    bool cur = AppConfig::IsAutoSwitchEnabled();
                    AppConfig::SetAutoSwitchEnabled(!cur);
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                }
                case ID_TRAY_AUTOSTART: {
                    bool cur = AppConfig::IsAutoStartEnabled();
                    AppConfig::SetAutoStartEnabled(!cur);
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                }
                case ID_TRAY_CHECK_UPDATE: {
                    UpdateManager::CheckForUpdatesAsync([hWnd](UpdateStatus s, const UpdateInfo& info) {
                        if (s == UpdateStatus::UpToDate) {
                            TrayManager::ShowBalloon(L"EnergyControl", L"У вас встановлено найновішу версію!");
                        }
                    });
                    break;
                }
                case ID_TRAY_OPEN_LOG:
                    Logger::OpenLogFile();
                    break;
                case ID_TRAY_EXIT:
                    DestroyWindow(hWnd);
                    break;
            }
            return 0;
        }

        case WM_CLOSE: {
            // Minimize to tray instead of terminating
            ShowWindow(hWnd, SW_HIDE);
            TrayManager::ShowBalloon(L"EnergyControl у треї", 
                                     L"• ЛКМ по значку в треї: перемикає Еко / Макс\n• ПКМ: меню налаштувань");
            return 0;
        }

        case WM_DESTROY: {
            Logger::Info(L"App", L"EnergyControl shutting down");
            TelemetryThread::Stop();
            TrayManager::Destroy();
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Logger::Init();
    CrashHandler::Init();
    Logger::Info(L"App", L"EnergyControl process initialized — Version: " + AppConfig::GetAppVersion() + L" (" + AppConfig::GetAppStage() + L")");

    std::string cmdLine(lpCmdLine);

    if (cmdLine.find("--test-crash") != std::string::npos) {
        CrashHandler::TriggerSyntheticCrash();
        return 0;
    }

    if (cmdLine.find("--install-autostart") != std::string::npos) {
        bool ok = AppConfig::SetAutoStartEnabled(true);
        MessageBoxW(NULL, ok ? L"EnergyControl успішно додано в автозапуск з правами Адміністратора!\n\nПрограма з'явиться у Диспетчері завдань і стартуватиме у треї." 
                             : L"Не вдалося зареєструвати автозапуск. Перевірте дозволи UAC.", 
                    L"EnergyControl Setup", MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));
        return 0;
    }

    if (cmdLine.find("--uninstall-autostart") != std::string::npos) {
        bool ok = AppConfig::SetAutoStartEnabled(false);
        MessageBoxW(NULL, ok ? L"Автозапуск EnergyControl успішно вимкнено." 
                             : L"Не вдалося видалити налаштування автозапуску.", 
                    L"EnergyControl Setup", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // Single instance mutex
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"EnergyControlAppMutex_v2");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        Logger::Info(L"App", L"Existing instance detected, activating existing window");
        HWND hExisting = FindWindowW(L"EnergyControlWindowClass_v2", NULL);
        if (hExisting) {
            ShowWindow(hExisting, SW_SHOW);
            SetForegroundWindow(hExisting);
        }
        return 0;
    }

    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Register Window Class
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"EnergyControlWindowClass_v2";
    RegisterClassExW(&wc);

    // Center window on screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - WINDOW_WIDTH) / 2;
    int posY = (screenH - WINDOW_HEIGHT) / 2;

    HWND hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"EnergyControl",
        WS_POPUP, // Modern frameless window
        posX, posY, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) {
        GdiplusShutdown(gdiplusToken);
        return 1;
    }

    // Initialize Tray Icon
    TrayManager::Init(hWnd);

    // Initial tray icon state
    TelemetryData initialTd = TelemetryThread::GetData();
    TrayManager::UpdateIcon(initialTd.isEcoMode);

    // Check if launched with --minimized flag
    if (cmdLine.find("--minimized") != std::string::npos) {
        Logger::Info(L"App", L"Started in minimized mode (System Tray)");
        ShowWindow(hWnd, SW_HIDE);
    } else {
        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);
    }

    // Main Message Loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return (int)msg.wParam;
}