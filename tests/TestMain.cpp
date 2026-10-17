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
    ASSERT_TRUE(ver == L"alpha-2026.10");
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
    ASSERT_TRUE(display.find(L"alpha-2026.10") != std::wstring::npos);
}

int main() {
    return UnitTest::RunAll();
}
