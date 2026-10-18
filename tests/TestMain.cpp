#include "TestRunner.hpp"
#include "../include/core/AppConfig.hpp"
#include "../include/core/Logger.hpp"
#include "../include/core/CrashHandler.hpp"
#include "../include/core/UpdateManager.hpp"
#include "../include/hardware/BatteryController.hpp"
#include "../include/hardware/CpuController.hpp"
#include "../include/hardware/WindowsPowerController.hpp"
#include "../include/hardware/LenovoWmi.hpp"
#include "../include/hardware/GpuController.hpp"
#include "../include/hardware/DisplayController.hpp"
#include "../include/core/SystemInfo.hpp"
#include "../include/core/TelemetryThread.hpp"
#include "../include/gui/GuiWindow.hpp"
#include "../include/gui/TrayManager.hpp"


#include <fstream>


// ======================================================================
// 1. AppConfig & Versioning Tests
// ======================================================================
TEST_CASE(AppConfig_VersionNotEmpty) {
    std::wstring ver = AppConfig::GetAppVersion();
    ASSERT_TRUE(!ver.empty());
    ASSERT_TRUE(ver.find(L"alpha") != std::wstring::npos);
}

TEST_CASE(AppConfig_CalVerNamingStandard) {
    std::wstring ver = AppConfig::GetAppVersion();
    ASSERT_TRUE(ver == L"2026.10.4-alpha");
    ASSERT_TRUE(AppConfig::GetAppYearMonth() == L"2026.10");
    ASSERT_TRUE(AppConfig::GetAppStage() == L"Alpha");
}

TEST_CASE(AppConfig_StageIsAlpha) {
    std::wstring stage = AppConfig::GetAppStage();
    ASSERT_TRUE(!stage.empty());
    ASSERT_TRUE(stage.find(L"Alpha") != std::wstring::npos);
}

TEST_CASE(AppConfig_AutoSwitchToggle) {
    bool original = AppConfig::IsAutoSwitchEnabled();
    AppConfig::SetAutoSwitchEnabled(!original);
    ASSERT_EQ(AppConfig::IsAutoSwitchEnabled(), !original);
    // Restore
    AppConfig::SetAutoSwitchEnabled(original);
    ASSERT_EQ(AppConfig::IsAutoSwitchEnabled(), original);
}

// ======================================================================
// 2. Logger Tests
// ======================================================================
TEST_CASE(Logger_FileCreationAndWriting) {
    Logger::Init();
    std::wstring logPath = Logger::GetLogFilePath();
    ASSERT_TRUE(!logPath.empty());

    Logger::Info(L"UnitTest", L"Executing test entry in automated test suite");
    Logger::Warn(L"UnitTest", L"Testing warning level log output");
    Logger::Error(L"UnitTest", L"Testing error level log output");
    Logger::Debug(L"UnitTest", L"Testing debug level log output");

    // Verify file exists
    DWORD attr = GetFileAttributesW(logPath.c_str());
    ASSERT_TRUE(attr != INVALID_FILE_ATTRIBUTES);
}

// ======================================================================
// 3. Battery Snapshot & Calculation Tests
// ======================================================================
TEST_CASE(Battery_SanityChecks) {
    BatterySnapshot bat = BatteryController::Query();
    // Percent must be in [0, 100]
    ASSERT_TRUE(bat.percent >= 0 && bat.percent <= 100);
    // Capacity must be non-negative
    ASSERT_TRUE(bat.fullCapacityMWh >= 0);
    ASSERT_TRUE(bat.currentCapacityMWh >= 0);
    // Health must be non-negative
    ASSERT_TRUE(bat.healthPercent >= 0.0);
}

TEST_CASE(Battery_DischargeRateCalculation) {
    BatterySnapshot bat;
    bat.isAcOnline = false;
    bat.isDischarging = true;
    bat.dischargeRateMW = 15000; // 15W
    bat.currentCapacityMWh = 45000; // 45 Wh

    // Estimated minutes = (45000 / 15000) * 60 = 180 min (3 hours)
    double hrs = (double)bat.currentCapacityMWh / (double)bat.dischargeRateMW;
    int estMins = (int)(hrs * 60.0);
    ASSERT_EQ(estMins, 180);

    // Test zero discharge rate does not crash or divide by zero
    bat.dischargeRateMW = 0;
    int safeMins = (bat.dischargeRateMW > 0) ? (int)(((double)bat.currentCapacityMWh / bat.dischargeRateMW) * 60.0) : 0;
    ASSERT_EQ(safeMins, 0);
}

// ======================================================================
// 4. CPU Controller Enum & State Tests
// ======================================================================
TEST_CASE(CpuController_StringConversions) {
    ASSERT_TRUE(CpuController::ToString(CpuBoostMode::Disabled) == L"Вимкнено (~2.4 GHz)");
    ASSERT_TRUE(CpuController::ToString(CpuBoostMode::Enabled) == L"Увімкнено (Стандартний)");
    ASSERT_TRUE(CpuController::ToString(CpuBoostMode::Aggressive) == L"Агресивний (до 4.4 GHz)");
    ASSERT_TRUE(CpuController::ToString(CpuBoostMode::Efficient) == L"Ефективний");
}

TEST_CASE(CpuController_GetBoostModeDoesNotCrash) {
    CpuBoostMode modeAC = CpuController::GetBoostMode(false);
    CpuBoostMode modeDC = CpuController::GetBoostMode(true);
    ASSERT_TRUE(modeAC == CpuBoostMode::Disabled || modeAC == CpuBoostMode::Enabled || modeAC == CpuBoostMode::Aggressive || modeAC == CpuBoostMode::Efficient || modeAC == CpuBoostMode::Unknown);
    ASSERT_TRUE(modeDC == CpuBoostMode::Disabled || modeDC == CpuBoostMode::Enabled || modeDC == CpuBoostMode::Aggressive || modeDC == CpuBoostMode::Efficient || modeDC == CpuBoostMode::Unknown);
}

// ======================================================================
// 5. Windows Power Controller Tests
// ======================================================================
TEST_CASE(WindowsPower_ModeToString) {
    ASSERT_TRUE(WindowsPowerController::ModeToString(WindowsPowerMode::PowerEfficiency) == L"Енергоефективність");
    ASSERT_TRUE(WindowsPowerController::ModeToString(WindowsPowerMode::Balanced) == L"Збалансований");
    ASSERT_TRUE(WindowsPowerController::ModeToString(WindowsPowerMode::BestPerformance) == L"Найвища продуктивність");
}

TEST_CASE(WindowsPower_QueryReturnsValidStructure) {
    WindowsPowerStatus st = WindowsPowerController::Query();
    ASSERT_TRUE(!st.activeSchemeName.empty());
}

TEST_CASE(WindowsPower_EnergySaverToggle) {
    bool newState = WindowsPowerController::ToggleEnergySaver();
    bool restoredState = WindowsPowerController::ToggleEnergySaver();
    ASSERT_TRUE(restoredState != newState);
}

// ======================================================================
// 6. Lenovo WMI Enum & Helper Tests
// ======================================================================
TEST_CASE(LenovoWmi_ThermalModeToString) {
    ASSERT_TRUE(LenovoWmi::ThermalToString(ThermalMode::Quiet) == L"Quiet (Синій Fn+Q)");
    ASSERT_TRUE(LenovoWmi::ThermalToString(ThermalMode::Balance) == L"Balance (Білий)");
    ASSERT_TRUE(LenovoWmi::ThermalToString(ThermalMode::Performance) == L"Performance (Червоний)");
}

TEST_CASE(LenovoWmi_MuxModeToString) {
    ASSERT_TRUE(LenovoWmi::MuxToString(GpuMuxMode::Hybrid) == L"Hybrid (Динамічний)");
    ASSERT_TRUE(LenovoWmi::MuxToString(GpuMuxMode::iGpuOnly) == L"iGPU Only (Енергозбереження)");
    ASSERT_TRUE(LenovoWmi::MuxToString(GpuMuxMode::dGpuOnly) == L"dGPU Only (Ігровий MUX)");
}

TEST_CASE(LenovoWmi_KeyboardLightToString) {
    ASSERT_TRUE(LenovoWmi::KbdToString(KbdBacklight::Off) == L"Вимкнено");
    ASSERT_TRUE(LenovoWmi::KbdToString(KbdBacklight::Low) == L"Низька");
    ASSERT_TRUE(LenovoWmi::KbdToString(KbdBacklight::High) == L"Висока");
}

TEST_CASE(LenovoWmi_KeyboardLight_QueryDirect) {
    KbdBacklight light = LenovoWmi::GetKeyboardLight();
    ASSERT_TRUE(light == KbdBacklight::Off || light == KbdBacklight::Low || light == KbdBacklight::High);
}

// ======================================================================
// 7. GPU Watchdog Tests
// ======================================================================
TEST_CASE(GpuWatchdog_QueryReturnsValidModel) {
    GpuSnapshot gpu = GpuController::Query();
    ASSERT_TRUE(!gpu.model.empty());
    // Temperature must be reasonable (e.g. 0 to 120 C)
    ASSERT_TRUE(gpu.temperatureC >= 0 && gpu.temperatureC < 120);
    ASSERT_TRUE(gpu.powerWatts >= 0.0 && gpu.powerWatts < 300.0);
}

// ======================================================================
// 8. Crash Handler Tests
// ======================================================================
TEST_CASE(CrashHandler_ExceptionDescriptions) {
    std::wstring descAv = CrashHandler::GetExceptionDescription(EXCEPTION_ACCESS_VIOLATION);
    ASSERT_TRUE(descAv.find(L"0xC0000005") != std::wstring::npos);

    std::wstring descDiv0 = CrashHandler::GetExceptionDescription(EXCEPTION_INT_DIVIDE_BY_ZERO);
    ASSERT_TRUE(descDiv0.find(L"0xC0000094") != std::wstring::npos);

    std::wstring descStack = CrashHandler::GetExceptionDescription(EXCEPTION_STACK_OVERFLOW);
    ASSERT_TRUE(descStack.find(L"0xC00000FD") != std::wstring::npos);
}

// ======================================================================
// 9. In-App Update Manager & Version Control Tests
// ======================================================================
TEST_CASE(UpdateManager_ParseVersionLogic) {
    int y = 0, m = 0, p = 0, s = 0;
    ASSERT_TRUE(UpdateManager::ParseVersion(L"alpha-2026.10", y, m, p, s));
    ASSERT_EQ(y, 2026);
    ASSERT_EQ(m, 10);
    ASSERT_EQ(p, 0);
    ASSERT_EQ(s, 1); // Alpha

    ASSERT_TRUE(UpdateManager::ParseVersion(L"beta-2026.11.2", y, m, p, s));
    ASSERT_EQ(y, 2026);
    ASSERT_EQ(m, 11);
    ASSERT_EQ(p, 2);
    ASSERT_EQ(s, 2); // Beta

    ASSERT_TRUE(UpdateManager::ParseVersion(L"v2027.01", y, m, p, s));
    ASSERT_EQ(y, 2027);
    ASSERT_EQ(m, 1);
    ASSERT_EQ(p, 0);
    ASSERT_EQ(s, 4); // Stable
}

TEST_CASE(UpdateManager_IsNewerVersionComparison) {
    // Newer month in same year
    ASSERT_TRUE(UpdateManager::IsNewerVersion(L"alpha-2026.10", L"alpha-2026.11"));
    ASSERT_FALSE(UpdateManager::IsNewerVersion(L"alpha-2026.11", L"alpha-2026.10"));

    // Newer year
    ASSERT_TRUE(UpdateManager::IsNewerVersion(L"alpha-2026.12", L"alpha-2027.01"));

    // Newer patch
    ASSERT_TRUE(UpdateManager::IsNewerVersion(L"alpha-2026.10", L"alpha-2026.10.1"));
    ASSERT_FALSE(UpdateManager::IsNewerVersion(L"alpha-2026.10.1", L"alpha-2026.10"));

    // Higher stage (beta > alpha)
    ASSERT_TRUE(UpdateManager::IsNewerVersion(L"alpha-2026.10", L"beta-2026.10"));
    ASSERT_FALSE(UpdateManager::IsNewerVersion(L"beta-2026.10", L"alpha-2026.10"));

    // Same version -> not newer
    ASSERT_FALSE(UpdateManager::IsNewerVersion(L"alpha-2026.10", L"alpha-2026.10"));
}

TEST_CASE(UpdateManager_StatusDisplayInitial) {
    UpdateManager::Init();
    std::wstring display = UpdateManager::GetStatusDisplayString();
    ASSERT_TRUE(!display.empty());
    ASSERT_TRUE(display.find(AppConfig::GetAppVersion()) != std::wstring::npos);
}

// ======================================================================
// 10. Display Wattage, Process Hierarchy, and Top Processes Tests
// ======================================================================
TEST_CASE(Display_DynamicWattageCalculation) {
    float w60_min = DisplayController::CalculateDisplayWatts(60, 0);
    float w60_max = DisplayController::CalculateDisplayWatts(60, 100);
    float w144_max = DisplayController::CalculateDisplayWatts(144, 100);

    // 60Hz 0% brightness should be lower than 60Hz 100%
    ASSERT_TRUE(w60_min < w60_max);
    // 144Hz 100% brightness should consume more than 60Hz 100%
    ASSERT_TRUE(w144_max > w60_max);
    // Clamping checks
    ASSERT_TRUE(DisplayController::CalculateDisplayWatts(60, -50) == w60_min);
    ASSERT_TRUE(DisplayController::CalculateDisplayWatts(60, 150) == w60_max);
}

TEST_CASE(SystemInfo_TopMemoryProcesses) {
    auto procs = SystemInfo::GetTopMemoryProcesses(5);
    ASSERT_TRUE(!procs.empty());
    ASSERT_TRUE(procs.size() <= 5);
    // Ensure sorted in descending order of memory
    for (size_t i = 1; i < procs.size(); i++) {
        ASSERT_TRUE(procs[i - 1].memMB >= procs[i].memMB);
    }
}

TEST_CASE(GpuController_SafeTerminationChecks) {
    // Attempting to terminate PID 0 or current process must be safely rejected
    ASSERT_FALSE(GpuController::TerminateProcessByPid(0));
    ASSERT_FALSE(GpuController::TerminateProcessByPid(GetCurrentProcessId()));
}


// ======================================================================
// 11. UI Text, Symbols, and Localization Integrity Tests
// ======================================================================
TEST_CASE(Ui_GuiButtonsAndSymbolsIntegrity) {
    GuiWindow::InitButtons();
    const auto& buttons = GuiWindow::GetButtons();
    ASSERT_TRUE(!buttons.empty());

    bool foundMin = false;
    bool foundClose = false;
    bool foundUpdate = false;
    bool foundEco = false;
    bool foundMax = false;
    bool foundTabDash = false;
    bool foundTabGraphs = false;
    bool foundTabCpu = false;
    bool foundTabIGpu = false;
    bool foundTabDGpu = false;

    for (const auto& btn : buttons) {
        // No button text should contain replacement character \uFFFD or mojibake signatures
        for (wchar_t ch : btn.text) {
            ASSERT_TRUE(ch != 0xFFFD);
        }
        // Ensure no CP1251 double-encoded characters
        ASSERT_TRUE(btn.text.find(L"Р—") == std::wstring::npos);
        ASSERT_TRUE(btn.text.find(L"вЂ") == std::wstring::npos);
        ASSERT_TRUE(btn.text.find(L"вњ") == std::wstring::npos);

        if (btn.id == 16) {
            foundMin = true;
            ASSERT_TRUE(btn.text == L"—");
        } else if (btn.id == 17) {
            foundClose = true;
            ASSERT_TRUE(btn.text == L"✕");
        } else if (btn.id == 20) {
            foundUpdate = true;
            ASSERT_TRUE(btn.text.find(L"Оновлення") != std::wstring::npos);
        } else if (btn.id == 1) {
            foundEco = true;
            ASSERT_TRUE(btn.text.find(L"ECO") != std::wstring::npos);
            ASSERT_TRUE(btn.text.find(L"60Hz") != std::wstring::npos);
        } else if (btn.id == 19) {
            foundMax = true;
            ASSERT_TRUE(btn.text.find(L"MAX") != std::wstring::npos);
            ASSERT_TRUE(btn.text.find(L"144Hz") != std::wstring::npos);
        } else if (btn.id == 100) {
            foundTabDash = true;
            ASSERT_TRUE(btn.text.find(L"Головна") != std::wstring::npos);
            ASSERT_TRUE(btn.text.find(L"⚡") != std::wstring::npos);
        } else if (btn.id == 101) {
            foundTabGraphs = true;
            ASSERT_TRUE(btn.text.find(L"Графіки") != std::wstring::npos);
            ASSERT_TRUE(btn.text.find(L"◈") != std::wstring::npos);
        } else if (btn.id == 105) {
            foundTabCpu = true;
            ASSERT_TRUE(btn.text.find(L"Процеси CPU") != std::wstring::npos);
            ASSERT_TRUE(btn.text.find(L"■") != std::wstring::npos);
        } else if (btn.id == 106) {
            foundTabIGpu = true;
            ASSERT_TRUE(btn.text.find(L"Процеси iGPU") != std::wstring::npos);
            ASSERT_TRUE(btn.text.find(L"◇") != std::wstring::npos);
        } else if (btn.id == 102) {
            foundTabDGpu = true;
            ASSERT_TRUE(btn.text.find(L"Графіка dGPU") != std::wstring::npos);
            ASSERT_TRUE(btn.text.find(L"▲") != std::wstring::npos);
        }
    }

    ASSERT_TRUE(foundMin);
    ASSERT_TRUE(foundClose);
    ASSERT_TRUE(foundUpdate);
    ASSERT_TRUE(foundEco);
    ASSERT_TRUE(foundMax);
    ASSERT_TRUE(foundTabDash);
    ASSERT_TRUE(foundTabGraphs);
    ASSERT_TRUE(foundTabCpu);
    ASSERT_TRUE(foundTabIGpu);
    ASSERT_TRUE(foundTabDGpu);
}

TEST_CASE(SystemInfo_CpuAndIGpuProcesses) {
    std::wstring igpu = SystemInfo::DetectIGpuName();
    ASSERT_TRUE(!igpu.empty());

    auto cpuProcs = SystemInfo::GetTopCpuProcesses(5, 15.0f);
    ASSERT_TRUE(!cpuProcs.empty());
    for (const auto& p : cpuProcs) {
        ASSERT_TRUE(p.pid > 0);
        ASSERT_TRUE(!p.name.empty());
    }

    std::vector<DWORD> dPids;
    auto igpuProcs = SystemInfo::GetIGpuProcesses(dPids, 5);
    // В ізольованому сеансі може не бути доступних віконних процесів.
    ASSERT_TRUE(igpuProcs.size() <= 5);
    ASSERT_TRUE(SystemInfo::GetIGpuProcesses(dPids, 0).empty());
    for (const auto& p : igpuProcs) {
        ASSERT_TRUE(p.pid > 0);
        ASSERT_TRUE(!p.name.empty());
        ASSERT_TRUE(!p.role.empty());
    }
}

TEST_CASE(Ui_HardwareStringsLocalizationIntegrity) {
    // 1. CPU Boost
    ASSERT_TRUE(CpuController::ToString(CpuBoostMode::Disabled) == L"Вимкнено (~2.4 GHz)");
    ASSERT_TRUE(CpuController::ToString(CpuBoostMode::Enabled) == L"Увімкнено (Стандартний)");
    ASSERT_TRUE(CpuController::ToString(CpuBoostMode::Aggressive) == L"Агресивний (до 4.4 GHz)");
    ASSERT_TRUE(CpuController::ToString(CpuBoostMode::Efficient) == L"Ефективний");

    // 2. Windows Power
    ASSERT_TRUE(WindowsPowerController::ModeToString(WindowsPowerMode::PowerEfficiency) == L"Енергоефективність");
    ASSERT_TRUE(WindowsPowerController::ModeToString(WindowsPowerMode::Balanced) == L"Збалансований");
    ASSERT_TRUE(WindowsPowerController::ModeToString(WindowsPowerMode::BestPerformance) == L"Найвища продуктивність");

    // 3. Lenovo Thermal Mode
    ASSERT_TRUE(LenovoWmi::ThermalToString(ThermalMode::Quiet) == L"Quiet (Синій Fn+Q)");
    ASSERT_TRUE(LenovoWmi::ThermalToString(ThermalMode::Balance) == L"Balance (Білий)");
    ASSERT_TRUE(LenovoWmi::ThermalToString(ThermalMode::Performance) == L"Performance (Червоний)");

    // 4. Lenovo MUX Mode
    ASSERT_TRUE(LenovoWmi::MuxToString(GpuMuxMode::Hybrid) == L"Hybrid (Динамічний)");
    ASSERT_TRUE(LenovoWmi::MuxToString(GpuMuxMode::iGpuOnly) == L"iGPU Only (Енергозбереження)");
    ASSERT_TRUE(LenovoWmi::MuxToString(GpuMuxMode::dGpuOnly) == L"dGPU Only (Ігровий MUX)");

    // 5. Lenovo Keyboard Light
    ASSERT_TRUE(LenovoWmi::KbdToString(KbdBacklight::Off) == L"Вимкнено");
    ASSERT_TRUE(LenovoWmi::KbdToString(KbdBacklight::Low) == L"Низька");
    ASSERT_TRUE(LenovoWmi::KbdToString(KbdBacklight::High) == L"Висока");

    // 6. Crash Handler Descriptions
    std::wstring desc = CrashHandler::GetExceptionDescription(EXCEPTION_ACCESS_VIOLATION);
    ASSERT_TRUE(desc.find(L"Неприпустимий доступ до пам'яті") != std::wstring::npos);
}

TEST_CASE(Ui_SourceFilesUtf8BomIntegrity) {
    const char* filesToCheck[] = {
        "main.cpp",
        "src/gui/GuiWindow.cpp",
        "src/gui/TrayManager.cpp",
        "src/core/CrashHandler.cpp",
        "src/core/UpdateManager.cpp",
        "src/core/TelemetryThread.cpp",
        "src/hardware/CpuController.cpp",
        "src/hardware/LenovoWmi.cpp",
        "src/hardware/WindowsPowerController.cpp",
        "include/gui/GuiWindow.hpp",
        "include/gui/TrayManager.hpp"
    };

    for (const char* path : filesToCheck) {
        std::ifstream f(path, std::ios::binary);
        ASSERT_TRUE(f.is_open());

        unsigned char bom[3] = {0};
        f.read((char*)bom, 3);
        // Must start with UTF-8 BOM
        ASSERT_EQ(bom[0], 0xEF);
        ASSERT_EQ(bom[1], 0xBB);
        ASSERT_EQ(bom[2], 0xBF);

        // Read remaining content and check no duplicate BOM or CP1251 mojibake markers
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        ASSERT_TRUE(content.rfind("\xEF\xBB\xBF", 0) != 0);
        // Ensure no "Р—" or "вњ" in bytes
        ASSERT_TRUE(content.find("\xD0\xA0\xE2\x80\x94") == std::string::npos);
        ASSERT_TRUE(content.find("\xD0\xB2\xD1\x9A") == std::string::npos);
    }
}

// Перевіряємо фактичні координати натискань і малювання за різних масштабів.
TEST_CASE(Ui_ScaledNavigationAndRendering) {
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token;
    ASSERT_EQ(Gdiplus::GdiplusStartup(&token, &input, nullptr), Gdiplus::Ok);
    TelemetryData sample;
    sample.battery.isAcOnline = true;
    sample.battery.isConnected = true;
    sample.battery.percent = 100;
    sample.battery.voltageMV = 15840;
    sample.battery.fullCapacityMWh = 80000;
    sample.battery.designCapacityMWh = 80000;
    sample.battery.currentCapacityMWh = 80000;
    sample.system.cpuName = L"Intel Core i7-12700H — процесор із довгою назвою";
    sample.iGpuName = L"Intel Iris Xe Graphics — інтегрований графічний адаптер";
    sample.gpu.model = L"NVIDIA GeForce RTX 3070 Ti Laptop GPU";
    sample.gpu.isAwake = true;
    sample.gpu.powerWatts = 35.5f;
    sample.cpuProcesses.push_back({12345, L"Дуже довга назва процесу для перевірки меж таблиці.exe", 16384, 23.5f, 15.7f, L"Довгий опис призначення процесу"});
    sample.iGpuProcesses = sample.cpuProcesses;
    CreateDirectoryW(L"build", nullptr);
    CreateDirectoryW(L"build/ui-previews", nullptr);
    for (int percent : {100, 125, 150, 200}) {
        int width = 880 * percent / 100;
        int height = 720 * percent / 100;
        HWND window = CreateWindowExW(0, L"STATIC", L"", WS_POPUP,
                                      0, 0, width, height, nullptr, nullptr, nullptr, nullptr);
        ASSERT_TRUE(window != nullptr);
        GuiWindow::InitButtons();
        HDC dc = GetDC(window);
        HDC memory = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
        HGDIOBJ previous = SelectObject(memory, bitmap);
        for (int tab = 0; tab < 5; ++tab) {
            int x = (40 + 167 * tab) * percent / 100;
            int y = 60 * percent / 100;
            ASSERT_TRUE(GuiWindow::HandleClick(window, x, y));
            ASSERT_EQ((int)GuiWindow::GetTab(), tab);
            GuiWindow::Paint(window, memory, sample);
            ASSERT_EQ(GetPixel(memory, 5, 5), RGB(13, 17, 23));
            // Пояснення та значення графіків не повинні потрапляти у проміжки між картками.
            if (tab == 1) {
                for (int y = 410; y < 655; ++y) {
                    ASSERT_EQ(GetPixel(memory, 440 * percent / 100, y * percent / 100), RGB(13, 17, 23));
                    ASSERT_EQ(GetPixel(memory, 867 * percent / 100, y * percent / 100), RGB(13, 17, 23));
                }
            }
            if (percent == 100 || percent == 150) {
                Gdiplus::Bitmap preview(bitmap, nullptr);
                CLSID png = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
                std::wstring path = L"build/ui-previews/tab-" + std::to_wstring(tab) + L"-" + std::to_wstring(percent) + L".png";
                ASSERT_EQ(preview.Save(path.c_str(), &png, nullptr), Gdiplus::Ok);
            }
        }
        GuiWindow::HandleMouseMove(window, 40 * percent / 100, 60 * percent / 100);
        GuiWindow::HandleMouseMove(window, -1, -1);
        for (const auto& button : GuiWindow::GetButtons()) ASSERT_FALSE(button.isHovered);
        SelectObject(memory, previous);
        DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(window, dc);
        DestroyWindow(window);
    }
    GuiWindow::SetTab(GuiTab::Dashboard);
    Gdiplus::GdiplusShutdown(token);
}

int main() {

    return UnitTest::RunAll();
}

