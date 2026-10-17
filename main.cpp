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
            return 1; // Запобігання мерехтінню завдяки подвійній буферизації

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            // Спочатку перевіряємо клік по інтерактивних кнопках інтерфейсу
            if (GuiWindow::HandleClick(hWnd, x, y)) {
                return 0;
            }

            // Дозволяємо перетягування вікна ЛИШЕ на вільній зоні заголовка (y < 46)
            if (y < 46) {
                ReleaseCapture();
                SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }

            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            GuiWindow::HandleMouseMove(hWnd, x, y);
            return 0;
        }

        case WM_USER + 2: { // Сповіщення від фонового потоку телеметрії про оновлення даних
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
                            TrayManager::ShowBalloon(L"EnergySavior", L"У вас встановлено найновішу версію!");
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
            // Згортання у трей замість повного закриття процесу
            ShowWindow(hWnd, SW_HIDE);
            TrayManager::ShowBalloon(L"EnergySavior у треї", 
                                     L"• ЛКМ по значку в треї: перемикає Еко / Макс\n• ПКМ: меню налаштувань");
            return 0;
        }

        case WM_DESTROY: {
            Logger::Info(L"Застосунок", L"Завершення роботи EnergySavior");
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
    Logger::Info(L"Застосунок", L"Процес EnergySavior ініціалізовано — Версія: " + AppConfig::GetAppVersion() + L" (" + AppConfig::GetAppStage() + L")");

    std::string cmdLine(lpCmdLine);

    if (cmdLine.find("--test-crash") != std::string::npos) {
        CrashHandler::TriggerSyntheticCrash();
        return 0;
    }

    if (cmdLine.find("--install-autostart") != std::string::npos) {
        bool ok = AppConfig::SetAutoStartEnabled(true);
        MessageBoxW(NULL, ok ? L"EnergySavior успішно додано в автозапуск з правами Адміністратора!\n\nПрограма стартуватиме разом із Windows у системному треї." 
                             : L"Не вдалося зареєструвати автозапуск. Перевірте дозволи UAC.", 
                    L"EnergySavior Налаштування", MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));
        return 0;
    }

    if (cmdLine.find("--uninstall-autostart") != std::string::npos) {
        bool ok = AppConfig::SetAutoStartEnabled(false);
        MessageBoxW(NULL, ok ? L"Автозапуск EnergySavior успішно вимкнено." 
                             : L"Не вдалося видалити налаштування автозапуску.", 
                    L"EnergySavior Налаштування", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // Запобігання запуску кількох екземплярів через системний м'ютекс
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"EnergySaviorAppMutex_v2");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        Logger::Info(L"Застосунок", L"Виявлено вже запущений екземпляр EnergySavior, активація наявного вікна");
        HWND hExisting = FindWindowW(L"EnergySaviorWindowClass_v2", NULL);
        if (!hExisting) hExisting = FindWindowW(L"EnergyControlWindowClass_v2", NULL);
        if (hExisting) {
            ShowWindow(hExisting, SW_SHOW);
            SetForegroundWindow(hExisting);
        }
        return 0;
    }

    // Ініціалізація підсистеми GDI+
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

    // Центрування вікна на робочому столі
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - WINDOW_WIDTH) / 2;
    int posY = (screenH - WINDOW_HEIGHT) / 2;

    HWND hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"EnergySavior",
        WS_POPUP, // Сучасне вікно без стандартних системних рамок Windows
        posX, posY, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) {
        GdiplusShutdown(gdiplusToken);
        return 1;
    }

    // Ініціалізація значка в системному треї
    TrayManager::Init(hWnd);

    // Початковий стан іконки відповідно до режиму
    TelemetryData initialTd = TelemetryThread::GetData();
    TrayManager::UpdateIcon(initialTd.isEcoMode);

    // Перевірка прапорця старту у згорнутому стані
    if (cmdLine.find("--minimized") != std::string::npos) {
        Logger::Info(L"Застосунок", L"Запуск у фоновому згорнутому режимі (Системний трей)");
        ShowWindow(hWnd, SW_HIDE);
    } else {
        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);
    }

    // Головний цикл обробки повідомлень Windows
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