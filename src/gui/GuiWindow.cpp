#include "../../include/gui/GuiWindow.hpp"
#include "../../include/core/TelemetryThread.hpp"
#include "../../include/core/AppConfig.hpp"
#include "../../include/core/Logger.hpp"
#include "../../include/core/UpdateManager.hpp"
#include "../../include/gui/TrayManager.hpp"
#include <iomanip>
#include <sstream>
#include <cmath>

using namespace Gdiplus;

static std::vector<float> s_powerHistory;

void GuiWindow::DrawRoundedRect(Graphics& g, const Rect& rc, int radius, const Brush& brush, const Pen* pen) {
    GraphicsPath path;
    int d = radius * 2;
    path.AddArc(rc.X, rc.Y, d, d, 180, 90);
    path.AddArc(rc.X + rc.Width - d, rc.Y, d, d, 270, 90);
    path.AddArc(rc.X + rc.Width - d, rc.Y + rc.Height - d, d, d, 0, 90);
    path.AddArc(rc.X, rc.Y + rc.Height - d, d, d, 90, 90);
    path.CloseFigure();

    g.FillPath(&brush, &path);
    if (pen) g.DrawPath(pen, &path);
}

void GuiWindow::DrawCard(Graphics& g, int x, int y, int w, int h, const std::wstring& title) {
    Rect rc(x, y, w, h);
    SolidBrush bgBrush(Color(255, 22, 27, 34)); // #161b22
    Pen borderPen(Color(255, 48, 54, 61), 1.0f); // #30363d
    DrawRoundedRect(g, rc, 8, bgBrush, &borderPen);

    if (!title.empty()) {
        Font font(L"Segoe UI", 9.2f, FontStyleBold);
        SolidBrush titleBrush(Color(255, 139, 148, 158));
        g.DrawString(title.c_str(), -1, &font, PointF((REAL)x + 16, (REAL)y + 10), &titleBrush);
    }
}

void GuiWindow::InitButtons() {
    s_buttons.clear();

    // =========================================================================
    // Persistent Header & System Buttons (tab = -1)
    // =========================================================================
    // Titlebar Buttons: Minimize (ID 16), Close (ID 17)
    s_buttons.push_back({ 16, Rect(808, 12, 28, 26), L"—", false, false, Color(255, 100, 100, 100), Color(255, 40, 45, 55), Color(255, 22, 27, 34), -1 });
    s_buttons.push_back({ 17, Rect(842, 12, 28, 26), L"✕", false, false, Color(255, 255, 60, 60), Color(255, 80, 25, 25), Color(255, 22, 27, 34), -1 });

    // Windows Battery Saver Toggle Button (ID 21) in header
    s_buttons.push_back({ 21, Rect(650, 12, 145, 26), L"WIN SAVER: OFF", false, false, Color(255, 0, 230, 118), Color(40, 0, 230, 118), Color(30, 100, 110, 125), -1 });

    // In-App Update Button (ID 20) in bottom footer
    s_buttons.push_back({ 20, Rect(630, 680, 225, 28), L"[🔄 Оновлення]", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), -1 });

    // Navigation Tabs (tab = -1)
    s_buttons.push_back({ 100, Rect(25, 48, 130, 30), L"⚡ Головна", false, true, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), -1 });
    s_buttons.push_back({ 101, Rect(165, 48, 210, 30), L"📈 Графіки та моніторинг", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), -1 });
    s_buttons.push_back({ 102, Rect(385, 48, 210, 30), L"💻 Процеси та пристрої", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), -1 });

    // =========================================================================
    // Dashboard Tab Buttons (tab = 0)
    // =========================================================================
    // 1. Hero Mode Buttons (ID 1: Eco, ID 19: Perf)
    s_buttons.push_back({ 1, Rect(585, 92, 125, 88), L"[ ECO ]\n60Hz + Quiet", false, false, Color(255, 0, 230, 118), Color(255, 20, 50, 35), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 19, Rect(720, 92, 125, 88), L"[ MAX ]\n144Hz + Boost", false, false, Color(255, 0, 229, 255), Color(255, 20, 45, 60), Color(255, 22, 27, 34), 0 });

    // 2. Open Graphs button inside Card 3 (ID 22)
    s_buttons.push_back({ 22, Rect(655, 224, 190, 48), L"📈 Графіки ват →", false, false, Color(255, 0, 229, 255), Color(255, 20, 45, 60), Color(255, 26, 32, 42), 0 });

    // 3. Refresh Rate 60 Hz (ID 2), 144 Hz (ID 3)
    s_buttons.push_back({ 2, Rect(30, 375, 195, 34), L"60 Hz (Енергозбереження)", false, false, Color(255, 0, 230, 118), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 3, Rect(235, 375, 195, 34), L"144 Hz (Макс. плавність)", false, false, Color(255, 0, 229, 255), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });

    // 4. CPU Turbo Boost: Off (ID 4), Aggressive (ID 5)
    s_buttons.push_back({ 4, Rect(450, 375, 195, 34), L"Boost OFF (~2.4 GHz)", false, false, Color(255, 0, 230, 118), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 5, Rect(655, 375, 195, 34), L"Boost ON (до 4.4 GHz)", false, false, Color(255, 0, 229, 255), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });

    // 5. Lenovo Thermal Mode: Quiet (ID 6), Balance (ID 7), Performance (ID 8)
    s_buttons.push_back({ 6, Rect(30, 439, 125, 34), L"Quiet (Fn+Q)", false, false, Color(255, 59, 130, 246), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 7, Rect(165, 439, 125, 34), L"Balance", false, false, Color(255, 243, 244, 246), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 8, Rect(300, 439, 125, 34), L"Performance", false, false, Color(255, 239, 68, 68), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });

    // 6. MUX Switch / iGPU Mode: Hybrid (ID 9), iGPU Only (ID 10), dGPU Only (ID 11)
    s_buttons.push_back({ 9, Rect(450, 439, 125, 34), L"MUX: Hybrid", false, false, Color(255, 0, 229, 255), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 10, Rect(585, 439, 135, 34), L"MUX: iGPU Only", false, false, Color(255, 0, 230, 118), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 11, Rect(730, 439, 120, 34), L"MUX: dGPU", false, false, Color(255, 239, 68, 68), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });

    // 7. Keyboard Light (ID 12)
    s_buttons.push_back({ 12, Rect(30, 503, 195, 34), L"Підсвітка клавіатури", false, false, Color(255, 147, 51, 234), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });

    // 8. Auto-switch (ID 13) & Auto-start (ID 14)
    s_buttons.push_back({ 13, Rect(235, 503, 195, 34), L"Авто-перемикач AC/DC", false, false, Color(255, 0, 230, 118), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });
    s_buttons.push_back({ 14, Rect(450, 503, 205, 34), L"Автозапуск з Windows", false, false, Color(255, 0, 229, 255), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });

    // 9. Open Log File (ID 18)
    s_buttons.push_back({ 18, Rect(665, 503, 185, 34), L"[!] Журнал подій (Лог)", false, false, Color(255, 234, 179, 8), Color(255, 33, 41, 54), Color(255, 22, 27, 34), 0 });

    // 10. Dashboard Card 5: Open Devices & Processes View (ID 23)
    s_buttons.push_back({ 23, Rect(440, 582, 215, 42), L"💻 Всі процеси та девайси →", false, false, Color(255, 0, 229, 255), Color(255, 25, 45, 60), Color(255, 26, 32, 42), 0 });

    // 11. Kill dGPU apps (ID 15)
    s_buttons.push_back({ 15, Rect(665, 582, 185, 42), L"[X] Закрити софт на dGPU", false, false, Color(255, 239, 68, 68), Color(255, 60, 20, 25), Color(255, 45, 15, 20), 0 });

    // =========================================================================
    // Graphs Tab Buttons (tab = 1)
    // =========================================================================
    s_buttons.push_back({ 103, Rect(25, 86, 210, 30), L"← Повернутися на головну", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 1 });

    // =========================================================================
    // Devices Tab Buttons (tab = 2)
    // =========================================================================
    s_buttons.push_back({ 104, Rect(25, 86, 210, 30), L"← Повернутися на головну", false, false, Color(255, 0, 229, 255), Color(255, 30, 45, 60), Color(255, 22, 27, 34), 2 });
    s_buttons.push_back({ 15, Rect(575, 86, 275, 30), L"🛑 Закрити всі процеси на dGPU", false, false, Color(255, 239, 68, 68), Color(255, 60, 20, 25), Color(255, 45, 15, 20), 2 });
}

void GuiWindow::SetTab(GuiTab tab) {
    s_currentTab = tab;
    for (auto& btn : s_buttons) {
        if (btn.id == 100) btn.isActive = (s_currentTab == GuiTab::Dashboard);
        if (btn.id == 101) btn.isActive = (s_currentTab == GuiTab::Graphs);
        if (btn.id == 102) btn.isActive = (s_currentTab == GuiTab::Devices);
    }
}

void GuiWindow::Paint(HWND hWnd, HDC hdc) {
    RECT clientRc;
    GetClientRect(hWnd, &clientRc);
    int w = clientRc.right - clientRc.left;
    int h = clientRc.bottom - clientRc.top;

    TelemetryData td = TelemetryThread::GetData();

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

    Graphics g(hdcMem);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // 1. Background
    SolidBrush bg(Color(255, 13, 17, 23));
    g.FillRectangle(&bg, 0, 0, w, h);

    // 2. Universal Header
    Font fontTitle(L"Segoe UI", 13.0f, FontStyleBold);
    Font fontSub(L"Segoe UI", 8.5f, FontStyleRegular);
    SolidBrush textWhite(Color(255, 240, 246, 252));
    SolidBrush textMuted(Color(255, 139, 148, 158));

    std::wstring appTitle = L"ENERGY CONTROL";
    g.DrawString(appTitle.c_str(), -1, &fontTitle, PointF(25, 12), &textWhite);

    std::wstring sysSub = L"BIOS: " + td.system.biosVersion + L"  |  " + td.system.cpuName;
    if (td.system.hasExternalDisplay) {
        sysSub += L"  |  [Зовнішній дисплей: " + std::to_wstring(td.system.externalDisplayCount) + L"]";
    }
    g.DrawString(sysSub.c_str(), -1, &fontSub, PointF(25, 34), &textMuted);

    // Badges in Header
    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);
    Font fBadge(L"Segoe UI", 8.2f, FontStyleBold);

    Rect badgeAdmin(385, 12, 125, 24);
    SolidBrush bAdminBg(td.isElevated ? Color(40, 0, 230, 118) : Color(40, 234, 179, 8));
    Pen pAdmin(td.isElevated ? Color(180, 0, 230, 118) : Color(180, 234, 179, 8), 1.0f);
    DrawRoundedRect(g, badgeAdmin, 4, bAdminBg, &pAdmin);
    SolidBrush bAdminText(td.isElevated ? Color(255, 0, 230, 118) : Color(255, 234, 179, 8));
    g.DrawString(td.isElevated ? L"АДМІНІСТРАТОР" : L"ЗВИЧАЙНИЙ ЮЗЕР", -1, &fBadge, RectF(385, 12, 125, 24), &sfCenter, &bAdminText);

    Rect badgePower(515, 12, 125, 24);
    SolidBrush bPwrBg(td.battery.isAcOnline ? Color(40, 0, 229, 255) : Color(40, 249, 115, 22));
    Pen pPwr(td.battery.isAcOnline ? Color(180, 0, 229, 255) : Color(180, 249, 115, 22), 1.0f);
    DrawRoundedRect(g, badgePower, 4, bPwrBg, &pPwr);
    SolidBrush bPwrText(td.battery.isAcOnline ? Color(255, 0, 229, 255) : Color(255, 249, 115, 22));
    g.DrawString(td.battery.isAcOnline ? L"МЕРЕЖА (220V)" : L"БАТАРЕЯ (DC)", -1, &fBadge, RectF(515, 12, 125, 24), &sfCenter, &bPwrText);

    // Update Live Power History Buffer (60 historical points)
    float activeWatts = 0.0f;
    float gpuWatts = td.gpu.powerWatts > 0.0f ? td.gpu.powerWatts : 0.0f;
    float baseWatts = (td.refreshRate == 144 ? 11.5f : 7.5f);
    float cpuWatts = (td.cpuBoost == CpuBoostMode::Disabled ? 11.0f : 22.0f);
    if (td.gpu.isAwake || !td.gpu.processes.empty()) cpuWatts += 8.0f;

    if (td.battery.isDischarging && td.battery.dischargeRateMW > 0) {
        activeWatts = td.battery.dischargeRateMW / 1000.0f;
    } else if (td.battery.isCharging && td.battery.dischargeRateMW > 0) {
        activeWatts = td.battery.dischargeRateMW / 1000.0f;
    } else {
        activeWatts = cpuWatts + gpuWatts + baseWatts;
    }

    if (s_powerHistory.empty()) {
        s_powerHistory.assign(60, activeWatts);
    } else {
        s_powerHistory.push_back(activeWatts);
        if (s_powerHistory.size() > 60) {
            s_powerHistory.erase(s_powerHistory.begin());
        }
    }

    // 3. Update dynamic button states
    bool autoSw = AppConfig::IsAutoSwitchEnabled();
    bool autoSt = AppConfig::IsAutoStartEnabled();

    for (auto& btn : s_buttons) {
        if (btn.id == 100) btn.isActive = (s_currentTab == GuiTab::Dashboard);
        else if (btn.id == 101) btn.isActive = (s_currentTab == GuiTab::Graphs);
        else if (btn.id == 102) btn.isActive = (s_currentTab == GuiTab::Devices);
        else if (btn.id == 1) btn.isActive = td.isEcoMode;
        else if (btn.id == 19) btn.isActive = !td.isEcoMode;
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
        else if (btn.id == 12) {
            btn.text = L"Підсвітка: " + LenovoWmi::KbdToString(td.kbdLight);
            btn.isActive = (td.kbdLight != KbdBacklight::Off);
        }
        else if (btn.id == 13) {
            btn.isActive = autoSw;
            btn.text = autoSw ? L"[V] Авто-перемикач AC/DC" : L"[ ] Авто-перемикач AC/DC";
        }
        else if (btn.id == 14) {
            btn.isActive = autoSt;
            btn.text = autoSt ? L"[V] Автозапуск з Windows" : L"[ ] Автозапуск з Windows";
        }
        else if (btn.id == 18) {
            btn.text = L"[!] Журнал подій (Лог)";
            btn.isActive = false;
        }
        else if (btn.id == 20) {
            btn.text = UpdateManager::GetStatusDisplayString();
            UpdateStatus st = UpdateManager::GetStatus();
            if (st == UpdateStatus::UpdateAvailable || st == UpdateStatus::ReadyToRestart) {
                btn.isActive = true;
                btn.activeColor = Color(255, 0, 230, 118);
            } else if (st == UpdateStatus::Checking || st == UpdateStatus::Downloading) {
                btn.isActive = true;
                btn.activeColor = Color(255, 0, 229, 255);
            } else {
                btn.isActive = false;
            }
        }
        else if (btn.id == 21) {
            bool isWinSaver = td.winPower.isEnergySaverActive;
            btn.isActive = isWinSaver;
            btn.text = isWinSaver ? L"WIN SAVER: ON" : L"WIN SAVER: OFF";
            btn.activeColor = isWinSaver ? Color(255, 0, 230, 118) : Color(255, 139, 148, 158);
        }
    }

    // 4. Render Active Tab Content
    if (s_currentTab == GuiTab::Dashboard) {
        PaintDashboard(g, td, w, h);
    } else if (s_currentTab == GuiTab::Graphs) {
        PaintGraphs(g, td, w, h);
    } else if (s_currentTab == GuiTab::Devices) {
        PaintDevices(g, td, w, h);
    }

    // 5. Draw All Visible Buttons for the current tab
    Font fBtn(L"Segoe UI", 8.8f, FontStyleBold);
    Font fHeroBtn(L"Segoe UI", 10.0f, FontStyleBold);
    Font fTabBtn(L"Segoe UI", 9.0f, FontStyleBold);

    for (const auto& btn : s_buttons) {
        if (btn.tab != -1 && btn.tab != (int)s_currentTab) continue;

        SolidBrush bBtnBg(btn.isActive ? btn.activeColor : (btn.isHovered ? btn.hoverColor : btn.normalColor));
        Pen pBtnBorder(btn.isActive ? btn.activeColor : (btn.isHovered ? Color(255, 80, 90, 105) : Color(255, 48, 54, 61)), 1.0f);

        DrawRoundedRect(g, btn.bounds, 6, bBtnBg, &pBtnBorder);

        SolidBrush bBtnText(btn.isActive ? Color(255, 10, 15, 20) : Color(255, 240, 246, 252));
        if (btn.id == 15) bBtnText.SetColor(Color(255, 255, 120, 120));
        if (btn.id == 18) bBtnText.SetColor(Color(255, 234, 179, 8));
        if (btn.id == 20 && !btn.isActive) bBtnText.SetColor(Color(255, 0, 229, 255));
        if (btn.id == 22 || btn.id == 23) bBtnText.SetColor(Color(255, 0, 229, 255));

        if (btn.id == 1 || btn.id == 19) {
            g.DrawString(btn.text.c_str(), -1, &fHeroBtn, RectF((REAL)btn.bounds.X, (REAL)btn.bounds.Y, (REAL)btn.bounds.Width, (REAL)btn.bounds.Height), &sfCenter, &bBtnText);
        } else if (btn.id >= 100 && btn.id <= 104) {
            g.DrawString(btn.text.c_str(), -1, &fTabBtn, RectF((REAL)btn.bounds.X, (REAL)btn.bounds.Y, (REAL)btn.bounds.Width, (REAL)btn.bounds.Height), &sfCenter, &bBtnText);
        } else {
            g.DrawString(btn.text.c_str(), -1, &fBtn, RectF((REAL)btn.bounds.X, (REAL)btn.bounds.Y, (REAL)btn.bounds.Width, (REAL)btn.bounds.Height), &sfCenter, &bBtnText);
        }
    }

    // 6. Footer
    Font fFooter(L"Segoe UI", 8.5f, FontStyleRegular);
    g.DrawString(L"Підказка: ЛКМ по значку в треї миттєво перемикає Еко/Макс, ПКМ відкриває повне меню.", -1, &fFooter, PointF(25, 688), &textMuted);

    BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

void GuiWindow::PaintDashboard(Graphics& g, const TelemetryData& td, int /*w*/, int /*h*/) {
    SolidBrush textWhite(Color(255, 240, 246, 252));
    SolidBrush textMuted(Color(255, 139, 148, 158));
    SolidBrush textCyan(Color(255, 0, 229, 255));
    SolidBrush textGreen(Color(255, 0, 230, 118));
    SolidBrush textRed(Color(255, 255, 23, 68));

    // 1. Hero Mode Card (Y: 86, H: 104, W: 840)
    DrawCard(g, 20, 86, 840, 104);

    Rect heroBanner(30, 94, 545, 88);
    SolidBrush hBannerBg(td.isEcoMode ? Color(35, 0, 230, 118) : Color(35, 0, 229, 255));
    Pen hBannerBorder(td.isEcoMode ? Color(160, 0, 230, 118) : Color(160, 0, 229, 255), 1.0f);
    DrawRoundedRect(g, heroBanner, 6, hBannerBg, &hBannerBorder);

    Font fHeroTitle(L"Segoe UI", 11.0f, FontStyleBold);
    Font fHeroDesc(L"Segoe UI", 8.8f, FontStyleRegular);
    if (td.isEcoMode) {
        g.DrawString(L"[ECO] РЕЖИМ 'ЕНЕРГОЗБЕРЕЖЕННЯ' АКТИВОВАНО", -1, &fHeroTitle, PointF(45, 104), &textGreen);
        g.DrawString(L"• Дисплей: 60 Hz (-2.5W)  |  CPU Boost: Вимкнено (~2.4 GHz)  |  Quiet Fan Mode\n• Windows: Енергоефективність  |  Усуває стрибки 65W та подовжує час роботи", -1, &fHeroDesc, PointF(45, 130), &textWhite);
    } else {
        g.DrawString(L"[MAX] РЕЖИМ 'МАКСИМАЛЬНА ПРОДУКТИВНІСТЬ' АКТИВОВАНО", -1, &fHeroTitle, PointF(45, 104), &textCyan);
        g.DrawString(L"• Дисплей: 144 Hz (Плавний)  |  CPU Boost: Увімкнено (до 4.4 GHz)  |  Balance Mode\n• Windows: Найвища продуктивність  |  Повна швидкість для важких задач та ігор", -1, &fHeroDesc, PointF(45, 130), &textWhite);
    }

    // 2. Telemetry Row (Y: 198, H: 125)
    DrawCard(g, 20, 198, 415, 125, L"АКУМУЛЯТОР ТА ЗДОРОВ'Я");
    DrawCard(g, 445, 198, 415, 125, L"СПОЖИВАННЯ ТА АВТОНОМНІСТЬ");

    Font fBigNumber(L"Segoe UI", 25.0f, FontStyleBold);
    Font fSubVal(L"Segoe UI", 8.8f, FontStyleRegular);

    std::wstring pctStr = std::to_wstring(td.battery.percent) + L"%";
    g.DrawString(pctStr.c_str(), -1, &fBigNumber, PointF(35, 224), &textGreen);

    Rect barBg(135, 236, 285, 14);
    SolidBrush bBarBg(Color(255, 33, 38, 45));
    DrawRoundedRect(g, barBg, 4, bBarBg);

    int fillW = (int)(285.0 * (td.battery.percent / 100.0));
    if (fillW > 285) fillW = 285;
    if (fillW < 4) fillW = 4;
    Rect barFill(135, 236, fillW, 14);
    SolidBrush bBarFill(td.isEcoMode ? Color(255, 0, 230, 118) : Color(255, 0, 229, 255));
    DrawRoundedRect(g, barFill, 4, bBarFill);

    std::wstringstream batDetails;
    batDetails << L"Ємність: " << td.battery.currentCapacityMWh << L" / " << td.battery.fullCapacityMWh << L" mWh (" 
               << std::fixed << std::setprecision(1) << td.battery.healthPercent << L"% здоров'я, " 
               << td.battery.cycleCount << L" циклів)";
    g.DrawString(batDetails.str().c_str(), -1, &fSubVal, PointF(35, 274), &textMuted);

    std::wstringstream batVolt;
    batVolt << L"Напруга: " << std::fixed << std::setprecision(2) << (td.battery.voltageMV / 1000.0) << L" V  |  "
            << (td.battery.isAcOnline ? L"Підключено до мережі 220V" : L"Автономна робота від АКБ");
    g.DrawString(batVolt.str().c_str(), -1, &fSubVal, PointF(35, 296), &textMuted);

    // Power & Consumption Calculation
    float activeWatts = s_powerHistory.empty() ? 43.0f : s_powerHistory.back();
    Color pwrColor = Color(255, 0, 229, 255);
    std::wstring pwrTypeLabel;

    if (td.battery.isDischarging && td.battery.dischargeRateMW > 0) {
        pwrColor = activeWatts > 28.0f ? Color(255, 255, 23, 68) : Color(255, 249, 115, 22);
        pwrTypeLabel = L"(АКБ Розряд)";
    } else if (td.battery.isCharging && td.battery.dischargeRateMW > 0) {
        pwrColor = Color(255, 0, 230, 118);
        pwrTypeLabel = L"(Зарядка АКБ)";
    } else {
        pwrColor = Color(255, 0, 229, 255);
        pwrTypeLabel = L"(AC 220V)";
    }

    std::wstringstream pwrValStr;
    if (td.battery.isDischarging) pwrValStr << L"-";
    else if (td.battery.isCharging) pwrValStr << L"+";
    pwrValStr << std::fixed << std::setprecision(1) << activeWatts << L" Вт";

    SolidBrush bPwrVal(pwrColor);
    g.DrawString(pwrValStr.str().c_str(), -1, &fBigNumber, PointF(460, 220), &bPwrVal);

    Font fPwrTag(L"Segoe UI", 8.2f, FontStyleBold);
    SolidBrush bPwrTag(pwrColor);
    g.DrawString(pwrTypeLabel.c_str(), -1, &fPwrTag, PointF(460, 258), &bPwrTag);

    std::wstringstream etaStr;
    if (td.battery.isDischarging && td.battery.estimatedMinutes > 0) {
        int hrs = td.battery.estimatedMinutes / 60;
        int mins = td.battery.estimatedMinutes % 60;
        etaStr << L"Орієнтовно: " << hrs << L" год " << mins << L" хв (" << (td.battery.voltageMV / 1000.0) << L" V)";
    } else {
        etaStr << L"Схема: " << td.winPower.activeSchemeName;
    }
    g.DrawString(etaStr.str().c_str(), -1, &fSubVal, PointF(460, 288), &textMuted);

    // Subtitle note next to button
    Font fMiniHint(L"Segoe UI", 7.8f, FontStyleRegular);
    g.DrawString(L"Історія споживання та вати", -1, &fMiniHint, PointF(660, 276), &textMuted);

    // 3. Hardware Controls Grid (Y: 331, H: 226, W: 840)
    DrawCard(g, 20, 331, 840, 226, L"АПАРАТНІ НАЛАШТУВАННЯ ТА ПРОФІЛІ");

    Font fGroup(L"Segoe UI", 9.0f, FontStyleBold);
    g.DrawString(L"Частота екрана:", -1, &fGroup, PointF(30, 355), &textWhite);
    g.DrawString(L"Intel Turbo Boost (АКБ):", -1, &fGroup, PointF(450, 355), &textWhite);
    g.DrawString(L"Режим охолодження (Fn+Q):", -1, &fGroup, PointF(30, 418), &textWhite);
    g.DrawString(L"Режим відеокарти (MUX Switch):", -1, &fGroup, PointF(450, 418), &textWhite);
    g.DrawString(L"Клавіатура, Автоматизація та Журнал:", -1, &fGroup, PointF(30, 482), &textWhite);

    // 4. NVIDIA Watchdog Card (Y: 565, H: 104, W: 840)
    DrawCard(g, 20, 565, 840, 104, L"NVIDIA GEFORCE RTX WATCHDOG ТА ПРОЦЕСИ");

    Font fGpuName(L"Segoe UI", 10.0f, FontStyleBold);
    Font fGpuStatus(L"Segoe UI", 8.8f, FontStyleRegular);

    std::wstring gpuHeader = td.gpu.model + L": ";
    if (td.gpu.isAwake) {
        gpuHeader += L"АКТИВНА (" + std::to_wstring((int)td.gpu.powerWatts) + L" W, " + std::to_wstring(td.gpu.temperatureC) + L" C)";
        g.DrawString(gpuHeader.c_str(), -1, &fGpuName, PointF(35, 588), &textRed);

        std::wstring procList = L"[!] Процеси на dGPU (" + std::to_wstring(td.gpu.processes.size()) + L"): ";
        if (td.gpu.processes.empty()) {
            procList = L"[!] Графічний контекст активний (зовнішній дисплей або системний драйвер)";
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
        RectF rProc(35.0f, 612.0f, 395.0f, 48.0f);
        StringFormat sfWrap;
        sfWrap.SetTrimming(StringTrimmingEllipsisWord);
        g.DrawString(procList.c_str(), -1, &fGpuStatus, rProc, &sfWrap, &textWhite);
    } else {
        gpuHeader += L"СПИТЬ (D3Cold, 0 W — Максимальна економія)";
        g.DrawString(gpuHeader.c_str(), -1, &fGpuName, PointF(35, 588), &textGreen);
        g.DrawString(L"[OK] Жоден процес не тримає dGPU. Повний C-стан сну, відеокарта знеструмлена.", -1, &fGpuStatus, PointF(35, 614), &textMuted);
    }
}

void GuiWindow::PaintGraphs(Graphics& g, const TelemetryData& td, int /*w*/, int /*h*/) {
    SolidBrush textWhite(Color(255, 240, 246, 252));
    SolidBrush textMuted(Color(255, 139, 148, 158));
    SolidBrush textCyan(Color(255, 0, 229, 255));
    SolidBrush textGreen(Color(255, 0, 230, 118));
    SolidBrush textRed(Color(255, 255, 23, 68));

    // Top Header info banner
    Font fTopHint(L"Segoe UI", 9.2f, FontStyleBold);
    std::wstringstream topInfo;
    float curWatts = s_powerHistory.empty() ? 43.0f : s_powerHistory.back();
    topInfo << L"ПОТОЧНЕ СПОЖИВАННЯ: " << std::fixed << std::setprecision(1) << curWatts << L" Вт   |   НАПРУГА: "
            << std::fixed << std::setprecision(2) << (td.battery.voltageMV / 1000.0) << L" V   |   БАТАРЕЯ: "
            << td.battery.percent << L"% (" << (td.battery.isAcOnline ? L"220V Мережа" : L"DC Акумулятор") << L")";
    g.DrawString(topInfo.str().c_str(), -1, &fTopHint, PointF(250, 92), &textCyan);

    // 1. Large High-Resolution Power Graph Card (X: 20, Y: 124, W: 840, H: 255)
    DrawCard(g, 20, 124, 840, 255, L"МОНІТОРИНГ СПОЖИВАННЯ ПОТУЖНОСТІ В РЕАЛЬНОМУ ЧАСІ (WATT TIMELINE — 60 СЕК)");

    Rect gBox(35, 152, 810, 175);
    SolidBrush bGBg(Color(255, 17, 22, 30));
    Pen pGBorder(Color(255, 48, 54, 61), 1.0f);
    DrawRoundedRect(g, gBox, 6, bGBg, &pGBorder);

    // Min / Max / Avg computation across 60 samples
    float minPwr = s_powerHistory[0];
    float maxPwr = s_powerHistory[0];
    float sumPwr = 0.0f;
    for (float v : s_powerHistory) {
        if (v < minPwr) minPwr = v;
        if (v > maxPwr) maxPwr = v;
        sumPwr += v;
    }
    float avgPwr = sumPwr / (float)s_powerHistory.size();
    if (maxPwr - minPwr < 6.0f) {
        maxPwr += 4.0f;
        minPwr = (minPwr > 4.0f) ? (minPwr - 2.0f) : 0.0f;
    }

    // Grid lines inside graph
    Pen pGGrid(Color(35, 255, 255, 255), 1.0f);
    for (int i = 1; i <= 3; i++) {
        int gy = gBox.Y + (gBox.Height * i) / 4;
        g.DrawLine(&pGGrid, gBox.X + 8, gy, gBox.X + gBox.Width - 8, gy);
    }

    // Build smooth polyline
    int nPoints = static_cast<int>(s_powerHistory.size());
    if (nPoints >= 2) {
        std::vector<PointF> pts(nPoints);
        float stepX = (float)(gBox.Width - 24) / (float)(nPoints - 1);
        for (int i = 0; i < nPoints; i++) {
            float valNorm = (s_powerHistory[i] - minPwr) / (maxPwr - minPwr);
            if (valNorm < 0.0f) valNorm = 0.0f;
            if (valNorm > 1.0f) valNorm = 1.0f;
            float px = (float)(gBox.X + 12) + i * stepX;
            float py = (float)(gBox.Y + gBox.Height - 12) - valNorm * (float)(gBox.Height - 24);
            pts[i] = PointF(px, py);
        }

        // Smooth polygon fill under graph
        GraphicsPath path;
        path.AddLine(pts[0].X, (float)(gBox.Y + gBox.Height - 12), pts[0].X, pts[0].Y);
        path.AddLines(pts.data(), nPoints);
        path.AddLine(pts[nPoints - 1].X, pts[nPoints - 1].Y, pts[nPoints - 1].X, (float)(gBox.Y + gBox.Height - 12));
        path.CloseFigure();

        LinearGradientBrush fillGrad(PointF((float)gBox.X, (float)gBox.Y),
                                    PointF((float)gBox.X, (float)(gBox.Y + gBox.Height)),
                                    Color(60, 0, 229, 255),
                                    Color(5, 0, 229, 255));
        g.FillPath(&fillGrad, &path);

        // Neon outline
        Pen pGLine(Color(255, 0, 229, 255), 2.2f);
        g.DrawLines(&pGLine, pts.data(), nPoints);

        // Glowing live dot
        SolidBrush bDot(Color(255, 255, 255, 255));
        g.FillEllipse(&bDot, pts.back().X - 4.0f, pts.back().Y - 4.0f, 8.0f, 8.0f);
    }

    // Min / Max / Avg numeric readouts below graph inside card
    Font fStat(L"Segoe UI", 9.0f, FontStyleBold);
    std::wstringstream sMax, sMin, sAvg, sCur;
    sMax << L"▲ ПІКОВЕ (MAX): " << std::fixed << std::setprecision(1) << maxPwr << L" Вт";
    sMin << L"▼ МІНІМУМ (MIN): " << std::fixed << std::setprecision(1) << minPwr << L" Вт";
    sAvg << L"• СЕРЕДНЄ (AVG): " << std::fixed << std::setprecision(1) << avgPwr << L" Вт";
    sCur << L"⚡ ПОТОЧНЕ: " << std::fixed << std::setprecision(1) << curWatts << L" Вт";

    g.DrawString(sCur.str().c_str(), -1, &fStat, PointF(40, 345), &textCyan);
    g.DrawString(sMax.str().c_str(), -1, &fStat, PointF(230, 345), &textRed);
    g.DrawString(sMin.str().c_str(), -1, &fStat, PointF(430, 345), &textGreen);
    g.DrawString(sAvg.str().c_str(), -1, &fStat, PointF(630, 345), &textWhite);

    // 2. Bottom Row Cards (Y: 388, H: 280)
    DrawCard(g, 20, 388, 415, 280, L"ДЕТАЛЬНА ТЕЛЕМЕТРІЯ АКУМУЛЯТОРА");
    DrawCard(g, 445, 388, 415, 280, L"СПОЖИВАННЯ ТА НАВАНТАЖЕННЯ АПАРАТУРИ");

    Font fItemLabel(L"Segoe UI", 9.0f, FontStyleRegular);
    Font fItemVal(L"Segoe UI", 9.0f, FontStyleBold);

    // Battery items
    auto drawBatRow = [&](int y, const std::wstring& label, const std::wstring& val, const Brush& valBrush) {
        g.DrawString(label.c_str(), -1, &fItemLabel, PointF(35, (REAL)y), &textMuted);
        g.DrawString(val.c_str(), -1, &fItemVal, PointF(225, (REAL)y), &valBrush);
    };

    drawBatRow(418, L"Рівень заряду:", std::to_wstring(td.battery.percent) + L"%", textGreen);
    drawBatRow(444, L"Паспортна ємність:", std::to_wstring(td.battery.fullCapacityMWh) + L" mWh", textWhite);
    drawBatRow(470, L"Залишок ємності:", std::to_wstring(td.battery.currentCapacityMWh) + L" mWh", textWhite);
    
    std::wstringstream ssHealth;
    ssHealth << std::fixed << std::setprecision(1) << td.battery.healthPercent << L"% (Відмінний)";
    drawBatRow(496, L"Здоров'я акумулятора:", ssHealth.str(), textGreen);
    drawBatRow(522, L"Кількість циклів:", std::to_wstring(td.battery.cycleCount) + L" циклів", textWhite);

    std::wstring pwrFlow = td.battery.isDischarging ? (L"-" + std::to_wstring(td.battery.dischargeRateMW) + L" mW (Розряд)")
                         : (td.battery.isCharging ? (L"+" + std::to_wstring(td.battery.dischargeRateMW) + L" mW (Зарядка)")
                         : L"0 mW (Мережа 220V)");
    drawBatRow(548, L"Потужність струму:", pwrFlow, td.battery.isDischarging ? textRed : textGreen);

    std::wstringstream ssVolt;
    ssVolt << std::fixed << std::setprecision(2) << (td.battery.voltageMV / 1000.0) << L" V";
    drawBatRow(574, L"Напруга на елементах:", ssVolt.str(), textWhite);

    std::wstring winSaverTxt = td.winPower.isEnergySaverActive ? L"УВІМКНЕНО (Power Efficiency)" : L"ВИМКНЕНО (Balanced)";
    drawBatRow(600, L"Windows Energy Saver:", winSaverTxt, td.winPower.isEnergySaverActive ? textGreen : textMuted);

    std::wstring etaFull = td.battery.isAcOnline ? L"Робота від мережі (Необмежено)" 
                         : (std::to_wstring(td.battery.estimatedMinutes / 60) + L" год " + std::to_wstring(td.battery.estimatedMinutes % 60) + L" хв");
    drawBatRow(626, L"Орієнтовний час роботи:", etaFull, textCyan);

    // Hardware items
    auto drawHwRow = [&](int y, const std::wstring& label, const std::wstring& val, const Brush& valBrush) {
        g.DrawString(label.c_str(), -1, &fItemLabel, PointF(460, (REAL)y), &textMuted);
        g.DrawString(val.c_str(), -1, &fItemVal, PointF(635, (REAL)y), &valBrush);
    };

    drawHwRow(418, L"Центральний процесор:", L"Intel Core i5-12450HX", textWhite);
    drawHwRow(444, L"Режим CPU Boost:", CpuController::ToString(td.cpuBoost), (td.cpuBoost == CpuBoostMode::Disabled ? textGreen : textCyan));
    drawHwRow(470, L"Режим кулерів Fn+Q:", LenovoWmi::ThermalToString(td.thermalMode), textWhite);
    drawHwRow(496, L"Частота екрана:", std::to_wstring(td.refreshRate) + L" Hz", (td.refreshRate == 60 ? textGreen : textCyan));
    drawHwRow(522, L"Споживання екрана:", (td.refreshRate == 60 ? L"~9.0 Вт (Економія -2.5W)" : L"~11.5 Вт (Макс. плавність)"), textWhite);
    drawHwRow(548, L"Дискретна відеокарта:", td.gpu.model, textWhite);
    drawHwRow(574, L"Стан dGPU:", (td.gpu.isAwake ? (L"АКТИВНА (" + std::to_wstring((int)td.gpu.powerWatts) + L" W, " + std::to_wstring(td.gpu.temperatureC) + L"°C)") : L"СПИТЬ (D3Cold, 0 W)"), (td.gpu.isAwake ? textRed : textGreen));
    drawHwRow(600, L"Режим MUX Switch:", LenovoWmi::MuxToString(td.muxMode), textCyan);
    drawHwRow(626, L"Підсвітка клавіатури:", LenovoWmi::KbdToString(td.kbdLight), textWhite);
}

void GuiWindow::PaintDevices(Graphics& g, const TelemetryData& td, int /*w*/, int /*h*/) {
    SolidBrush textWhite(Color(255, 240, 246, 252));
    SolidBrush textMuted(Color(255, 139, 148, 158));
    SolidBrush textCyan(Color(255, 0, 229, 255));
    SolidBrush textGreen(Color(255, 0, 230, 118));
    SolidBrush textRed(Color(255, 255, 23, 68));

    // 1. Card 1: NVIDIA RTX Watchdog Processes (X: 20, Y: 124, W: 840, H: 240)
    DrawCard(g, 20, 124, 840, 240, L"NVIDIA GEFORCE RTX WATCHDOG — СПИСОК ПРОЦЕСІВ НА ВІДЕОКАРТІ");

    Font fStatusHeader(L"Segoe UI", 9.5f, FontStyleBold);
    std::wstring gpuSummary = td.gpu.model + L": ";
    if (td.gpu.isAwake) {
        gpuSummary += L"АКТИВНА (" + std::to_wstring((int)td.gpu.powerWatts) + L" Вт, " + std::to_wstring(td.gpu.temperatureC) + L"°C) — Процесів на dGPU: " + std::to_wstring(td.gpu.processes.size());
        g.DrawString(gpuSummary.c_str(), -1, &fStatusHeader, PointF(35, 150), &textRed);
    } else {
        gpuSummary += L"СПИТЬ (D3Cold, 0 Вт — Максимальна економія заряду батареї)";
        g.DrawString(gpuSummary.c_str(), -1, &fStatusHeader, PointF(35, 150), &textGreen);
    }

    if (td.gpu.processes.empty()) {
        Rect okBox(35, 180, 810, 160);
        SolidBrush bOkBg(Color(25, 0, 230, 118));
        Pen pOkBorder(Color(120, 0, 230, 118), 1.0f);
        DrawRoundedRect(g, okBox, 6, bOkBg, &pOkBorder);

        Font fOkTitle(L"Segoe UI", 11.0f, FontStyleBold);
        Font fOkDesc(L"Segoe UI", 9.0f, FontStyleRegular);
        g.DrawString(L"[✓] Жодного стороннього процесу на dGPU не виявлено!", -1, &fOkTitle, PointF(55, 210), &textGreen);
        g.DrawString(L"• Відеокарта NVIDIA повністю знеструмлена в системному стані D3Cold.\n• Усі задачі робочого столу та вікна виконуються на енергоефективній графіці Intel UHD.\n• Забезпечено максимальну автономність від акумулятора (нульовий витік енергії на PCIe).", -1, &fOkDesc, PointF(55, 245), &textWhite);
    } else {
        // Table Header
        Font fTh(L"Segoe UI", 8.5f, FontStyleBold);
        g.DrawString(L"№", -1, &fTh, PointF(35, 180), &textMuted);
        g.DrawString(L"НАЗВА ПРОЦЕСУ", -1, &fTh, PointF(70, 180), &textMuted);
        g.DrawString(L"PID", -1, &fTh, PointF(320, 180), &textMuted);
        g.DrawString(L"ПАМ'ЯТЬ VRAM", -1, &fTh, PointF(410, 180), &textMuted);
        g.DrawString(L"ВПЛИВ НА ЕНЕРГІЮ", -1, &fTh, PointF(530, 180), &textMuted);
        g.DrawString(L"ДІЯ", -1, &fTh, PointF(760, 180), &textMuted);

        Pen pDiv(Color(35, 255, 255, 255), 1.0f);
        g.DrawLine(&pDiv, 35, 202, 845, 202);

        Font fRow(L"Segoe UI", 9.0f, FontStyleRegular);
        Font fKillBtn(L"Segoe UI", 8.2f, FontStyleBold);
        StringFormat sfCenter;
        sfCenter.SetAlignment(StringAlignmentCenter);
        sfCenter.SetLineAlignment(StringAlignmentCenter);

        for (size_t i = 0; i < td.gpu.processes.size() && i < 4; i++) {
            int rowY = 210 + (int)i * 36;
            std::wstring idxStr = std::to_wstring(i + 1) + L".";
            g.DrawString(idxStr.c_str(), -1, &fRow, PointF(35, (REAL)rowY), &textWhite);
            g.DrawString(td.gpu.processes[i].name.c_str(), -1, &fRow, PointF(70, (REAL)rowY), &textCyan);
            g.DrawString(std::to_wstring(td.gpu.processes[i].pid).c_str(), -1, &fRow, PointF(320, (REAL)rowY), &textWhite);
            
            std::wstring memStr = td.gpu.processes[i].memoryMB > 0 ? (std::to_wstring(td.gpu.processes[i].memoryMB) + L" MB") : L"Активний";
            g.DrawString(memStr.c_str(), -1, &fRow, PointF(410, (REAL)rowY), &textWhite);
            g.DrawString(L"Утримує dGPU (+8W)", -1, &fRow, PointF(530, (REAL)rowY), &textRed);

            // Per-process kill button
            Rect rKill(745, rowY - 3, 95, 26);
            SolidBrush bKillBg(Color(255, 45, 15, 20));
            Pen pKill(Color(255, 239, 68, 68), 1.0f);
            DrawRoundedRect(g, rKill, 4, bKillBg, &pKill);
            SolidBrush bKillTxt(Color(255, 255, 120, 120));
            g.DrawString(L"Завершити", -1, &fKillBtn, RectF((REAL)rKill.X, (REAL)rKill.Y, (REAL)rKill.Width, (REAL)rKill.Height), &sfCenter, &bKillTxt);
        }
    }

    // 2. Card 2: System Hardware Devices (X: 20, Y: 375, H: 295)
    DrawCard(g, 20, 375, 840, 295, L"АПАРАТНІ ДЕВАЙСИ ТА КОМПОНЕНТИ СИСТЕМИ");

    // 6 Device Tiles (2 columns x 3 rows)
    auto drawDevTile = [&](int x, int y, int w, int h, const std::wstring& title, const std::wstring& line1, const std::wstring& line2) {
        Rect rTile(x, y, w, h);
        SolidBrush bTileBg(Color(255, 17, 22, 30));
        Pen pTileBorder(Color(255, 48, 54, 61), 1.0f);
        DrawRoundedRect(g, rTile, 6, bTileBg, &pTileBorder);

        Font fTTitle(L"Segoe UI", 9.2f, FontStyleBold);
        Font fTBody(L"Segoe UI", 8.4f, FontStyleRegular);
        g.DrawString(title.c_str(), -1, &fTTitle, PointF((REAL)x + 12, (REAL)y + 8), &textCyan);
        g.DrawString(line1.c_str(), -1, &fTBody, PointF((REAL)x + 12, (REAL)y + 32), &textWhite);
        g.DrawString(line2.c_str(), -1, &fTBody, PointF((REAL)x + 12, (REAL)y + 52), &textMuted);
    };

    int colW = 398;
    int rowH = 78;
    int c1 = 35;
    int c2 = 447;

    // Row 1
    drawDevTile(c1, 408, colW, rowH, L"1. Центральний процесор (CPU)", 
                td.system.cpuName, 
                L"8 ядер (4P + 4E), 12 потоків  |  до 4.4 GHz Turbo Boost");

    drawDevTile(c2, 408, colW, rowH, L"2. Дискретна відеокарта (dGPU)", 
                td.gpu.model, 
                L"6 GB GDDR6  |  PCIe Link Power State: L1.2 Substates");

    // Row 2
    drawDevTile(c1, 496, colW, rowH, L"3. Вбудована графіка (iGPU)", 
                L"Intel(R) UHD Graphics (Alder Lake-HX)", 
                L"32 Execution Units  |  Основний адаптер робочого столу DWM");

    std::wstring dispDesc = std::to_wstring(td.refreshRate) + L" Hz (Підтримує 60 Hz та 144 Hz)";
    drawDevTile(c2, 496, colW, rowH, L"4. Вбудований дисплей ноутбука", 
                L"Full HD 1920x1080 IPS Matrix", 
                dispDesc);

    // Row 3
    std::wstring extDispTxt = td.system.hasExternalDisplay 
        ? (L"Підключено: " + std::to_wstring(td.system.externalDisplayCount) + L" монітор(и)") 
        : L"Зовнішні монітори не підключені";
    drawDevTile(c1, 584, colW, rowH, L"5. Зовнішні монітори та порти", 
                L"HDMI 2.1 / USB Type-C DisplayPort", 
                extDispTxt);

    std::wstring ecTxt = L"BIOS: " + td.system.biosVersion + L"  |  Lenovo WMI Interface";
    drawDevTile(c2, 584, colW, rowH, L"6. Материнська плата та EC", 
                L"Lenovo ACPI Controller", 
                ecTxt);
}

bool GuiWindow::HandleClick(HWND hWnd, int x, int y) {
    // 1. Check per-process kill buttons on Devices tab
    if (s_currentTab == GuiTab::Devices) {
        TelemetryData td = TelemetryThread::GetData();
        for (size_t i = 0; i < td.gpu.processes.size() && i < 4; i++) {
            int rowY = 210 + (int)i * 36;
            Rect rKill(745, rowY - 3, 95, 26);
            if (rKill.Contains(x, y)) {
                Logger::Info(L"Watchdog", L"Terminating specific rogue process: " + td.gpu.processes[i].name + L" (PID " + std::to_wstring(td.gpu.processes[i].pid) + L")");
                GpuController::TerminateProcessByPid(td.gpu.processes[i].pid);
                InvalidateRect(hWnd, NULL, FALSE);
                return true;
            }
        }
    }

    // 2. Check all configured GUI buttons
    for (const auto& btn : s_buttons) {
        if (btn.tab != -1 && btn.tab != (int)s_currentTab) continue;

        if (btn.bounds.Contains(x, y)) {
            switch (btn.id) {
                case 100: // Tab: Dashboard
                    SetTab(GuiTab::Dashboard);
                    break;
                case 101: // Tab: Graphs
                case 22:  // Dashboard "Open Graphs" button
                    SetTab(GuiTab::Graphs);
                    break;
                case 102: // Tab: Devices
                case 23:  // Dashboard "All processes & devices" button
                    SetTab(GuiTab::Devices);
                    break;
                case 103: // Return from Graphs to Dashboard
                case 104: // Return from Devices to Dashboard
                    SetTab(GuiTab::Dashboard);
                    break;
                case 1: // Hero Eco Profile
                    TelemetryThread::ApplyEcoProfile();
                    break;
                case 19: // Hero Perf Profile
                    TelemetryThread::ApplyPerformanceProfile();
                    break;
                case 2: // 60 Hz
                    DisplayController::SetRefreshRate(60);
                    TelemetryThread::SetRefreshRate(60);
                    break;
                case 3: // 144 Hz
                    DisplayController::SetRefreshRate(144);
                    TelemetryThread::SetRefreshRate(144);
                    break;
                case 4: // Boost off
                    CpuController::ApplyEcoProfile();
                    TelemetryThread::SetCpuBoost(CpuBoostMode::Disabled);
                    break;
                case 5: // Boost on
                    CpuController::ApplyPerformanceProfile();
                    TelemetryThread::SetCpuBoost(CpuBoostMode::Aggressive);
                    break;
                case 6: // Quiet
                    LenovoWmi::SetThermalMode(ThermalMode::Quiet);
                    TelemetryThread::SetThermalMode(ThermalMode::Quiet);
                    break;
                case 7: // Balance
                    LenovoWmi::SetThermalMode(ThermalMode::Balance);
                    TelemetryThread::SetThermalMode(ThermalMode::Balance);
                    break;
                case 8: // Perf
                    LenovoWmi::SetThermalMode(ThermalMode::Performance);
                    TelemetryThread::SetThermalMode(ThermalMode::Performance);
                    break;
                case 9: // MUX Hybrid
                    LenovoWmi::SetMuxMode(GpuMuxMode::Hybrid);
                    TelemetryThread::SetMuxMode(GpuMuxMode::Hybrid);
                    Logger::Info(L"MUX", L"Switched to Hybrid mode (Applies on reboot)");
                    break;
                case 10: // MUX iGPU Only
                    LenovoWmi::SetMuxMode(GpuMuxMode::iGpuOnly);
                    TelemetryThread::SetMuxMode(GpuMuxMode::iGpuOnly);
                    Logger::Info(L"MUX", L"Switched to iGPU Only mode (Applies on reboot)");
                    break;
                case 11: // MUX dGPU Only
                    LenovoWmi::SetMuxMode(GpuMuxMode::dGpuOnly);
                    TelemetryThread::SetMuxMode(GpuMuxMode::dGpuOnly);
                    Logger::Info(L"MUX", L"Switched to dGPU Only mode (Applies on reboot)");
                    break;
                case 12: // Toggle Keyboard Light
                    LenovoWmi::ToggleKeyboardLight();
                    TelemetryThread::SetKeyboardLight(LenovoWmi::GetKeyboardLight());
                    break;
                case 13: { // Auto-switch toggle
                    bool current = AppConfig::IsAutoSwitchEnabled();
                    AppConfig::SetAutoSwitchEnabled(!current);
                    Logger::Info(L"Config", std::wstring(L"Auto-switch AC/DC toggled: ") + (!current ? L"ON" : L"OFF"));
                    break;
                }
                case 14: { // Auto-start toggle
                    bool current = AppConfig::IsAutoStartEnabled();
                    AppConfig::SetAutoStartEnabled(!current);
                    Logger::Info(L"Config", std::wstring(L"Auto-start with Windows toggled: ") + (!current ? L"ON" : L"OFF"));
                    break;
                }
                case 15: // Kill dGPU apps
                    Logger::Info(L"Watchdog", L"User clicked terminate rogue dGPU processes");
                    GpuController::TerminateAllRogueProcesses();
                    break;
                case 16: // Minimize
                    ShowWindow(hWnd, SW_HIDE);
                    break;
                case 17: // Close
                    ShowWindow(hWnd, SW_HIDE);
                    break;
                case 18: // Open Log
                    Logger::OpenLogFile();
                    break;
                case 21: // Toggle Windows Energy Saver
                    WindowsPowerController::ToggleEnergySaver();
                    TelemetryThread::RefreshPowerStatus();
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                case 20: { // In-App Update check / install
                    UpdateStatus st = UpdateManager::GetStatus();
                    if (st == UpdateStatus::UpdateAvailable) {
                        int res = MessageBoxW(hWnd, 
                            (L"Знайдено нову версію EnergyControl: " + UpdateManager::GetInfo().latestVersion + 
                             L"\n\nБажаєте завантажити та підготувати оновлення зараз?").c_str(),
                            L"EnergyControl Auto-Update", MB_YESNO | MB_ICONQUESTION);
                        if (res == IDYES) {
                            UpdateManager::DownloadAndInstallAsync([hWnd](bool ok, const std::wstring& msg) {
                                if (ok) {
                                    int restart = MessageBoxW(hWnd, 
                                        L"Оновлення успішно завантажено!\nПерезапустити EnergyControl для застосування?", 
                                        L"EnergyControl Auto-Update", MB_YESNO | MB_ICONINFORMATION);
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
                                TrayManager::ShowBalloon(L"EnergyControl", L"У вас встановлено найновішу версію!");
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
    return false;
}

void GuiWindow::HandleMouseMove(HWND hWnd, int x, int y) {
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
