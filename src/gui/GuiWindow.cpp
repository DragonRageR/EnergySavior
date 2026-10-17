#include "../../include/gui/GuiWindow.hpp"
#include "../../include/core/TelemetryThread.hpp"
#include "../../include/core/AppConfig.hpp"
#include "../../include/core/Logger.hpp"
#include "../../include/gui/TrayManager.hpp"
#include <iomanip>
#include <sstream>

using namespace Gdiplus;

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
        Font font(L"Segoe UI", 9.5f, FontStyleBold);
        SolidBrush titleBrush(Color(255, 139, 148, 158));
        g.DrawString(title.c_str(), -1, &font, PointF((REAL)x + 16, (REAL)y + 10), &titleBrush);
    }
}

void GuiWindow::InitButtons() {
    s_buttons.clear();

    // 1. Hero Mode Buttons (ID 1: Eco, ID 19: Perf)
    s_buttons.push_back({ 1, Rect(585, 74, 125, 88), L"[ 🌿 ЕКО ]\n60Hz + Quiet", false, false, Color(255, 0, 230, 118), Color(255, 20, 50, 35), Color(255, 22, 27, 34) });
    s_buttons.push_back({ 19, Rect(720, 74, 125, 88), L"[ 🚀 МАКС ]\n144Hz + Boost", false, false, Color(255, 0, 229, 255), Color(255, 20, 45, 60), Color(255, 22, 27, 34) });

    // 2. Refresh Rate 60 Hz (ID 2), 144 Hz (ID 3)
    s_buttons.push_back({ 2, Rect(30, 380, 195, 34), L"60 Hz (Енергозбереження)", false, false, Color(255, 0, 230, 118) });
    s_buttons.push_back({ 3, Rect(235, 380, 195, 34), L"144 Hz (Макс. плавність)", false, false, Color(255, 0, 229, 255) });

    // 3. CPU Turbo Boost: Off (ID 4), Aggressive (ID 5)
    s_buttons.push_back({ 4, Rect(450, 380, 195, 34), L"Boost OFF (~2.4 GHz)", false, false, Color(255, 0, 230, 118) });
    s_buttons.push_back({ 5, Rect(655, 380, 195, 34), L"Boost ON (до 4.4 GHz)", false, false, Color(255, 0, 229, 255) });

    // 4. Lenovo Thermal Mode: Quiet (ID 6), Balance (ID 7), Performance (ID 8)
    s_buttons.push_back({ 6, Rect(30, 444, 125, 34), L"Quiet (Fn+Q)", false, false, Color(255, 59, 130, 246) });
    s_buttons.push_back({ 7, Rect(165, 444, 125, 34), L"Balance", false, false, Color(255, 243, 244, 246) });
    s_buttons.push_back({ 8, Rect(300, 444, 125, 34), L"Performance", false, false, Color(255, 239, 68, 68) });

    // 5. MUX Switch / iGPU Mode: Hybrid (ID 9), iGPU Only (ID 10), dGPU Only (ID 11)
    s_buttons.push_back({ 9, Rect(450, 444, 125, 34), L"MUX: Hybrid", false, false, Color(255, 0, 229, 255) });
    s_buttons.push_back({ 10, Rect(585, 444, 135, 34), L"MUX: iGPU Only", false, false, Color(255, 0, 230, 118) });
    s_buttons.push_back({ 11, Rect(730, 444, 120, 34), L"MUX: dGPU", false, false, Color(255, 239, 68, 68) });

    // 6. Keyboard Light (ID 12)
    s_buttons.push_back({ 12, Rect(30, 508, 195, 34), L"Підсвітка клавіатури", false, false, Color(255, 147, 51, 234) });

    // 7. Auto-switch (ID 13) & Auto-start (ID 14)
    s_buttons.push_back({ 13, Rect(235, 508, 195, 34), L"Авто-перемикач AC/DC", false, false, Color(255, 0, 230, 118) });
    s_buttons.push_back({ 14, Rect(450, 508, 205, 34), L"Автозапуск з Windows", false, false, Color(255, 0, 229, 255) });

    // 8. Open Log File (ID 18)
    s_buttons.push_back({ 18, Rect(665, 508, 185, 34), L"[!] Журнал подій (Лог)", false, false, Color(255, 234, 179, 8) });

    // 9. Kill dGPU apps (ID 15)
    s_buttons.push_back({ 15, Rect(645, 592, 205, 48), L"[X] Закрити софт на dGPU", false, false, Color(255, 239, 68, 68), Color(255, 60, 20, 25), Color(255, 45, 15, 20) });

    // 10. Titlebar Buttons: Minimize (ID 16), Close (ID 17)
    s_buttons.push_back({ 16, Rect(808, 12, 28, 26), L"—", false, false, Color(255, 100, 100, 100), Color(255, 40, 45, 55), Color(255, 22, 27, 34) });
    s_buttons.push_back({ 17, Rect(842, 12, 28, 26), L"✕", false, false, Color(255, 255, 60, 60), Color(255, 80, 25, 25), Color(255, 22, 27, 34) });
}

void GuiWindow::Paint(HWND hWnd, HDC hdc) {
    RECT clientRc;
    GetClientRect(hWnd, &clientRc);
    int w = clientRc.right - clientRc.left;
    int h = clientRc.bottom - clientRc.top;

    // Instant read from telemetry worker thread - 0ms, zero lag!
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

    // Dynamic Universal Header
    Font fontTitle(L"Segoe UI", 13.0f, FontStyleBold);
    Font fontSub(L"Segoe UI", 8.5f, FontStyleRegular);
    SolidBrush textWhite(Color(255, 240, 246, 252));
    SolidBrush textMuted(Color(255, 139, 148, 158));
    SolidBrush textCyan(Color(255, 0, 229, 255));
    SolidBrush textGreen(Color(255, 0, 230, 118));
    SolidBrush textRed(Color(255, 255, 23, 68));

    std::wstring appTitle = td.system.manufacturer + L" " + td.system.model + L" ENERGY CONTROL";
    g.DrawString(appTitle.c_str(), -1, &fontTitle, PointF(25, 14), &textWhite);

    std::wstring sysSub = L"BIOS: " + td.system.biosVersion + L"  |  " + td.system.cpuName;
    if (td.system.hasExternalDisplay) {
        sysSub += L"  |  [Зовнішній дисплей: " + std::to_wstring(td.system.externalDisplayCount) + L"]";
    }
    g.DrawString(sysSub.c_str(), -1, &fontSub, PointF(25, 38), &textMuted);

    // Admin, Power & Windows Saver Badges
    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);
    Font fBadge(L"Segoe UI", 8.2f, FontStyleBold);

    Rect badgeAdmin(385, 14, 125, 24);
    SolidBrush bAdminBg(td.isElevated ? Color(40, 0, 230, 118) : Color(40, 234, 179, 8));
    Pen pAdmin(td.isElevated ? Color(180, 0, 230, 118) : Color(180, 234, 179, 8), 1.0f);
    DrawRoundedRect(g, badgeAdmin, 4, bAdminBg, &pAdmin);
    SolidBrush bAdminText(td.isElevated ? Color(255, 0, 230, 118) : Color(255, 234, 179, 8));
    g.DrawString(td.isElevated ? L"АДМІНІСТРАТОР" : L"ЗВИЧАЙНИЙ ЮЗЕР", -1, &fBadge, RectF(385, 14, 125, 24), &sfCenter, &bAdminText);

    Rect badgePower(520, 14, 130, 24);
    SolidBrush bPwrBg(td.battery.isAcOnline ? Color(40, 0, 229, 255) : Color(40, 249, 115, 22));
    Pen pPwr(td.battery.isAcOnline ? Color(180, 0, 229, 255) : Color(180, 249, 115, 22), 1.0f);
    DrawRoundedRect(g, badgePower, 4, bPwrBg, &pPwr);
    SolidBrush bPwrText(td.battery.isAcOnline ? Color(255, 0, 229, 255) : Color(255, 249, 115, 22));
    g.DrawString(td.battery.isAcOnline ? L"МЕРЕЖА (220V)" : L"БАТАРЕЯ (DC)", -1, &fBadge, RectF(520, 14, 130, 24), &sfCenter, &bPwrText);

    // Windows Battery Saver / Energy Saver Badge
    Rect badgeSaver(660, 14, 135, 24);
    bool isWinSaver = td.winPower.isEnergySaverActive;
    SolidBrush bSaverBg(isWinSaver ? Color(40, 0, 230, 118) : Color(30, 100, 110, 125));
    Pen pSaver(isWinSaver ? Color(180, 0, 230, 118) : Color(80, 100, 110, 125), 1.0f);
    DrawRoundedRect(g, badgeSaver, 4, bSaverBg, &pSaver);
    SolidBrush bSaverText(isWinSaver ? Color(255, 0, 230, 118) : Color(255, 139, 148, 158));
    g.DrawString(isWinSaver ? L"WIN SAVER: ON" : L"WIN SAVER: OFF", -1, &fBadge, RectF(660, 14, 135, 24), &sfCenter, &bSaverText);

    // 2. Hero Mode Card (Y: 66, H: 104, W: 840)
    DrawCard(g, 20, 66, 840, 104);

    Rect heroBanner(30, 74, 545, 88);
    SolidBrush hBannerBg(td.isEcoMode ? Color(35, 0, 230, 118) : Color(35, 0, 229, 255));
    Pen hBannerBorder(td.isEcoMode ? Color(160, 0, 230, 118) : Color(160, 0, 229, 255), 1.0f);
    DrawRoundedRect(g, heroBanner, 6, hBannerBg, &hBannerBorder);

    Font fHeroTitle(L"Segoe UI", 11.0f, FontStyleBold);
    Font fHeroDesc(L"Segoe UI", 8.8f, FontStyleRegular);
    if (td.isEcoMode) {
        g.DrawString(L"[ECO] РЕЖИМ 'ЕНЕРГОЗБЕРЕЖЕННЯ' АКТИВОВАНО", -1, &fHeroTitle, PointF(45, 84), &textGreen);
        g.DrawString(L"• Дисплей: 60 Hz (-2.5W)  |  CPU Boost: Вимкнено (~2.4 GHz)  |  Quiet Fan Mode\n• Windows: Енергоефективність  |  Усуває стрибки 65W та подовжує час роботи", -1, &fHeroDesc, PointF(45, 110), &textWhite);
    } else {
        g.DrawString(L"[MAX] РЕЖИМ 'МАКСИМАЛЬНА ПРОДУКТИВНІСТЬ' АКТИВОВАНО", -1, &fHeroTitle, PointF(45, 84), &textCyan);
        g.DrawString(L"• Дисплей: 144 Hz (Плавний)  |  CPU Boost: Увімкнено (до 4.4 GHz)  |  Balance Mode\n• Windows: Найвища продуктивність  |  Повна швидкість для важких задач та ігор", -1, &fHeroDesc, PointF(45, 110), &textWhite);
    }

    // 3. Telemetry Row (Y: 178, H: 135)
    DrawCard(g, 20, 178, 415, 135, L"АКУМУЛЯТОР ТА ЗДОРОВ'Я");
    DrawCard(g, 445, 178, 415, 135, L"СПОЖИВАННЯ ТА АВТОНОМНІСТЬ");

    Font fBigNumber(L"Segoe UI", 26.0f, FontStyleBold);
    Font fSubVal(L"Segoe UI", 9.0f, FontStyleRegular);

    std::wstring pctStr = std::to_wstring(td.battery.percent) + L"%";
    g.DrawString(pctStr.c_str(), -1, &fBigNumber, PointF(35, 208), &textGreen);

    Rect barBg(135, 221, 285, 14);
    SolidBrush bBarBg(Color(255, 33, 38, 45));
    DrawRoundedRect(g, barBg, 4, bBarBg);

    int fillW = (int)(285.0 * (td.battery.percent / 100.0));
    if (fillW > 285) fillW = 285;
    if (fillW < 4) fillW = 4;
    Rect barFill(135, 221, fillW, 14);
    SolidBrush bBarFill(td.isEcoMode ? Color(255, 0, 230, 118) : Color(255, 0, 229, 255));
    DrawRoundedRect(g, barFill, 4, bBarFill);

    std::wstringstream batDetails;
    batDetails << L"Ємність: " << td.battery.currentCapacityMWh << L" / " << td.battery.fullCapacityMWh << L" mWh (" 
               << std::fixed << std::setprecision(1) << td.battery.healthPercent << L"% здоров'я, " 
               << td.battery.cycleCount << L" циклів)";
    g.DrawString(batDetails.str().c_str(), -1, &fSubVal, PointF(35, 268), &textMuted);

    std::wstringstream pwrStr;
    if (td.battery.isDischarging) {
        pwrStr << L"-" << std::fixed << std::setprecision(1) << (td.battery.dischargeRateMW / 1000.0) << L" Вт";
        g.DrawString(pwrStr.str().c_str(), -1, &fBigNumber, PointF(460, 208), &textRed);
    } else {
        pwrStr << L"0.0 Вт (AC 220V)";
        g.DrawString(pwrStr.str().c_str(), -1, &fBigNumber, PointF(460, 208), &textCyan);
    }

    std::wstringstream etaStr;
    if (td.battery.isDischarging && td.battery.estimatedMinutes > 0) {
        int hrs = td.battery.estimatedMinutes / 60;
        int mins = td.battery.estimatedMinutes % 60;
        etaStr << L"Орієнтовний час роботи: " << hrs << L" год " << mins << L" хв";
    } else {
        etaStr << L"Схема Windows: " << td.winPower.activeSchemeName << L" (" << (td.battery.voltageMV / 1000.0) << L" V)";
    }
    g.DrawString(etaStr.str().c_str(), -1, &fSubVal, PointF(460, 268), &textMuted);

    // 4. Hardware Controls Grid (Y: 323, H: 232, W: 840)
    DrawCard(g, 20, 323, 840, 232, L"АПАРАТНІ НАЛАШТУВАННЯ ТА ПРОФІЛІ");

    Font fGroup(L"Segoe UI", 9.0f, FontStyleBold);
    g.DrawString(L"Частота екрана:", -1, &fGroup, PointF(30, 358), &textWhite);
    g.DrawString(L"Intel Turbo Boost (АКБ):", -1, &fGroup, PointF(450, 358), &textWhite);
    g.DrawString(L"Lenovo Thermal Mode (Fn+Q):", -1, &fGroup, PointF(30, 422), &textWhite);
    g.DrawString(L"Режим відеокарти (MUX Switch):", -1, &fGroup, PointF(450, 422), &textWhite);
    g.DrawString(L"Клавіатура, Автоматизація та Журнал:", -1, &fGroup, PointF(30, 486), &textWhite);

    // Update button states from memory cache
    bool autoSw = AppConfig::IsAutoSwitchEnabled();
    bool autoSt = AppConfig::IsAutoStartEnabled();

    for (auto& btn : s_buttons) {
        if (btn.id == 1) btn.isActive = td.isEcoMode;
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
    }

    // 5. NVIDIA Watchdog Card (Y: 565, H: 110, W: 840)
    DrawCard(g, 20, 565, 840, 110, L"NVIDIA GEFORCE RTX WATCHDOG ТА ПРОЦЕСИ");

    Font fGpuName(L"Segoe UI", 10.0f, FontStyleBold);
    Font fGpuStatus(L"Segoe UI", 8.8f, FontStyleRegular);

    std::wstring gpuHeader = td.gpu.model + L": ";
    if (td.gpu.isAwake) {
        gpuHeader += L"АКТИВНА (" + std::to_wstring((int)td.gpu.powerWatts) + L" W, " + std::to_wstring(td.gpu.temperatureC) + L" C)";
        g.DrawString(gpuHeader.c_str(), -1, &fGpuName, PointF(35, 590), &textRed);

        std::wstring procList = L"[!] Процеси на dGPU (" + std::to_wstring(td.gpu.processes.size()) + L"): ";
        if (td.gpu.processes.empty()) {
            procList = L"[!] Графічний контекст активний (зовнішній дисплей або системний драйвер)";
        } else {
            for (size_t i = 0; i < td.gpu.processes.size(); i++) {
                if (i > 0) procList += L",  ";
                procList += td.gpu.processes[i].name + L" (PID " + std::to_wstring(td.gpu.processes[i].pid) + L")";
                if (i >= 3 && td.gpu.processes.size() > 4) {
                    procList += L"  (+ще " + std::to_wstring(td.gpu.processes.size() - 4) + L")";
                    break;
                }
            }
        }
        RectF rProc(35.0f, 614.0f, 600.0f, 48.0f);
        StringFormat sfWrap;
        sfWrap.SetTrimming(StringTrimmingEllipsisWord);
        g.DrawString(procList.c_str(), -1, &fGpuStatus, rProc, &sfWrap, &textWhite);
    } else {
        gpuHeader += L"СПИТЬ (D3Cold, 0 W — Максимальна економія)";
        g.DrawString(gpuHeader.c_str(), -1, &fGpuName, PointF(35, 590), &textGreen);
        g.DrawString(L"[OK] Жоден процес не тримає dGPU. Відеокарта знеструмлена, повний C-стан сну.", -1, &fGpuStatus, PointF(35, 616), &textMuted);
    }

    // Draw All Buttons
    Font fBtn(L"Segoe UI", 8.8f, FontStyleBold);
    Font fHeroBtn(L"Segoe UI", 10.0f, FontStyleBold);

    for (const auto& btn : s_buttons) {
        SolidBrush bBtnBg(btn.isActive ? btn.activeColor : (btn.isHovered ? btn.hoverColor : btn.normalColor));
        Pen pBtnBorder(btn.isActive ? btn.activeColor : (btn.isHovered ? Color(255, 80, 90, 105) : Color(255, 48, 54, 61)), 1.0f);

        DrawRoundedRect(g, btn.bounds, 6, bBtnBg, &pBtnBorder);

        SolidBrush bBtnText(btn.isActive ? Color(255, 10, 15, 20) : Color(255, 240, 246, 252));
        if (btn.id == 15) bBtnText.SetColor(Color(255, 255, 120, 120));
        if (btn.id == 18) bBtnText.SetColor(Color(255, 234, 179, 8));

        if (btn.id == 1 || btn.id == 19) {
            g.DrawString(btn.text.c_str(), -1, &fHeroBtn, RectF((REAL)btn.bounds.X, (REAL)btn.bounds.Y, (REAL)btn.bounds.Width, (REAL)btn.bounds.Height), &sfCenter, &bBtnText);
        } else {
            g.DrawString(btn.text.c_str(), -1, &fBtn, RectF((REAL)btn.bounds.X, (REAL)btn.bounds.Y, (REAL)btn.bounds.Width, (REAL)btn.bounds.Height), &sfCenter, &bBtnText);
        }
    }

    // Footer
    Font fFooter(L"Segoe UI", 8.5f, FontStyleRegular);
    g.DrawString(L"Підказка: ЛКМ по значку в треї миттєво перемикає Еко/Макс, ПКМ відкриває повне меню.", -1, &fFooter, PointF(25, 690), &textMuted);

    BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

void GuiWindow::HandleClick(HWND hWnd, int x, int y) {
    for (const auto& btn : s_buttons) {
        if (btn.bounds.Contains(x, y)) {
            switch (btn.id) {
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
                    TrayManager::ShowBalloon(L"Режим MUX змінено", L"Встановлено гібридний режим (Hybrid). Зміни набудуть чинності після перезавантаження комп'ютера.");
                    break;
                case 10: // MUX iGPU Only
                    LenovoWmi::SetMuxMode(GpuMuxMode::iGpuOnly);
                    TelemetryThread::SetMuxMode(GpuMuxMode::iGpuOnly);
                    TrayManager::ShowBalloon(L"Режим MUX змінено", L"Встановлено тільки iGPU. Зміни набудуть чинності після перезавантаження комп'ютера.");
                    break;
                case 11: // MUX dGPU Only
                    LenovoWmi::SetMuxMode(GpuMuxMode::dGpuOnly);
                    TelemetryThread::SetMuxMode(GpuMuxMode::dGpuOnly);
                    TrayManager::ShowBalloon(L"Режим MUX змінено", L"Встановлено тільки dGPU. Зміни набудуть чинності після перезавантаження комп'ютера.");
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
                    TrayManager::ShowBalloon(L"EnergyControl у треї", L"Програма продовжує контролювати енергоспоживання у фоні.");
                    break;
                case 17: // Close
                    ShowWindow(hWnd, SW_HIDE);
                    break;
                case 18: // Open Log
                    Logger::OpenLogFile();
                    break;
            }
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
    }
}

void GuiWindow::HandleMouseMove(HWND hWnd, int x, int y) {
    int newHover = -1;
    for (size_t i = 0; i < s_buttons.size(); i++) {
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
