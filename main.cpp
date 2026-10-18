#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <windowsx.h>
#include <algorithm>
#include <gdiplus.h>
#include <string>

#include "include/core/AppConfig.hpp"
#include "include/core/Logger.hpp"
#include "include/core/CrashHandler.hpp"
#include "include/core/UpdateManager.hpp"
#include "include/core/TelemetryThread.hpp"
#include "include/gui/GuiWindow.hpp"
#include "include/gui/TrayManager.hpp"
#include "include/hardware/GpuController.hpp"

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
            return 1;

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            if (GuiWindow::HandleClick(hWnd, x, y)) {
                return 0;
            }

            // Drag window by clicking header (y < 46)
            RECT client;
            GetClientRect(hWnd, &client);
            float scale = (std::min)(client.right / 880.0f, client.bottom / 720.0f);
            if (y < 46 * scale) {
                ReleaseCapture();
                SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }

            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            TRACKMOUSEEVENT tracking = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tracking);
            GuiWindow::HandleMouseMove(hWnd, x, y);
            return 0;
        }

        case WM_MOUSELEAVE:
            GuiWindow::HandleMouseMove(hWnd, -1, -1);
            return 0;

        case WM_DPICHANGED: {
            const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(hWnd, NULL, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_USER + 2: { // Update from telemetry thread
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
                    UpdateManager::CheckForUpdatesAsync([hWnd](UpdateStatus s, const UpdateInfo& /*info*/) {
                        if (s == UpdateStatus::UpToDate) {
                            TrayManager::ShowBalloon(L"EnergySavior", L"У вас найновіша версія!");
                        }
                    });
                    break;
                }
                case ID_TRAY_OPEN_LOG:
                    Logger::OpenLogFile();
                    break;
                case ID_TRAY_REPORT_BUG:
                    Logger::OpenLogFile();
                    ShellExecuteW(NULL, L"open", APP_ISSUES_URL, NULL, NULL, SW_SHOWNORMAL);
                    break;
                case ID_TRAY_EXIT:
                    DestroyWindow(hWnd);
                    break;
            }
            return 0;
        }

        case WM_CLOSE: {
            // Minimize to tray instead of quitting
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        }

        case WM_DESTROY: {
            Logger::Info(L"Запуск", L"Завершення роботи EnergySavior");
            TelemetryThread::Stop();
            GpuController::Shutdown();
            TrayManager::Destroy();
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR lpCmdLine, int /*nCmdShow*/) {
    Logger::Init();
    CrashHandler::Init();

    // Clean up any stale registry entries from previous app names
    {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                          0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, L"EnergyControl");
            RegDeleteValueW(hKey, L"LenovoEnergyControl");
            RegCloseKey(hKey);
        }
    }
    Logger::Info(L"Запуск", L"Запуск EnergySavior - Версія: " + AppConfig::GetAppVersion() + L" (" + AppConfig::GetAppStage() + L")");

    std::string cmdLine(lpCmdLine);

    if (cmdLine.find("--test-crash") != std::string::npos) {
        CrashHandler::TriggerSyntheticCrash();
        return 0;
    }

    if (cmdLine.find("--install-autostart") != std::string::npos) {
        bool ok = AppConfig::SetAutoStartEnabled(true);
        MessageBoxW(NULL, ok ? L"EnergySavior успішно додано в автозапуск!\n\nПрограма запускатиметься разом з Windows у системний трей." 
                             : L"Не вдалося налаштувати автозапуск. Перевірте права адміністратора.", 
                    L"EnergySavior Автозапуск", MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));
        return 0;
    }

    if (cmdLine.find("--uninstall-autostart") != std::string::npos) {
        bool ok = AppConfig::SetAutoStartEnabled(false);
        MessageBoxW(NULL, ok ? L"Автозапуск EnergySavior успішно вимкнено." 
                             : L"Не вдалося вимкнути автозапуск.", 
                    L"EnergySavior Автозапуск", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // Запобігання запуску декількох копій програми
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"EnergySaviorAppMutex_v2");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        Logger::Info(L"Запуск", L"Виявлено інший екземпляр EnergySavior, активую наявне вікно");
        HWND hExisting = FindWindowW(L"EnergySaviorWindowClass_v2", NULL);
        if (!hExisting) hExisting = FindWindowW(L"EnergyControlWindowClass_v2", NULL);
        if (hExisting) {
            ShowWindow(hExisting, SW_SHOW);
            SetForegroundWindow(hExisting);
        }
        return 0;
    }

    // Ініціалізація GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Реєстрація віконного класу
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"EnergySaviorWindowClass_v2";
    RegisterClassExW(&wc);

    // Масштабуємо вікно відповідно до DPI та доступної робочої області.
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    HDC screen = GetDC(NULL);
    float scale = GetDeviceCaps(screen, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, screen);
    scale = (std::min)(scale, (std::min)((workArea.right - workArea.left) / 880.0f,
                                       (workArea.bottom - workArea.top) / 720.0f));
    int windowWidth = (int)(WINDOW_WIDTH * scale);
    int windowHeight = (int)(WINDOW_HEIGHT * scale);
    int posX = workArea.left + (workArea.right - workArea.left - windowWidth) / 2;
    int posY = workArea.top + (workArea.bottom - workArea.top - windowHeight) / 2;

    HWND hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"EnergySavior",
        WS_POPUP,
        posX, posY, windowWidth, windowHeight,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) {
        GdiplusShutdown(gdiplusToken);
        return 1;
    }

    // Ініціалізація іконки в системному треї
    TrayManager::Init(hWnd);

    // Початковий стан іконки
    TelemetryData initialTd = TelemetryThread::GetData();
    TrayManager::UpdateIcon(initialTd.isEcoMode);

    // Перевірка запуску у фоні
    if (cmdLine.find("--minimized") != std::string::npos) {
        Logger::Info(L"Запуск", L"Запуск у фоновому режимі (системний трей)");
        ShowWindow(hWnd, SW_HIDE);
    } else {
        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);
    }

    // Головний цикл обробки повідомлень
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