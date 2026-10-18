#include "../../include/gui/GuiWindow.hpp"
#include "../../include/core/TelemetryThread.hpp"
#include "../../include/core/AppConfig.hpp"
#include "../../include/core/Logger.hpp"
#include "../../include/core/UpdateManager.hpp"
#include "../../include/gui/TrayManager.hpp"
#include "../../include/hardware/DisplayController.hpp"
#include "../../include/hardware/CpuController.hpp"
#include "../../include/hardware/LenovoWmi.hpp"
#include "../../include/hardware/WindowsPowerController.hpp"
#include "../../include/hardware/GpuController.hpp"
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

using namespace Gdiplus;

// Текст завжди залишається в межах своєї картки або колонки.
static void DrawLabel(Graphics& g, const std::wstring& text, const Font& font,
                      const RectF& bounds, const Brush& brush, bool wrap = false) {
    StringFormat format;
    format.SetTrimming(StringTrimmingEllipsisCharacter);
    if (!wrap) format.SetFormatFlags(StringFormatFlagsNoWrap);
    g.DrawString(text.c_str(), -1, &font, bounds, &format, &brush);
}

static std::wstring FormatWatts(float w) {
    wchar_t buf[32];
    swprintf_s(buf, L"%.1f W", w);
    return std::wstring(buf);
}

void GuiWindow::DrawRoundedRect(Graphics& g, const Rect& rc, int radius, const Brush& brush, const Pen* pen) {
    GraphicsPath path;
    if (rc.Width <= 0 || rc.Height <= 0) return;
    int d = (std::min)(radius * 2, (std::min)(rc.Width, rc.Height));
    path.AddArc(rc.X, rc.Y, d, d, 180, 90);
    path.AddArc(rc.X + rc.Width - d, rc.Y, d, d, 270, 90);
    path.AddArc(rc.X + rc.Width - d, rc.Y + rc.Height - d, d, d, 0, 90);
    path.AddArc(rc.X, rc.Y + rc.Height - d, d, d, 90, 90);
    path.CloseFigure();
    g.FillPath(&brush, &path);
    if (pen) {
        g.DrawPath(pen, &path);
    }
}

void GuiWindow::DrawCard(Graphics& g, int x, int y, int w, int h, const std::wstring& title) {
    Rect rc(x, y, w, h);
    SolidBrush bg(Color(255, 17, 24, 39));
    Pen border(Color(255, 33, 41, 54), 1.0f);
    DrawRoundedRect(g, rc, 8, bg, &border);

    if (!title.empty()) {
        Font fTitle(L"Segoe UI", 8.8f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
        SolidBrush bTitle(Color(255, 139, 148, 158));
        g.DrawString(title.c_str(), -1, &fTitle, PointF((REAL)x + 15, (REAL)y + 10), &bTitle);
    }
}

static void PromptTerminateProcess(HWND hWnd, DWORD pid, const std::wstring& name, unsigned long long memMB) {
    if (pid <= 4 || _wcsicmp(name.c_str(), L"dwm.exe") == 0 ||
        _wcsicmp(name.c_str(), L"explorer.exe") == 0 ||
        _wcsicmp(name.c_str(), L"EnergySavior.exe") == 0 ||
        _wcsicmp(name.c_str(), L"System") == 0 ||
        _wcsicmp(name.c_str(), L"csrss.exe") == 0) {
        MessageBoxW(hWnd, 
            (L"Процес \"" + name + L"\" (PID " + std::to_wstring(pid) + L") є захищеним системним компонентом Windows та не може бути завершений для запобігання збою ОС.").c_str(),
            L"Захист системи", MB_OK | MB_ICONWARNING);
        return;
    }

    std::wstring msg = L"Ви дійсно бажаєте примусово завершити цей процес?\n\n"
                       L"Процес:  " + name + L"\n"
                       L"PID:     " + std::to_wstring(pid) + L"\n"
                       L"Пам'ять: " + std::to_wstring(memMB) + L" MB\n\n"
                       L"Усі незбережені дані в цьому додатку буде втрачено.";
    int res = MessageBoxW(hWnd, msg.c_str(), L"Підтвердження завершення процесу", MB_YESNO | MB_ICONQUESTION);
    if (res == IDYES) {
        bool ok = SystemInfo::TerminateProcessByPid(pid);
        if (ok) {
            Logger::Info(L"Процеси", L"Користувач завершив процес: " + name + L" (PID " + std::to_wstring(pid) + L")");
            TelemetryThread::RefreshPowerStatus();
            InvalidateRect(hWnd, NULL, FALSE);
        } else {
            MessageBoxW(hWnd, L"Не вдалося завершити процес. Можливо, потрібні права адміністратора.", L"Помилка", MB_OK | MB_ICONERROR);
        }
    }
}

void GuiWindow::InitButtons() {
    s_buttons.clear();

    // =========================================================================
    // Persistent Header & System Buttons (tab = -1)
    // =========================================================================
    s_buttons.push_back({ 16, Rect(808, 12, 28, 26), L"—", false, false, Color(255, 100, 100, 100), Color(255, 40, 45, 55), Color(255, 22, 27, 34), -1 });
    s_buttons.push_back({ 17, Rect(842, 12, 28, 26), L"✕", false, false, Color(255, 255, 60, 60), Color(255, 80, 25, 25), Color(255, 22, 27, 34), -1 });
    s_buttons.push_back({ 21, Rect(650, 12, 145, 26), L"WIN SAVER: OFF", false, false, Color(255, 0, 230, 118), Color(40, 0, 230, 118), Color(30, 100, 110, 125), -1 });
    s_buttons.push_back({ 38, Rect(330, 680, 145, 28), L"[⚙] Налаштування", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), -1 });
    s_buttons.push_back({ 25, Rect(485, 680, 145, 28), L"[!] Баг-репорт", false, false, Color(255, 234, 179, 8), Color(255, 45, 38, 20), Color(255, 22, 27, 34), -1 });
    s_buttons.push_back({ 20, Rect(640, 680, 215, 28), L"[Оновлення]", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), -1 });

    // 5 Navigation Tabs (tab = -1): width = 160 each, gap = 7.5px
    s_buttons.push_back({ 100, Rect(25, 48, 160, 30), L"⚡ Головна", false, true, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), -1 });
    s_buttons.push_back({ 101, Rect(192, 48, 160, 30), L"◈ Графіки", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), -1 });
    s_buttons.push_back({ 105, Rect(359, 48, 160, 30), L"■ Процеси CPU", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), -1 });
    s_buttons.push_back({ 106, Rect(526, 48, 160, 30), L"◇ Процеси iGPU", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), -1 });
    s_buttons.push_back({ 102, Rect(693, 48, 162, 30), L"▲ Графіка dGPU", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), -1 });

    // =========================================================================
    // Dashboard Tab Buttons (tab = 0)
    // =========================================================================
    // Hero Mode Buttons: Savior (36), Eco (1), Custom (37), Max (19)
    s_buttons.push_back({ 36, Rect(485, 92, 175, 42), L"[ SAVIOR ]\nУльтра-Еко", false, false, Color(255, 16, 185, 129), Color(255, 15, 45, 30), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 1, Rect(670, 92, 175, 42), L"[ ECO ]\n60Hz + Quiet", false, false, Color(255, 0, 230, 118), Color(255, 20, 50, 35), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 37, Rect(485, 138, 175, 42), L"[ CUSTOM ]\nВласний", false, false, Color(255, 168, 85, 247), Color(255, 40, 25, 55), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 19, Rect(670, 138, 175, 42), L"[ MAX ]\n144Hz + Boost", false, false, Color(255, 0, 229, 255), Color(255, 20, 45, 60), Color(255, 22, 27, 34), 0 });

    // Quick links in Card 2
    s_buttons.push_back({ 22, Rect(655, 212, 190, 36), L"◈ Графіки  >", false, false, Color(255, 0, 229, 255), Color(255, 20, 45, 60), Color(255, 26, 32, 42), 0 });
    s_buttons.push_back({ 24, Rect(655, 256, 190, 36), L"■ Процеси CPU  >", false, false, Color(255, 0, 229, 255), Color(255, 20, 45, 60), Color(255, 26, 32, 42), 0 });

    // Refresh Rate
    s_buttons.push_back({ 2, Rect(30, 381, 195, 34), L"60 Hz (Енергозбереження)", false, false, Color(255, 0, 230, 118), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 3, Rect(235, 381, 195, 34), L"144 Hz (Плавність)", false, false, Color(255, 0, 229, 255), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });

    // CPU Boost
    s_buttons.push_back({ 4, Rect(450, 381, 195, 34), L"Boost OFF (~2.4 GHz)", false, false, Color(255, 0, 230, 118), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 5, Rect(655, 381, 195, 34), L"Boost ON (до 4.4 GHz)", false, false, Color(255, 0, 229, 255), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });

    // Thermal Profile: Quiet, Balance, Performance
    s_buttons.push_back({ 6, Rect(30, 439, 125, 34), L"Тихий", false, false, Color(255, 59, 130, 246), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 7, Rect(165, 439, 125, 34), L"Баланс", false, false, Color(255, 243, 244, 246), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 8, Rect(300, 439, 125, 34), L"Максимум", false, false, Color(255, 239, 68, 68), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });

    // MUX Switch
    s_buttons.push_back({ 9, Rect(450, 439, 125, 34), L"MUX: Hybrid", false, false, Color(255, 0, 229, 255), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 10, Rect(585, 439, 135, 34), L"MUX: iGPU Only", false, false, Color(255, 0, 230, 118), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 11, Rect(730, 439, 120, 34), L"MUX: dGPU", false, false, Color(255, 239, 68, 68), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });

    // Keyboard light & Config
    s_buttons.push_back({ 12, Rect(30, 503, 195, 34), L"Підсвітка клавіш", false, false, Color(255, 147, 51, 234), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 13, Rect(235, 503, 195, 34), L"Авто-перемикач AC/DC", false, false, Color(255, 0, 230, 118), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 14, Rect(450, 503, 205, 34), L"Автозапуск з Windows", false, false, Color(255, 0, 229, 255), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 18, Rect(665, 503, 185, 34), L"[!] Журнал подій (Лог)", false, false, Color(255, 234, 179, 8), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });

    // Dashboard Card 4: Open Devices & Processes View
    s_buttons.push_back({ 23, Rect(440, 582, 205, 42), L"▲ Графіка dGPU  >", false, false, Color(255, 0, 229, 255), Color(255, 25, 45, 60), Color(255, 26, 32, 42), 0 });
    s_buttons.push_back({ 15, Rect(655, 582, 195, 42), L"[X] Зупинити процеси dGPU", false, false, Color(255, 239, 68, 68), Color(255, 60, 20, 25), Color(255, 45, 15, 20), 0 });

    // =========================================================================
    // Graphs Tab Buttons (tab = 1)
    // =========================================================================
    s_buttons.push_back({ 103, Rect(25, 86, 180, 30), L"← Назад на головну", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 1 });
    s_buttons.push_back({ 26, Rect(570, 86, 65, 30), L"30 сек", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 1 });
    s_buttons.push_back({ 27, Rect(640, 86, 65, 30), L"1 хв", false, true, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 1 });
    s_buttons.push_back({ 28, Rect(710, 86, 65, 30), L"5 хв", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 1 });
    s_buttons.push_back({ 29, Rect(780, 86, 65, 30), L"15 хв", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 1 });

    // =========================================================================
    // CpuProcesses Tab Buttons (tab = 2)
    // =========================================================================
    s_buttons.push_back({ 107, Rect(25, 86, 180, 30), L"← Назад на головну", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 2 });
    s_buttons.push_back({ 31, Rect(475, 86, 160, 30), L"[↻] Оновити процеси", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 2 });
    s_buttons.push_back({ 34, Rect(645, 86, 210, 30), L"[X] Завершити процес...", false, false, Color(255, 239, 68, 68), Color(255, 60, 20, 25), Color(255, 45, 15, 20), 2 });

    // =========================================================================
    // IGpuProcesses Tab Buttons (tab = 3)
    // =========================================================================
    s_buttons.push_back({ 108, Rect(25, 86, 180, 30), L"← Назад на головну", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 3 });
    s_buttons.push_back({ 32, Rect(475, 86, 160, 30), L"[↻] Оновити процеси", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 3 });
    s_buttons.push_back({ 35, Rect(645, 86, 210, 30), L"[X] Завершити процес...", false, false, Color(255, 239, 68, 68), Color(255, 60, 20, 25), Color(255, 45, 15, 20), 3 });

    // =========================================================================
    // Devices / dGPU Tab Buttons (tab = 4)
    // =========================================================================
    s_buttons.push_back({ 104, Rect(25, 86, 180, 30), L"← Назад на головну", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 4 });
    s_buttons.push_back({ 33, Rect(475, 86, 160, 30), L"[↻] Оновити процеси", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 4 });
    s_buttons.push_back({ 15, Rect(645, 86, 210, 30), L"[X] Зупинити процеси dGPU", false, false, Color(255, 239, 68, 68), Color(255, 60, 20, 25), Color(255, 45, 15, 20), 4 });

    // =========================================================================
    // Settings Tab Buttons (tab = 5)
    // =========================================================================
    s_buttons.push_back({ 109, Rect(25, 86, 180, 30), L"← Назад на головну", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 5 });

    s_buttons.push_back({ 201, Rect(220, 160, 95, 30), L"60 Hz", false, false, Color(255, 0, 230, 118), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });
    s_buttons.push_back({ 202, Rect(325, 160, 95, 30), L"144 Hz", false, false, Color(255, 0, 229, 255), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });

    s_buttons.push_back({ 203, Rect(220, 200, 150, 30), L"Boost OFF (~2.4 GHz)", false, false, Color(255, 0, 230, 118), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });
    s_buttons.push_back({ 204, Rect(380, 200, 150, 30), L"Boost ON (до 4.4 GHz)", false, false, Color(255, 0, 229, 255), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });

    s_buttons.push_back({ 205, Rect(220, 240, 95, 30), L"Тихий", false, false, Color(255, 59, 130, 246), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });
    s_buttons.push_back({ 206, Rect(325, 240, 95, 30), L"Баланс", false, false, Color(255, 243, 244, 246), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });
    s_buttons.push_back({ 207, Rect(430, 240, 95, 30), L"Максимум", false, false, Color(255, 239, 68, 68), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });

    s_buttons.push_back({ 208, Rect(220, 280, 95, 30), L"Вимкнено", false, false, Color(255, 139, 148, 158), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });
    s_buttons.push_back({ 209, Rect(325, 280, 95, 30), L"Низька", false, false, Color(255, 147, 51, 234), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });
    s_buttons.push_back({ 210, Rect(430, 280, 95, 30), L"Висока", false, false, Color(255, 192, 132, 252), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });

    s_buttons.push_back({ 211, Rect(220, 320, 110, 30), L"Saver Увімк", false, false, Color(255, 0, 230, 118), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });
    s_buttons.push_back({ 212, Rect(340, 320, 110, 30), L"Saver Вимк", false, false, Color(255, 139, 148, 158), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });

    s_buttons.push_back({ 215, Rect(565, 200, 270, 72), L"[ АКТИВУВАТИ CUSTOM ]\nЗастосувати налаштування", false, false, Color(255, 168, 85, 247), Color(255, 45, 25, 60), Color(255, 22, 27, 34), 5 });

    s_buttons.push_back({ 221, Rect(35, 435, 800, 34), L"[↻]  Перемикати режими по колу (Savior → Eco → Max → Custom → Savior)", false, false, Color(255, 0, 229, 255), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });
    s_buttons.push_back({ 222, Rect(35, 477, 800, 34), L"[□]  Відкрити / Згорнути головне вікно EnergySavior", false, false, Color(255, 0, 229, 255), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });
    s_buttons.push_back({ 223, Rect(35, 519, 800, 34), L"[≡]  Відкрити контекстне меню системного трею", false, false, Color(255, 0, 229, 255), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 5 });
}

void GuiWindow::SetTab(GuiTab tab) {
    s_currentTab = tab;
    s_hoveredBtnId = -1;
    for (auto& button : s_buttons) button.isHovered = false;
    for (auto& btn : s_buttons) {
        if (btn.id == 100) btn.isActive = (s_currentTab == GuiTab::Dashboard);
        else if (btn.id == 101) btn.isActive = (s_currentTab == GuiTab::Graphs);
        else if (btn.id == 105) btn.isActive = (s_currentTab == GuiTab::CpuProcesses);
        else if (btn.id == 106) btn.isActive = (s_currentTab == GuiTab::IGpuProcesses);
        else if (btn.id == 102) btn.isActive = (s_currentTab == GuiTab::Devices);
        else if (btn.id == 38) btn.isActive = (s_currentTab == GuiTab::Settings);
    }
}

void GuiWindow::Paint(HWND hWnd, HDC hdc) {
    Paint(hWnd, hdc, TelemetryThread::GetData());
}

void GuiWindow::Paint(HWND hWnd, HDC hdc, const TelemetryData& td) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    if (w <= 0 || h <= 0) return;
    HDC bufferDc = CreateCompatibleDC(hdc);
    HBITMAP buffer = CreateCompatibleBitmap(hdc, w, h);
    if (!bufferDc || !buffer) {
        if (buffer) DeleteObject(buffer);
        if (bufferDc) DeleteDC(bufferDc);
        return;
    }
    HGDIOBJ previous = SelectObject(bufferDc, buffer);
    {
    Graphics g(bufferDc);
    g.SetPageUnit(UnitPixel);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Background
    SolidBrush bgBrush(Color(255, 13, 17, 23));
    g.FillRectangle(&bgBrush, 0, 0, w, h);

    const float scale = (std::min)(w / 880.0f, h / 720.0f);
    g.ScaleTransform(scale, scale);

    // 1. Header (Y: 10, H: 38)
    Font fApp(L"Segoe UI", 12.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    Font fSub(L"Segoe UI", 8.5f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);
    Font fStatus(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    SolidBrush textWhite(Color(255, 240, 246, 252));
    SolidBrush textMuted(Color(255, 139, 148, 158));
    SolidBrush textCyan(Color(255, 0, 229, 255));
    SolidBrush textGreen(Color(255, 0, 230, 118));
    SolidBrush textRed(Color(255, 255, 23, 68));

    g.DrawString(L"ENERGY SAVIOR", -1, &fApp, PointF(25, 10), &textWhite);
    std::wstring sysSub = td.system.cpuName + L"  |  " + (td.gpu.model.empty() ? L"Вбудована графіка" : td.gpu.model);
    DrawLabel(g, sysSub, fSub, RectF(25, 30, 605, 17), textMuted);

    // Dynamic button state updates
    for (auto& btn : s_buttons) {
        if (btn.id == 36) btn.isActive = (td.powerProfile == AppPowerProfile::Savior);
        else if (btn.id == 1) btn.isActive = (td.powerProfile == AppPowerProfile::Eco);
        else if (btn.id == 19) btn.isActive = (td.powerProfile == AppPowerProfile::Max);
        else if (btn.id == 37) btn.isActive = (td.powerProfile == AppPowerProfile::Custom);
        else if (btn.id == 38) btn.isActive = (s_currentTab == GuiTab::Settings);
        else if (btn.id == 2) btn.isActive = (td.refreshRate == 60);
        else if (btn.id == 3) btn.isActive = (td.refreshRate == 144);
        else if (btn.id == 4) btn.isActive = (td.cpuBoost == CpuBoostMode::Disabled);
        else if (btn.id == 5) btn.isActive = (td.cpuBoost == CpuBoostMode::Aggressive);
        else if (btn.id == 6) btn.isActive = (td.thermalMode == ThermalMode::Quiet);
        else if (btn.id == 7) btn.isActive = (td.thermalMode == ThermalMode::Balance);
        else if (btn.id == 8) btn.isActive = (td.thermalMode == ThermalMode::Performance);
        else if (btn.id == 9) btn.isActive = (td.muxMode == GpuMuxMode::Hybrid);
        else if (btn.id == 10) btn.isActive = (td.muxMode == GpuMuxMode::iGpuOnly);
        else if (btn.id == 11) btn.isActive = (td.muxMode == GpuMuxMode::dGpuOnly);
        else if (btn.id == 13) btn.isActive = AppConfig::IsAutoSwitchEnabled();
        else if (btn.id == 14) btn.isActive = AppConfig::IsAutoStartEnabled();
        else if (btn.id == 21) {
            btn.isActive = td.winPower.isEnergySaverActive;
            btn.text = td.winPower.isEnergySaverActive ? L"WIN SAVER: ON" : L"WIN SAVER: OFF";
        }
        else if (btn.id == 26) btn.isActive = (s_selectedGraphSeconds == 30);
        else if (btn.id == 27) btn.isActive = (s_selectedGraphSeconds == 60);
        else if (btn.id == 28) btn.isActive = (s_selectedGraphSeconds == 300);
        else if (btn.id == 29) btn.isActive = (s_selectedGraphSeconds == 900);
        else if (btn.id == 201) btn.isActive = (AppConfig::GetCustomRefreshRate() == 60);
        else if (btn.id == 202) btn.isActive = (AppConfig::GetCustomRefreshRate() == 144);
        else if (btn.id == 203) btn.isActive = (AppConfig::GetCustomCpuBoost() == 0);
        else if (btn.id == 204) btn.isActive = (AppConfig::GetCustomCpuBoost() != 0);
        else if (btn.id == 205) btn.isActive = (AppConfig::GetCustomThermalMode() == 0);
        else if (btn.id == 206) btn.isActive = (AppConfig::GetCustomThermalMode() == 1);
        else if (btn.id == 207) btn.isActive = (AppConfig::GetCustomThermalMode() == 2);
        else if (btn.id == 208) btn.isActive = (AppConfig::GetCustomKbdBacklight() == 0);
        else if (btn.id == 209) btn.isActive = (AppConfig::GetCustomKbdBacklight() == 1);
        else if (btn.id == 210) btn.isActive = (AppConfig::GetCustomKbdBacklight() == 2);
        else if (btn.id == 211) btn.isActive = AppConfig::GetCustomWinEnergySaver();
        else if (btn.id == 212) btn.isActive = !AppConfig::GetCustomWinEnergySaver();
        else if (btn.id == 221) btn.isActive = (AppConfig::GetTrayLeftClickAction() == TrayLeftClickAction::CycleProfiles);
        else if (btn.id == 222) btn.isActive = (AppConfig::GetTrayLeftClickAction() == TrayLeftClickAction::ToggleWindow);
        else if (btn.id == 223) btn.isActive = (AppConfig::GetTrayLeftClickAction() == TrayLeftClickAction::ShowMenu);
        else if (btn.id == 20) {
            UpdateStatus st = UpdateManager::GetStatus();
            if (st == UpdateStatus::UpdateAvailable) {
                btn.text = L"★ ОНОВИТИ: " + UpdateManager::GetInfo().latestVersion;
                btn.isActive = true;
            } else if (st == UpdateStatus::ReadyToRestart) {
                btn.text = L"★ ПЕРЕЗАПУСТИТИ ДЛЯ ОНОВЛЕННЯ";
                btn.isActive = true;
            } else if (st == UpdateStatus::Checking) {
                btn.text = L"Перевірка оновлень...";
                btn.isActive = false;
            } else if (st == UpdateStatus::Downloading) {
                btn.text = UpdateManager::GetStatusDisplayString();
                btn.isActive = true;
            } else {
                btn.text = L"[v" + AppConfig::GetAppVersion() + L" • Актуально]";
                btn.isActive = false;
            }
        }
        else if (btn.id == 12) {
            btn.text = L"Підсвітка: " + LenovoWmi::KbdToString(td.kbdLight);
        }
    }

    // Dispatch Tab Rendering
    switch (s_currentTab) {
        case GuiTab::Dashboard:
            PaintDashboard(g, td, w, h);
            break;
        case GuiTab::Graphs:
            PaintGraphs(g, td, w, h);
            break;
        case GuiTab::CpuProcesses:
            PaintCpuProcesses(g, td, w, h);
            break;
        case GuiTab::IGpuProcesses:
            PaintIGpuProcesses(g, td, w, h);
            break;
        case GuiTab::Devices:
            PaintDevices(g, td, w, h);
            break;
        case GuiTab::Settings:
            PaintSettings(g, td, w, h);
            break;
    }

    // Paint Buttons
    Font fBtn(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetTrimming(StringTrimmingEllipsisCharacter);

    for (const auto& btn : s_buttons) {
        if (btn.tab != -1 && btn.tab != (int)s_currentTab) continue;

        Color fillCol = btn.normalColor;
        Color textCol = Color(255, 240, 246, 252);
        Pen borderPen(Color(255, 48, 54, 61), 1.0f);

        if (btn.isActive) {
            fillCol = btn.activeColor;
            textCol = Color(255, 13, 17, 23);
        } else if (btn.isHovered) {
            fillCol = btn.hoverColor;
        }

        if (btn.id == 16 || btn.id == 17) {
            textCol = btn.activeColor;
            borderPen.SetColor(Color(255, 48, 54, 61));
        }

        SolidBrush btnBg(fillCol);
        SolidBrush btnTxt(textCol);

        DrawRoundedRect(g, btn.bounds, 6, btnBg, &borderPen);

        RectF rf((REAL)btn.bounds.X + 6, (REAL)btn.bounds.Y + 2, (REAL)btn.bounds.Width - 12, (REAL)btn.bounds.Height - 4);
        g.DrawString(btn.text.c_str(), -1, &fBtn, rf, &sf, &btnTxt);
    }

    // Footer banner
    Font fFooter(L"Segoe UI", 8.2f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);
    DrawLabel(g, L"EnergySavior • Моніторинг та оптимізація", fFooter, RectF(25, 686, 455, 18), textMuted);

    }
    BitBlt(hdc, 0, 0, w, h, bufferDc, 0, 0, SRCCOPY);
    SelectObject(bufferDc, previous);
    DeleteObject(buffer);
    DeleteDC(bufferDc);
}

void GuiWindow::PaintDashboard(Graphics& g, const TelemetryData& td, int /*w*/, int /*h*/) {
    SolidBrush textWhite(Color(255, 240, 246, 252));
    SolidBrush textMuted(Color(255, 139, 148, 158));
    SolidBrush textCyan(Color(255, 0, 229, 255));
    SolidBrush textGreen(Color(255, 0, 230, 118));
    SolidBrush textRed(Color(255, 255, 23, 68));

    // 1. Hero Profile Card (Y: 86, H: 104, W: 840)
    Rect heroRc(20, 86, 840, 104);
    Color heroBgCol = Color(255, 16, 32, 48);
    Color heroBorderCol = Color(255, 0, 229, 255);
    SolidBrush* titleBrush = &textCyan;
    std::wstring heroTitle = L"[MAX] МАКСИМАЛЬНА ПРОДУКТИВНІСТЬ";
    std::wstring heroSub = L"Екран 144 Hz • Boost увімкнено\nПрофіль для роботи та ігор.";

    SolidBrush textPurple(Color(255, 192, 132, 252));

    if (td.powerProfile == AppPowerProfile::Savior) {
        heroBgCol = Color(255, 8, 30, 20);
        heroBorderCol = Color(255, 16, 185, 129);
        titleBrush = &textGreen;
        heroTitle = L"[SAVIOR] МАКСИМАЛЬНЕ ЕНЕРГОЗБЕРЕЖЕННЯ";
        heroSub = L"Екстремальне енергозбереження (у рази економніше за Eco):\n60 Hz • Boost вимкнено • Мінімум ватів споживання.";
    } else if (td.powerProfile == AppPowerProfile::Eco) {
        heroBgCol = Color(255, 15, 35, 25);
        heroBorderCol = Color(255, 0, 230, 118);
        titleBrush = &textGreen;
        heroTitle = L"[ECO] ЕНЕРГОЗБЕРЕЖЕННЯ";
        heroSub = L"Екран 60 Hz • Boost вимкнено • Тихе охолодження\nПрофіль для тривалої автономної роботи.";
    } else if (td.powerProfile == AppPowerProfile::Custom) {
        heroBgCol = Color(255, 35, 20, 50);
        heroBorderCol = Color(255, 168, 85, 247);
        titleBrush = &textPurple;
        heroTitle = L"[CUSTOM] ВЛАСНИЙ ПРОФІЛЬ";
        heroSub = L"Індивідуальні налаштування з вкладки [⚙ Налаштування].\nНатисніть налаштування, щоб змінити параметри.";
    }

    SolidBrush heroBg(heroBgCol);
    Pen heroBorder(heroBorderCol, 1.5f);
    DrawRoundedRect(g, heroRc, 8, heroBg, &heroBorder);

    Font fHeroTitle(L"Segoe UI", 11.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    Font fHeroDesc(L"Segoe UI", 8.8f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);

    g.DrawString(heroTitle.c_str(), -1, &fHeroTitle, PointF(38, 98), titleBrush);
    DrawLabel(g, heroSub, fHeroDesc, RectF(38, 128, 435, 52), textWhite, true);

    // 2. Middle Row Cards (Y: 198, H: 125)
    DrawCard(g, 20, 198, 415, 125, L"ЗАРЯД ТА ЖИВЛЕННЯ");
    DrawCard(g, 445, 198, 415, 125, L"СПОЖИВАННЯ СИСТЕМИ");

    // Left card: Battery
    Font fBigNum(L"Segoe UI", 26.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    Font fSmallLbl(L"Segoe UI", 8.8f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);

    std::wstring batPctStr = std::to_wstring(td.battery.percent) + L"%";
    Color batCol = (td.battery.percent > 20) ? Color(255, 0, 230, 118) : Color(255, 239, 68, 68);
    SolidBrush bBat(batCol);
    g.DrawString(batPctStr.c_str(), -1, &fBigNum, PointF(35, 222), &bBat);

    Rect barBg(150, 240, 260, 14);
    SolidBrush bBarBg(Color(255, 33, 41, 54));
    DrawRoundedRect(g, barBg, 4, bBarBg);

    int fillW = (int)((std::clamp(td.battery.percent, 0, 100) / 100.0f) * 260);
    if (fillW > 0) {
        Rect barFill(150, 240, fillW, 14);
        SolidBrush bBarFill(Color(255, 0, 229, 255));
        DrawRoundedRect(g, barFill, 4, bBarFill);
    }

    std::wstringstream ssCap;
    ssCap << L"Ємність: " << td.battery.currentCapacityMWh << L" / " << td.battery.fullCapacityMWh << L" mWh ("
          << std::fixed << std::setprecision(1) << td.battery.healthPercent << L"% Здоров'я, " << td.battery.cycleCount << L" циклів)";
    DrawLabel(g, ssCap.str(), fSmallLbl, RectF(35, 274, 385, 18), textWhite);

    std::wstringstream ssVolt;
    ssVolt << L"Напруга: " << std::fixed << std::setprecision(2) << (td.battery.voltageMV / 1000.0) << L" V  |  Живлення: "
           << (td.battery.isAcOnline ? L"Підключено до 220V" : L"Робота від батареї (DC)");
    DrawLabel(g, ssVolt.str(), fSmallLbl, RectF(35, 294, 385, 18), textMuted);

    // Right card: Power Breakdown
    auto hist = TelemetryThread::GetPowerHistory(60);
    float curWatts = hist.empty() ? 0.0f : hist.back();
    std::wstring pwrStr = FormatWatts(curWatts);
    g.DrawString(pwrStr.c_str(), -1, &fBigNum, PointF(460, 222), &textCyan);

    std::wstring pwrSub = td.battery.isAcOnline ? L"Мережа 220V (Сумарне TDP)" : L"Розряд батареї (Датчик)";
    DrawLabel(g, pwrSub, fSmallLbl, RectF(460, 268, 185, 18), textGreen);

    std::wstring compSummary = L"Екран: " + FormatWatts(td.displayWatts) + L" (" + std::to_wstring(td.brightness) + L"%) | CPU: " + FormatWatts(td.cpuWatts);
    DrawLabel(g, compSummary, fSmallLbl, RectF(460, 298, 385, 18), textMuted);

    // 3. System Controls Card (Y: 331, H: 226, W: 840)
    DrawCard(g, 20, 331, 840, 226, L"КЕРУВАННЯ СИСТЕМОЮ ТА ПРИСТРОЯМИ");

    Font fGroup(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    g.DrawString(L"Частота екрану:", -1, &fGroup, PointF(30, 359), &textWhite);
    g.DrawString(L"Intel Turbo Boost (Процесор):", -1, &fGroup, PointF(450, 359), &textWhite);
    g.DrawString(L"Режим охолодження (TDP):", -1, &fGroup, PointF(30, 418), &textWhite);
    g.DrawString(L"Режим відеокарти (MUX Switch):", -1, &fGroup, PointF(450, 418), &textWhite);
    g.DrawString(L"Підсвітка, автоматизація та журнал:", -1, &fGroup, PointF(30, 482), &textWhite);

    // 4. NVIDIA Watchdog Card (Y: 565, H: 104, W: 840)
    DrawCard(g, 20, 565, 840, 104, L"NVIDIA GEFORCE RTX WATCHDOG МОНІТОРИНГ");

    Font fGpuName(L"Segoe UI", 10.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    Font fGpuStatus(L"Segoe UI", 8.8f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);

    std::wstring gpuHeader = td.gpu.model + L": ";
    if (td.gpu.isAwake) {
        gpuHeader += L"АКТИВНА (" + std::to_wstring((int)td.gpu.powerWatts) + L" W, " + std::to_wstring(td.gpu.temperatureC) + L" °C)";
        DrawLabel(g, gpuHeader, fGpuName, RectF(35, 600, 395, 21), textRed);

        std::wstring procList = L"[!] Процеси на dGPU (" + std::to_wstring(td.gpu.processes.size()) + L"): ";
        if (td.gpu.processes.empty()) {
            procList = L"[!] dGPU активна; доступних процесів не знайдено";
        } else {
            for (size_t i = 0; i < td.gpu.processes.size(); i++) {
                if (i > 0) procList += L",  ";
                procList += td.gpu.processes[i].name + L" (PID " + std::to_wstring(td.gpu.processes[i].pid) + L")";
                if (i >= 2 && td.gpu.processes.size() > 3) {
                    procList += L" (+ще " + std::to_wstring(td.gpu.processes.size() - 3) + L")";
                    break;
                }
            }
        }
        RectF rProc(35.0f, 626.0f, 395.0f, 36.0f);
        StringFormat sfWrap;
        sfWrap.SetTrimming(StringTrimmingEllipsisWord);
        g.DrawString(procList.c_str(), -1, &fGpuStatus, rProc, &sfWrap, &textWhite);
    } else {
        gpuHeader += L"СПИТЬ (D3Cold, 0 W — повне знеструмлення)";
        DrawLabel(g, gpuHeader, fGpuName, RectF(35, 600, 395, 21), textGreen);
        DrawLabel(g, L"Фонових процесів на dGPU не виявлено.", fGpuStatus, RectF(35, 626, 395, 36), textMuted, true);
    }
}

void GuiWindow::PaintGraphs(Graphics& g, const TelemetryData& td, int /*w*/, int /*h*/) {
    SolidBrush textWhite(Color(255, 240, 246, 252));
    SolidBrush textMuted(Color(255, 139, 148, 158));
    SolidBrush textCyan(Color(255, 0, 229, 255));
    SolidBrush textGreen(Color(255, 0, 230, 118));
    SolidBrush textRed(Color(255, 255, 23, 68));

    // 1. Graph Card (Y: 124, H: 255, W: 840)
    DrawCard(g, 20, 124, 840, 255, L"ГРАФІК СПОЖИВАННЯ ПОТУЖНОСТІ В ЧАСІ (WATT TIMELINE)");

    auto history = TelemetryThread::GetPowerHistory(s_selectedGraphSeconds);

    Rect gPlot(50, 160, 780, 180);
    SolidBrush plotBg(Color(255, 13, 17, 23));
    Pen plotBorder(Color(255, 33, 41, 54), 1.0f);
    g.FillRectangle(&plotBg, gPlot);
    g.DrawRectangle(&plotBorder, gPlot);

    float maxVal = 70.0f;
    for (float v : history) if (v > maxVal) maxVal = v;
    maxVal = std::ceil(maxVal / 10.0f) * 10.0f;
    Pen gridPen(Color(40, 255, 255, 255), 1.0f);
    Font fGrid(L"Segoe UI", 7.5f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);
    for (int p = 1; p <= 3; p++) {
        int y = gPlot.Y + (gPlot.Height * p / 4);
        g.DrawLine(&gridPen, gPlot.X, y, gPlot.X + gPlot.Width, y);
        float wattVal = maxVal * (1.0f - p / 4.0f);
        std::wstring wTxt = FormatWatts(wattVal);
        g.DrawString(wTxt.c_str(), -1, &fGrid, PointF((REAL)gPlot.X + 4, (REAL)y - 12), &textMuted);
    }

    if (history.size() >= 2) {
        std::vector<PointF> pts;
        float xStep = (float)gPlot.Width / (float)(history.size() - 1);
        for (size_t i = 0; i < history.size(); i++) {
            float val = history[i];
            float clamped = (val > maxVal) ? maxVal : (val < 0.0f ? 0.0f : val);
            float norm = clamped / maxVal;
            float px = gPlot.X + (i * xStep);
            float py = (gPlot.Y + gPlot.Height) - (norm * (float)gPlot.Height);
            pts.push_back(PointF(px, py));
        }

        GraphicsPath areaPath;
        areaPath.AddLine((REAL)gPlot.X, (REAL)(gPlot.Y + gPlot.Height), pts[0].X, pts[0].Y);
        for (size_t i = 1; i < pts.size(); i++) {
            areaPath.AddLine(pts[i - 1].X, pts[i - 1].Y, pts[i].X, pts[i].Y);
        }
        areaPath.AddLine(pts.back().X, pts.back().Y, (REAL)(gPlot.X + gPlot.Width), (REAL)(gPlot.Y + gPlot.Height));
        areaPath.CloseFigure();

        LinearGradientBrush areaBrush(
            PointF(0, (REAL)gPlot.Y),
            PointF(0, (REAL)(gPlot.Y + gPlot.Height)),
            Color(110, 0, 229, 255),
            Color(10, 0, 229, 255)
        );
        g.FillPath(&areaBrush, &areaPath);

        Pen linePen(Color(255, 0, 229, 255), 2.2f);
        linePen.SetLineJoin(LineJoinRound);
        for (size_t i = 1; i < pts.size(); i++) {
            g.DrawLine(&linePen, pts[i - 1], pts[i]);
        }

        PointF lastPt = pts.back();
        SolidBrush dotBrush(Color(255, 0, 230, 118));
        g.FillEllipse(&dotBrush, lastPt.X - 4.0f, lastPt.Y - 4.0f, 8.0f, 8.0f);
    }

    // Graph Stats Footer
    Font fStat(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    float curPwr = history.empty() ? 0.0f : history.back();
    float avgPwr = 0.0f;
    float minPwr = 999.0f, maxPwr = 0.0f;
    for (float v : history) {
        avgPwr += v;
        if (v < minPwr) minPwr = v;
        if (v > maxPwr) maxPwr = v;
    }
    if (!history.empty()) avgPwr /= history.size();
    else minPwr = 0.0f;

    std::wstring sCur = L"Поточне: " + FormatWatts(curPwr);
    std::wstring sAvg = L"Середнє (AVG): " + FormatWatts(avgPwr);
    std::wstring sMinMax = L"Мін / Макс: " + FormatWatts(minPwr) + L" / " + FormatWatts(maxPwr);

    g.DrawString(sCur.c_str(), -1, &fStat, PointF(50, 350), &textCyan);
    g.DrawString(sAvg.c_str(), -1, &fStat, PointF(260, 350), &textGreen);
    g.DrawString(sMinMax.c_str(), -1, &fStat, PointF(520, 350), &textWhite);

    // 2. Bottom Row Cards (Y: 388, H: 280)
    DrawCard(g, 20, 388, 415, 280, L"ДЕТАЛІ АКУМУЛЯТОРА");
    DrawCard(g, 445, 388, 415, 280, L"БАЛАНС ЕНЕРГОСПОЖИВАННЯ");

    Font fItemLabel(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);
    Font fItemVal(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);

    auto drawBatRow = [&](int y, const std::wstring& label, const std::wstring& val, const Brush& valBrush) {
        DrawLabel(g, label, fItemLabel, RectF(35, (REAL)y, 183, 22), textMuted);
        DrawLabel(g, val, fItemVal, RectF(230, (REAL)y, 190, 22), valBrush);
    };

    drawBatRow(418, L"Поточний рівень заряду:", std::to_wstring(td.battery.percent) + L"%", textGreen);
    drawBatRow(444, L"Заводська ємність:", std::to_wstring(td.battery.designCapacityMWh) + L" mWh", textWhite);
    drawBatRow(470, L"Повна ємність:", std::to_wstring(td.battery.fullCapacityMWh) + L" mWh", textWhite);
    drawBatRow(496, L"Здоров'я батареї:", std::to_wstring((int)td.battery.healthPercent) + L"% (Знос " + std::to_wstring((int)td.battery.wearPercent) + L"%)", textGreen);
    drawBatRow(522, L"Кількість циклів заряду:", std::to_wstring(td.battery.cycleCount), textWhite);
    drawBatRow(548, L"Швидкість розряду / заряду:", std::to_wstring(td.battery.dischargeRateMW) + L" mW", textCyan);

    std::wstringstream ssVolt;
    ssVolt << std::fixed << std::setprecision(2) << (td.battery.voltageMV / 1000.0) << L" V";
    drawBatRow(574, L"Напруга на клемах:", ssVolt.str(), textWhite);

    std::wstring winSaverTxt = td.winPower.isEnergySaverActive ? L"Увімкнено" : L"Вимкнено";
    drawBatRow(600, L"Енергозбереження Windows:", winSaverTxt, td.winPower.isEnergySaverActive ? textGreen : textMuted);

    std::wstring etaFull = td.battery.isAcOnline ? L"Живлення від мережі"
                         : td.battery.estimatedMinutes < 0 ? L"Немає даних"
                         : (std::to_wstring(td.battery.estimatedMinutes / 60) + L" год " + std::to_wstring(td.battery.estimatedMinutes % 60) + L" хв");
    drawBatRow(626, L"Розрахунковий час роботи:", etaFull, textCyan);

    // Right card: Components Power Split - Isolated columns to eliminate overlaps completely
    auto drawSplitRow = [&](int y, const std::wstring& comp, const std::wstring& watts, const std::wstring& desc, const Brush& b) {
        DrawLabel(g, comp, fItemVal, RectF(460, (REAL)y, 157, 22), textWhite);
        DrawLabel(g, watts, fItemVal, RectF(625, (REAL)y, 68, 22), b);
        DrawLabel(g, desc, fItemLabel, RectF(701, (REAL)y, 144, 22), textMuted);
    };

    std::wstring dispDesc = std::to_wstring(td.refreshRate) + L" Hz • " + std::to_wstring(td.brightness) + L"%";
    drawSplitRow(418, L"Екран (Дисплей):", FormatWatts(td.displayWatts), dispDesc, textCyan);

    std::wstring boostDesc = (td.cpuBoost == CpuBoostMode::Disabled) ? L"Boost вимкнено" : L"Boost агресивний";
    drawSplitRow(448, L"Процесор (CPU):", FormatWatts(td.cpuWatts), boostDesc, textCyan);

    std::wstring gpuWattsStr = td.gpu.isAwake ? FormatWatts(td.gpu.powerWatts) : L"0.0 W";
    std::wstring gpuDesc = td.gpu.isAwake ? (L"Активна (" + std::to_wstring(td.gpu.temperatureC) + L"°C)") : L"Спить (D3Cold)";
    drawSplitRow(478, L"Відеокарта (dGPU):", gpuWattsStr, gpuDesc, td.gpu.isAwake ? textRed : textGreen);

    drawSplitRow(508, L"Вбудована iGPU:", FormatWatts(td.iGpuWatts), td.iGpuName, textGreen);
    drawSplitRow(538, L"Системна плата / RAM:", L"~2.5 W", L"DDR5 + NVMe SSD", textMuted);

    drawSplitRow(574, L"Сумарне споживання:", FormatWatts(curPwr), (td.battery.isAcOnline ? L"Мережа 220V" : L"Батарея DC"), textGreen);

    // Quick tip at the bottom of the split
    Font fSubHint(L"Segoe UI", 8.2f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);
    DrawLabel(g, L"Деталі — у вкладках «Процеси CPU»\nта «Процеси iGPU».", fSubHint, RectF(460, 612, 385, 40), textCyan, true);
}

void GuiWindow::PaintCpuProcesses(Graphics& g, const TelemetryData& td, int /*w*/, int /*h*/) {
    SolidBrush textWhite(Color(255, 240, 246, 252));
    SolidBrush textMuted(Color(255, 139, 148, 158));
    SolidBrush textCyan(Color(255, 0, 229, 255));
    SolidBrush textGreen(Color(255, 0, 230, 118));
    SolidBrush textRed(Color(255, 255, 23, 68));
    SolidBrush textYellow(Color(255, 234, 179, 8));

    // Top Summary Banner (Y: 124, H: 65, W: 840)
    DrawCard(g, 20, 124, 840, 65, L"МОНІТОРИНГ НАВАНТАЖЕННЯ ПРОЦЕСОРА (CPU)");

    Font fHdrBold(L"Segoe UI", 9.8f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    Font fHdrReg(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);

    std::wstring cpuTitle = td.system.cpuName + L"  |  Загальне навантаження: " + std::to_wstring((int)td.cpuUsagePercent) + L"%";
    DrawLabel(g, cpuTitle, fHdrBold, RectF(35, 152, 810, 18), textCyan);

    std::wstring cpuSub = L"Орієнтовна потужність CPU: " + FormatWatts(td.cpuWatts) +
                          L"  |  Режим Boost: " + (td.cpuBoost == CpuBoostMode::Disabled ? L"Вимкнено (~2.4 GHz, Еко)" : L"Агресивний (до 4.4 GHz)");
    DrawLabel(g, cpuSub, fHdrReg, RectF(35, 171, 810, 17), textMuted);

    // Process Table Card (Y: 198, H: 470, W: 840)
    DrawCard(g, 20, 198, 840, 470, L"ТОП ЕНЕРГОЗАТРАТНИХ ПРОЦЕСІВ НА CPU");

    // Table Header
    Font fCol(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    g.DrawString(L"PID", -1, &fCol, PointF(35, 222), &textMuted);
    g.DrawString(L"Назва процесу", -1, &fCol, PointF(110, 222), &textMuted);
    g.DrawString(L"CPU, %", -1, &fCol, PointF(360, 222), &textMuted);
    g.DrawString(L"Пам'ять (RAM)", -1, &fCol, PointF(500, 222), &textMuted);
    g.DrawString(L"Оцінка, Вт", -1, &fCol, PointF(615, 222), &textMuted);
    g.DrawString(L"Категорія", -1, &fCol, PointF(710, 222), &textMuted);
    g.DrawString(L"Дія", -1, &fCol, PointF(815, 222), &textMuted);

    Pen linePen(Color(255, 33, 41, 54), 1.0f);
    g.DrawLine(&linePen, 35, 242, 845, 242);

    Font fRow(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);
    Font fRowBold(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);

    int rowY = 250;
    for (size_t i = 0; i < td.cpuProcesses.size() && i < 11; i++) {
        const auto& p = td.cpuProcesses[i];

        if (i % 2 == 1) {
            Rect rAlt(30, rowY - 2, 820, 32);
            SolidBrush bAlt(Color(12, 255, 255, 255));
            g.FillRectangle(&bAlt, rAlt);
        }

        g.DrawString(std::to_wstring(p.pid).c_str(), -1, &fRow, PointF(35, (REAL)rowY + 6), &textMuted);
        DrawLabel(g, p.name, fRowBold, RectF(110, (REAL)rowY + 6, 238, 22), textWhite);

        wchar_t cpuBuf[32];
        swprintf_s(cpuBuf, L"%.1f %%", p.cpuPercent);
        const Brush* bCpu = &textGreen;
        if (p.cpuPercent > 15.0f) bCpu = &textRed;
        else if (p.cpuPercent > 5.0f) bCpu = &textYellow;
        g.DrawString(cpuBuf, -1, &fRowBold, PointF(360, (REAL)rowY + 6), bCpu);

        std::wstring memStr = std::to_wstring(p.memMB) + L" MB";
        g.DrawString(memStr.c_str(), -1, &fRow, PointF(500, (REAL)rowY + 6), &textWhite);

        std::wstring wattStr = FormatWatts(p.estimatedWatts);
        g.DrawString(wattStr.c_str(), -1, &fRowBold, PointF(615, (REAL)rowY + 6), &textCyan);

        DrawLabel(g, p.role, fRow, RectF(710, (REAL)rowY + 6, 95, 22), textMuted);
        g.DrawString(L"[✕]", -1, &fRowBold, PointF(815, (REAL)rowY + 6), &textRed);

        rowY += 34;
    }

    if (td.cpuProcesses.empty()) {
        g.DrawString(L"Очікування вимірювання навантаження процесів...", -1, &fRow, PointF(35, 260), &textMuted);
    }
}

void GuiWindow::PaintIGpuProcesses(Graphics& g, const TelemetryData& td, int /*w*/, int /*h*/) {
    SolidBrush textWhite(Color(255, 240, 246, 252));
    SolidBrush textMuted(Color(255, 139, 148, 158));
    SolidBrush textCyan(Color(255, 0, 229, 255));
    SolidBrush textGreen(Color(255, 0, 230, 118));
    SolidBrush textRed(Color(255, 255, 23, 68));

    // Top Summary Banner (Y: 124, H: 65, W: 840)
    DrawCard(g, 20, 124, 840, 65, L"МОНІТОРИНГ ВБУДОВАНОЇ ГРАФІКИ (iGPU)");

    Font fHdrBold(L"Segoe UI", 9.8f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    Font fHdrReg(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);

    std::wstring iGpuTitle = td.iGpuName + L"  |  Віконні процеси Windows";
    DrawLabel(g, iGpuTitle, fHdrBold, RectF(35, 152, 810, 18), textGreen);

    std::wstring iGpuSub = L"Орієнтовний список: наявність вікна не підтверджує використання iGPU процесом.";
    DrawLabel(g, iGpuSub, fHdrReg, RectF(35, 171, 810, 17), textCyan);

    // Process Table Card (Y: 198, H: 470, W: 840)
    DrawCard(g, 20, 198, 840, 470, L"ДОДАТКИ ТА ПРОЦЕСИ НА ВБУДОВАНІЙ ВІДЕОКАРТІ (iGPU)");

    Font fCol(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    g.DrawString(L"PID", -1, &fCol, PointF(35, 222), &textMuted);
    g.DrawString(L"Додаток / Процес", -1, &fCol, PointF(110, 222), &textMuted);
    g.DrawString(L"Призначення", -1, &fCol, PointF(360, 222), &textMuted);
    g.DrawString(L"Пам'ять (RAM)", -1, &fCol, PointF(580, 222), &textMuted);
    g.DrawString(L"Визначення GPU", -1, &fCol, PointF(680, 222), &textMuted);
    g.DrawString(L"Дія", -1, &fCol, PointF(815, 222), &textMuted);

    Pen linePen(Color(255, 33, 41, 54), 1.0f);
    g.DrawLine(&linePen, 35, 242, 845, 242);

    Font fRow(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);
    Font fRowBold(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);

    int rowY = 250;
    for (size_t i = 0; i < td.iGpuProcesses.size() && i < 11; i++) {
        const auto& p = td.iGpuProcesses[i];

        if (i % 2 == 1) {
            Rect rAlt(30, rowY - 2, 820, 32);
            SolidBrush bAlt(Color(12, 255, 255, 255));
            g.FillRectangle(&bAlt, rAlt);
        }

        g.DrawString(std::to_wstring(p.pid).c_str(), -1, &fRow, PointF(35, (REAL)rowY + 6), &textMuted);
        DrawLabel(g, p.name, fRowBold, RectF(110, (REAL)rowY + 6, 238, 22), textWhite);
        DrawLabel(g, p.role, fRow, RectF(360, (REAL)rowY + 6, 208, 22), textCyan);

        std::wstring memStr = std::to_wstring(p.memMB) + L" MB";
        g.DrawString(memStr.c_str(), -1, &fRow, PointF(580, (REAL)rowY + 6), &textWhite);

        g.DrawString(L"Не підтверджено", -1, &fRowBold, PointF(680, (REAL)rowY + 6), &textGreen);
        g.DrawString(L"[✕]", -1, &fRowBold, PointF(815, (REAL)rowY + 6), &textRed);

        rowY += 34;
    }

    if (td.iGpuProcesses.empty()) {
        g.DrawString(L"Доступних віконних процесів не знайдено.", -1, &fRow, PointF(35, 260), &textMuted);
    }
}

void GuiWindow::PaintDevices(Graphics& g, const TelemetryData& td, int /*w*/, int /*h*/) {
    SolidBrush textWhite(Color(255, 240, 246, 252));
    SolidBrush textMuted(Color(255, 139, 148, 158));
    SolidBrush textCyan(Color(255, 0, 229, 255));
    SolidBrush textGreen(Color(255, 0, 230, 118));
    SolidBrush textRed(Color(255, 255, 23, 68));

    // 1. Card 1: NVIDIA RTX Watchdog Processes (X: 20, Y: 124, W: 840, H: 240)
    DrawCard(g, 20, 124, 840, 240, L"NVIDIA GEFORCE RTX WATCHDOG - ПРОЦЕСИ НА ВІДЕОКАРТІ");

    Font fStatusHeader(L"Segoe UI", 9.5f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    std::wstring gpuSummary = td.gpu.model + L": ";
    if (td.gpu.isAwake) {
        gpuSummary += L"АКТИВНА (" + std::to_wstring((int)td.gpu.powerWatts) + L" W, " + std::to_wstring(td.gpu.temperatureC) + L"°C) — Процесів на dGPU: " + std::to_wstring(td.gpu.processes.size());
        DrawLabel(g, gpuSummary, fStatusHeader, RectF(35, 152, 810, 22), textRed);
    } else {
        gpuSummary += L"СПИТЬ (D3Cold, 0 W — повне апаратне знеструмлення)";
        DrawLabel(g, gpuSummary, fStatusHeader, RectF(35, 152, 810, 22), textGreen);
    }

    if (td.gpu.processes.empty()) {
        Rect okBox(35, 180, 810, 160);
        SolidBrush bOkBg(Color(25, 0, 230, 118));
        Pen pOkBorder(Color(120, 0, 230, 118), 1.0f);
        DrawRoundedRect(g, okBox, 6, bOkBg, &pOkBorder);

        Font fOkTitle(L"Segoe UI", 11.0f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
        Font fOkDesc(L"Segoe UI", 9.0f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);
        g.DrawString(L"[✓] Жодного стороннього процесу на dGPU не виявлено!", -1, &fOkTitle, PointF(55, 210), &textGreen);
        DrawLabel(g, L"У поточному знімку немає доступних процесів dGPU.\nСтан і споживання відеокарти наведені вище.", fOkDesc, RectF(55, 240, 770, 70), textWhite, true);
    } else {
        Font fTableHeader(L"Segoe UI", 8.8f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
        g.DrawString(L"PID", -1, &fTableHeader, PointF(40, 180), &textMuted);
        g.DrawString(L"Назва процесу", -1, &fTableHeader, PointF(120, 180), &textMuted);
        g.DrawString(L"Пам'ять VRAM", -1, &fTableHeader, PointF(380, 180), &textMuted);
        g.DrawString(L"Вплив на батарею", -1, &fTableHeader, PointF(520, 180), &textMuted);
        g.DrawString(L"Дія", -1, &fTableHeader, PointF(805, 180), &textMuted);

        Font fRow(L"Segoe UI", 8.8f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);
        int yPos = 205;
        for (size_t i = 0; i < td.gpu.processes.size() && i < 4; i++) {
            const auto& p = td.gpu.processes[i];
            g.DrawString(std::to_wstring(p.pid).c_str(), -1, &fRow, PointF(40, (REAL)yPos), &textWhite);
            DrawLabel(g, p.name, fRow, RectF(120, (REAL)yPos, 248, 22), textWhite);
            g.DrawString((std::to_wstring(p.memoryMB) + L" MB").c_str(), -1, &fRow, PointF(380, (REAL)yPos), &textCyan);
            g.DrawString(L"Утримує dGPU активною (+35..50W)", -1, &fRow, PointF(520, (REAL)yPos), &textRed);
            g.DrawString(L"[✕]", -1, &fRow, PointF(805, (REAL)yPos), &textRed);
            yPos += 24;
        }

        if (td.gpu.processes.size() > 4) {
            std::wstring extra = L"... та ще " + std::to_wstring(td.gpu.processes.size() - 4) + L" процесів";
            g.DrawString(extra.c_str(), -1, &fRow, PointF(40, (REAL)yPos), &textMuted);
        }
    }

    // 2. Card 2: System Hardware Devices (X: 20, Y: 375, W: 840, H: 295)
    DrawCard(g, 20, 375, 840, 295, L"СИСТЕМНІ АПАРАТНІ ПРИСТРОЇ ТА МОДУЛІ");

    Font fDevTitle(L"Segoe UI", 9.2f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    Font fDevSub(L"Segoe UI", 8.5f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);

    auto drawDeviceBox = [&](int x, int y, int bw, int bh, const std::wstring& name, const std::wstring& stat, const std::wstring& sub, const Brush& statBrush) {
        Rect rb(x, y, bw, bh);
        SolidBrush bg(Color(255, 22, 27, 34));
        Pen border(Color(255, 33, 41, 54), 1.0f);
        DrawRoundedRect(g, rb, 6, bg, &border);

        DrawLabel(g, name, fDevTitle, RectF((REAL)x + 12, (REAL)y + 10, (REAL)bw - 24, 20), textWhite);
        DrawLabel(g, stat, fDevTitle, RectF((REAL)x + 12, (REAL)y + 30, (REAL)bw - 24, 20), statBrush);
        DrawLabel(g, sub, fDevSub, RectF((REAL)x + 12, (REAL)y + 50, (REAL)bw - 24, 20), textMuted);
    };

    // Row 1
    std::wstring dispSub = std::to_wstring(td.refreshRate) + L" Hz • " + std::to_wstring(td.brightness) + L"%";
    drawDeviceBox(35, 410, 260, 75, L"Дисплей ноутбука", (td.refreshRate == 60 ? L"60 Hz (Енергозбереження)" : L"144 Hz (Плавний)"), dispSub, textCyan);

    std::wstring cpuSub = (td.cpuBoost == CpuBoostMode::Disabled) ? L"Boost вимкнено (~2.4 GHz)" : L"Boost агресивний (до 4.4 GHz)";
    drawDeviceBox(310, 410, 260, 75, L"Процесор (CPU)", td.system.cpuName, cpuSub, (td.cpuBoost == CpuBoostMode::Disabled ? textGreen : textCyan));

    std::wstring gpuSub = td.gpu.isAwake ? (std::to_wstring((int)td.gpu.powerWatts) + L" W  |  " + std::to_wstring(td.gpu.temperatureC) + L"°C") : L"Повне знеструмлення (0 W)";
    drawDeviceBox(585, 410, 260, 75, L"Відеокарта (dGPU)", (td.gpu.isAwake ? L"Активна (Живлення подано)" : L"Спить (D3Cold)"), gpuSub, (td.gpu.isAwake ? textRed : textGreen));

    // Row 2
    std::wstring pwrSub = td.battery.isAcOnline ? L"Підключено до 220V" : (std::to_wstring(td.battery.dischargeRateMW) + L" mW розряд");
    drawDeviceBox(35, 495, 260, 75, L"Батарея та живлення", (std::to_wstring(td.battery.percent) + L"% (" + std::to_wstring((int)td.battery.healthPercent) + L"% здоров'я)"), pwrSub, textGreen);

    std::wstring ecTxt = td.system.manufacturer + L" " + td.system.model + L"  |  " + (td.system.isLenovo ? L"WMI EC Controller" : L"ACPI Standard Controller");
    drawDeviceBox(310, 495, 260, 75, (td.system.isLenovo ? L"Lenovo ACPI / WMI" : L"Universal ACPI Power"), L"Драйвер енергоконтролю активний", ecTxt, textCyan);

    std::wstring extTxt = td.system.hasExternalDisplay ? (std::to_wstring(td.system.externalDisplayCount) + L" зовн. дисплей(їв)") : L"Не підключено";
    drawDeviceBox(585, 495, 260, 75, L"Зовнішні монітори", (td.system.hasExternalDisplay ? L"Виявлено" : L"Лише екран лептопу"), extTxt, (td.system.hasExternalDisplay ? textCyan : textMuted));
}

void GuiWindow::PaintSettings(Graphics& g, const TelemetryData& /*td*/, int /*w*/, int /*h*/) {
    SolidBrush textWhite(Color(255, 240, 246, 252));
    SolidBrush textMuted(Color(255, 139, 148, 158));
    SolidBrush textCyan(Color(255, 0, 229, 255));
    SolidBrush textGreen(Color(255, 0, 230, 118));

    // Card 1: Custom Profile Settings (Y: 124, H: 245, W: 840)
    DrawCard(g, 20, 124, 840, 245, L"НАЛАШТУВАННЯ КОРИСТУВАЦЬКОГО ПРОФІЛЮ (CUSTOM MODE)");

    Font fHdr(L"Segoe UI", 9.2f * 4.0f / 3.0f, FontStyleBold, UnitPixel);
    Font fDesc(L"Segoe UI", 8.8f * 4.0f / 3.0f, FontStyleRegular, UnitPixel);

    g.DrawString(L"Частота екрану:", -1, &fHdr, PointF(35, 166), &textWhite);
    g.DrawString(L"Intel / AMD Boost:", -1, &fHdr, PointF(35, 206), &textWhite);
    g.DrawString(L"Охолодження (TDP):", -1, &fHdr, PointF(35, 246), &textWhite);
    g.DrawString(L"Підсвітка клавіш:", -1, &fHdr, PointF(35, 286), &textWhite);
    g.DrawString(L"Windows Saver:", -1, &fHdr, PointF(35, 326), &textWhite);

    DrawLabel(g, L"Ці параметри автоматично застосовуються при виборі режиму [CUSTOM] на головній панелі або через системний трей.\nНалаштуйте бажаний баланс і натисніть фіолетову кнопку для тесту.",
              fDesc, RectF(565, 285, 270, 75), textMuted, true);

    // Card 2: Tray Left Click Action (Y: 382, H: 185, W: 840)
    DrawCard(g, 20, 382, 840, 185, L"ДІЯ ПРИ ОДИНАРНОМУ КЛІКУ ЛКМ ПО ІКОНЦІ В ТРЕЇ");

    DrawLabel(g, L"Оберіть, яку дію виконувати при натисканні лівою кнопкою миші на іконку програми біля годинника:",
              fDesc, RectF(35, 412, 800, 20), textCyan);

    // Card 3: Savior Info (Y: 580, H: 85, W: 840)
    DrawCard(g, 20, 580, 840, 85, L"ПРОФІЛЬ SAVIOR (В РОЗРОБЦІ)");

    DrawLabel(g, L"Режим Savior розробляється як екстремальне енергозбереження: у рази економніше за звичайний Eco.\nБазовий профіль (60Hz + ліміт живлення) вже активний. Розширені твіки споживання готуються до наступного релізу.",
              fDesc, RectF(35, 608, 800, 48), textGreen, true);
}

bool GuiWindow::HandleClick(HWND hWnd, int x, int y) {
    RECT client;
    GetClientRect(hWnd, &client);
    const float scale = (std::min)(client.right / 880.0f, client.bottom / 720.0f);
    if (scale > 0) { x = (int)std::floor(x / scale); y = (int)std::floor(y / scale); }

    for (const auto& btn : s_buttons) {
        if (btn.tab != -1 && btn.tab != (int)s_currentTab) continue;
        if (btn.bounds.Contains(x, y)) {
            switch (btn.id) {
                case 100: SetTab(GuiTab::Dashboard); break;
                case 101: case 22: SetTab(GuiTab::Graphs); break;
                case 105: case 24: SetTab(GuiTab::CpuProcesses); break;
                case 106: SetTab(GuiTab::IGpuProcesses); break;
                case 102: case 23:
                    SetTab(GuiTab::Devices);
                    GpuController::RequestImmediateProbe();
                    break;
                case 38: SetTab(GuiTab::Settings); break;
                case 103: case 104: case 107: case 108: case 109: SetTab(GuiTab::Dashboard); break;
                case 36: TelemetryThread::ApplySaviorProfile(); break;
                case 1:  TelemetryThread::ApplyEcoProfile(); break;
                case 19: TelemetryThread::ApplyPerformanceProfile(); break;
                case 37: TelemetryThread::ApplyCustomProfile(); break;
                case 201: AppConfig::SetCustomRefreshRate(60); break;
                case 202: AppConfig::SetCustomRefreshRate(144); break;
                case 203: AppConfig::SetCustomCpuBoost(0); break;
                case 204: AppConfig::SetCustomCpuBoost(2); break;
                case 205: AppConfig::SetCustomThermalMode(0); break;
                case 206: AppConfig::SetCustomThermalMode(1); break;
                case 207: AppConfig::SetCustomThermalMode(2); break;
                case 208: AppConfig::SetCustomKbdBacklight(0); break;
                case 209: AppConfig::SetCustomKbdBacklight(1); break;
                case 210: AppConfig::SetCustomKbdBacklight(2); break;
                case 211: AppConfig::SetCustomWinEnergySaver(true); break;
                case 212: AppConfig::SetCustomWinEnergySaver(false); break;
                case 215: TelemetryThread::ApplyCustomProfile(); break;
                case 221: AppConfig::SetTrayLeftClickAction(TrayLeftClickAction::CycleProfiles); break;
                case 222: AppConfig::SetTrayLeftClickAction(TrayLeftClickAction::ToggleWindow); break;
                case 223: AppConfig::SetTrayLeftClickAction(TrayLeftClickAction::ShowMenu); break;
                case 31: case 32: case 33:
                    GpuController::RequestImmediateProbe();
                    TelemetryThread::RefreshPowerStatus();
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                case 25: {
                    int res = MessageBoxW(hWnd,
                        L"Бажаєте повідомити про баг або проблему на GitHub?\n\n"
                        L"Натисніть 'Так', щоб відкрити сторінку GitHub Issues та папку з логами програми.",
                        L"EnergySavior — Баг-репорт", MB_YESNO | MB_ICONINFORMATION);
                    if (res == IDYES) {
                        Logger::OpenLogFile();
                        ShellExecuteW(NULL, L"open", APP_ISSUES_URL, NULL, NULL, SW_SHOWNORMAL);
                    }
                    break;
                }
                case 34: {
                    TelemetryData td = TelemetryThread::GetData();
                    for (const auto& p : td.cpuProcesses) {
                        if (p.pid > 4 && _wcsicmp(p.name.c_str(), L"dwm.exe") != 0 &&
                            _wcsicmp(p.name.c_str(), L"explorer.exe") != 0 &&
                            _wcsicmp(p.name.c_str(), L"EnergySavior.exe") != 0) {
                            PromptTerminateProcess(hWnd, p.pid, p.name, p.memMB);
                            break;
                        }
                    }
                    break;
                }
                case 35: {
                    TelemetryData td = TelemetryThread::GetData();
                    for (const auto& p : td.iGpuProcesses) {
                        if (p.pid > 4 && _wcsicmp(p.name.c_str(), L"dwm.exe") != 0 &&
                            _wcsicmp(p.name.c_str(), L"explorer.exe") != 0 &&
                            _wcsicmp(p.name.c_str(), L"EnergySavior.exe") != 0) {
                            PromptTerminateProcess(hWnd, p.pid, p.name, p.memMB);
                            break;
                        }
                    }
                    break;
                }
                case 26: s_selectedGraphSeconds = 30; break;
                case 27: s_selectedGraphSeconds = 60; break;
                case 28: s_selectedGraphSeconds = 300; break;
                case 29: s_selectedGraphSeconds = 900; break;
                case 2:
                    DisplayController::SetRefreshRate(60);
                    TelemetryThread::SetRefreshRate(60);
                    break;
                case 3:
                    DisplayController::SetRefreshRate(144);
                    TelemetryThread::SetRefreshRate(144);
                    break;
                case 4:
                    CpuController::ApplyEcoProfile();
                    TelemetryThread::SetCpuBoost(CpuBoostMode::Disabled);
                    break;
                case 5:
                    CpuController::ApplyPerformanceProfile();
                    TelemetryThread::SetCpuBoost(CpuBoostMode::Aggressive);
                    break;
                case 6:
                    LenovoWmi::SetThermalMode(ThermalMode::Quiet);
                    TelemetryThread::SetThermalMode(ThermalMode::Quiet);
                    break;
                case 7:
                    LenovoWmi::SetThermalMode(ThermalMode::Balance);
                    TelemetryThread::SetThermalMode(ThermalMode::Balance);
                    break;
                case 8:
                    LenovoWmi::SetThermalMode(ThermalMode::Performance);
                    TelemetryThread::SetThermalMode(ThermalMode::Performance);
                    break;
                case 9:
                    LenovoWmi::SetMuxMode(GpuMuxMode::Hybrid);
                    TelemetryThread::SetMuxMode(GpuMuxMode::Hybrid);
                    Logger::Info(L"MUX", L"Перемкнуто в режим Hybrid");
                    break;
                case 10:
                    LenovoWmi::SetMuxMode(GpuMuxMode::iGpuOnly);
                    TelemetryThread::SetMuxMode(GpuMuxMode::iGpuOnly);
                    Logger::Info(L"MUX", L"Перемкнуто в режим iGPU Only");
                    break;
                case 11:
                    LenovoWmi::SetMuxMode(GpuMuxMode::dGpuOnly);
                    TelemetryThread::SetMuxMode(GpuMuxMode::dGpuOnly);
                    Logger::Info(L"MUX", L"Перемкнуто в режим dGPU Only");
                    break;
                case 12:
                    LenovoWmi::ToggleKeyboardLight();
                    TelemetryThread::SetKeyboardLight(LenovoWmi::GetKeyboardLight());
                    break;
                case 13: {
                    bool current = AppConfig::IsAutoSwitchEnabled();
                    AppConfig::SetAutoSwitchEnabled(!current);
                    Logger::Info(L"Config", std::wstring(L"Автоперемикач AC/DC: ") + (!current ? L"УВІМК" : L"ВИМК"));
                    break;
                }
                case 14: {
                    bool current = AppConfig::IsAutoStartEnabled();
                    AppConfig::SetAutoStartEnabled(!current);
                    Logger::Info(L"Config", std::wstring(L"Автозапуск з Windows: ") + (!current ? L"УВІМК" : L"ВИМК"));
                    break;
                }
                case 15:
                    Logger::Info(L"Watchdog", L"Користувач натиснув зупинку процесів dGPU");
                    GpuController::TerminateAllRogueProcesses();
                    break;
                case 16:
                    ShowWindow(hWnd, SW_HIDE);
                    break;
                case 17:
                    ShowWindow(hWnd, SW_HIDE);
                    break;
                case 18:
                    Logger::OpenLogFile();
                    break;
                case 21:
                    WindowsPowerController::ToggleEnergySaver();
                    TelemetryThread::RefreshPowerStatus();
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                case 20: {
                    UpdateStatus st = UpdateManager::GetStatus();
                    if (st == UpdateStatus::UpdateAvailable) {
                        int res = MessageBoxW(hWnd,
                            (L"Доступна нова версія EnergySavior: " + UpdateManager::GetInfo().latestVersion +
                             L"\n\nЗавантажити та встановити оновлення зараз?").c_str(),
                            L"EnergySavior Auto-Update", MB_YESNO | MB_ICONQUESTION);
                        if (res == IDYES) {
                            UpdateManager::DownloadAndInstallAsync([hWnd](bool ok, const std::wstring& msg) {
                                if (ok) {
                                    int restart = MessageBoxW(hWnd,
                                        L"Оновлення успішно завантажено!\nПерезапустити EnergySavior зараз для оновлення?",
                                        L"EnergySavior Auto-Update", MB_YESNO | MB_ICONINFORMATION);
                                    if (restart == IDYES) {
                                        UpdateManager::PerformSelfUpdateAndRestart(msg);
                                    }
                                } else {
                                    MessageBoxW(hWnd, msg.c_str(), L"Помилка оновлення", MB_OK | MB_ICONERROR);
                                }
                            });
                        }
                    } else if (st == UpdateStatus::ReadyToRestart) {
                        UpdateManager::PerformSelfUpdateAndRestart(L"");
                    } else if (st != UpdateStatus::Checking && st != UpdateStatus::Downloading) {
                        UpdateManager::CheckForUpdatesAsync([hWnd](UpdateStatus s, const UpdateInfo& /*info*/) {
                            if (s == UpdateStatus::UpToDate) {
                                TrayManager::ShowBalloon(L"EnergySavior", L"У вас встановлено найновішу версію!");
                            }
                        });
                    }
                    break;
                }
            }
            InvalidateRect(hWnd, NULL, FALSE);
            return true;
        }
    }

    // Process table row clicks on CPU (tab 2), iGPU (tab 3), or dGPU (tab 4)
    TelemetryData td = TelemetryThread::GetData();
    if (s_currentTab == GuiTab::CpuProcesses || s_currentTab == GuiTab::IGpuProcesses) {
        if (x >= 30 && x <= 850 && y >= 250 && y <= 624) {
            size_t rowIndex = (size_t)((y - 250) / 34);
            const auto& procs = (s_currentTab == GuiTab::CpuProcesses) ? td.cpuProcesses : td.iGpuProcesses;
            if (rowIndex < procs.size()) {
                PromptTerminateProcess(hWnd, procs[rowIndex].pid, procs[rowIndex].name, procs[rowIndex].memMB);
                return true;
            }
        }
    } else if (s_currentTab == GuiTab::Devices) {
        if (x >= 35 && x <= 845 && y >= 205 && y <= 305) {
            size_t rowIndex = (size_t)((y - 205) / 24);
            if (rowIndex < td.gpu.processes.size()) {
                const auto& p = td.gpu.processes[rowIndex];
                PromptTerminateProcess(hWnd, p.pid, p.name, p.memoryMB);
                return true;
            }
        }
    }

    return false;
}

void GuiWindow::HandleMouseMove(HWND hWnd, int x, int y) {
    RECT client;
    GetClientRect(hWnd, &client);
    const float scale = (std::min)(client.right / 880.0f, client.bottom / 720.0f);
    if (scale > 0) { x = (int)std::floor(x / scale); y = (int)std::floor(y / scale); }

    int newHover = -1;
    for (size_t i = 0; i < s_buttons.size(); i++) {
        if (s_buttons[i].tab != -1 && s_buttons[i].tab != (int)s_currentTab) continue;
        if (s_buttons[i].bounds.Contains(x, y)) {
            newHover = (int)i;
            break;
        }
    }
    if (newHover != s_hoveredBtnId) {
        s_hoveredBtnId = newHover;
        for (size_t i = 0; i < s_buttons.size(); i++) {
            s_buttons[i].isHovered = ((int)i == newHover);
        }
        InvalidateRect(hWnd, NULL, FALSE);
    }
}
