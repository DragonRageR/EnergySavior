#include "../../include/core/TelemetryThread.hpp"
#include "../../include/core/AppConfig.hpp"
#include "../../include/core/Logger.hpp"
#include "../../include/gui/TrayManager.hpp"
#include "../../include/hardware/WindowsPowerController.hpp"
#include <thread>

void TelemetryThread::Start(HWND notifyHwnd) {
    if (s_running) return;
    s_notifyHwnd = notifyHwnd;
    s_running = true;

    Logger::Init();

    // Initial hardware detection
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_data.system = SystemInfo::Detect();
        s_data.isElevated = AppConfig::IsElevated();
        s_data.winPower = WindowsPowerController::Query();

        Logger::Info(L"TelemetryThread", L"Telemetry engine initialized. Elevated: " + 
                     std::wstring(s_data.isElevated ? L"YES" : L"NO"));
        Logger::Info(L"SystemInfo", L"Detected: " + s_data.system.manufacturer + L" " + s_data.system.model + 
                     L" | BIOS: " + s_data.system.biosVersion + L" | CPU: " + s_data.system.cpuName);
        if (s_data.system.hasExternalDisplay) {
            Logger::Info(L"Display", L"External monitors detected: " + std::to_wstring(s_data.system.externalDisplayCount));
        }
    }

    std::thread(WorkerLoop).detach();
}

void TelemetryThread::Stop() {
    Logger::Info(L"TelemetryThread", L"Stopping telemetry worker...");
    s_running = false;
}

TelemetryData TelemetryThread::GetData() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_data;
}

bool TelemetryThread::CanNotify() {
    DWORD now = GetTickCount();
    return (now - s_lastNotificationTime > 5000);
}

void TelemetryThread::MarkNotificationSent() {
    s_lastNotificationTime = GetTickCount();
}

void TelemetryThread::ApplyEcoProfile() {
    if (s_isSwitching.exchange(true)) return; // Debounce rapid clicks

    Logger::Info(L"Profile", L"Applying ECO profile: 60Hz, Boost OFF, Quiet Mode, Windows Energy Saver");

    DisplayController::SetRefreshRate(60);
    CpuController::ApplyEcoProfile();
    LenovoWmi::SetThermalMode(ThermalMode::Quiet);
    WindowsPowerController::ApplyEcoProfile();

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_data.refreshRate = 60;
        s_data.cpuBoost = CpuBoostMode::Disabled;
        s_data.thermalMode = ThermalMode::Quiet;
        s_data.isEcoMode = true;
        s_data.winPower = WindowsPowerController::Query();
    }

    TrayManager::UpdateIcon(true);
    if (CanNotify()) {
        MarkNotificationSent();
        TrayManager::ShowBalloon(L"[ECO] Режим Енергозбереження", 
                                 L"• Екран: 60 Hz (-2.5W)\n• CPU Boost: Вимкнено (~2.4 GHz)\n• Охолодження: Quiet\n• Windows: Енергоефективність");
    }

    if (s_notifyHwnd) PostMessage(s_notifyHwnd, WM_USER + 2, 0, 0);

    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        s_isSwitching = false;
    }).detach();
}

void TelemetryThread::ApplyPerformanceProfile() {
    if (s_isSwitching.exchange(true)) return; // Debounce rapid clicks

    Logger::Info(L"Profile", L"Applying PERFORMANCE profile: 144Hz, Boost ON, Balance Mode, Windows Best Performance");

    DisplayController::SetRefreshRate(144);
    CpuController::ApplyPerformanceProfile();
    LenovoWmi::SetThermalMode(ThermalMode::Balance);
    WindowsPowerController::ApplyPerformanceProfile();

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_data.refreshRate = 144;
        s_data.cpuBoost = CpuBoostMode::Aggressive;
        s_data.thermalMode = ThermalMode::Balance;
        s_data.isEcoMode = false;
        s_data.winPower = WindowsPowerController::Query();
    }

    TrayManager::UpdateIcon(false);
    if (CanNotify()) {
        MarkNotificationSent();
        TrayManager::ShowBalloon(L"[MAX] Режим Продуктивність", 
                                 L"• Екран: 144 Hz (Плавний)\n• CPU Boost: Увімкнено (до 4.4 GHz)\n• Охолодження: Balance\n• Windows: Найвища продуктивність");
    }

    if (s_notifyHwnd) PostMessage(s_notifyHwnd, WM_USER + 2, 0, 0);

    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        s_isSwitching = false;
    }).detach();
}

void TelemetryThread::ToggleMode() {
    bool eco = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        eco = s_data.isEcoMode;
    }
    if (eco) ApplyPerformanceProfile();
    else ApplyEcoProfile();
}

void TelemetryThread::SetRefreshRate(DWORD hz) {
    Logger::Info(L"Display", L"Manual refresh rate switch to " + std::to_wstring(hz) + L" Hz");
    std::lock_guard<std::mutex> lock(s_mutex);
    s_data.refreshRate = hz;
}

void TelemetryThread::SetCpuBoost(CpuBoostMode mode) {
    Logger::Info(L"CPU", L"Manual CPU Boost mode switch: " + CpuController::ToString(mode));
    std::lock_guard<std::mutex> lock(s_mutex);
    s_data.cpuBoost = mode;
}

void TelemetryThread::SetThermalMode(ThermalMode mode) {
    Logger::Info(L"Thermal", L"Manual thermal mode switch: " + LenovoWmi::ThermalToString(mode));
    std::lock_guard<std::mutex> lock(s_mutex);
    s_data.thermalMode = mode;
}

void TelemetryThread::SetMuxMode(GpuMuxMode mode) {
    Logger::Info(L"MUX", L"Manual MUX Switch mode: " + LenovoWmi::MuxToString(mode));
    std::lock_guard<std::mutex> lock(s_mutex);
    s_data.muxMode = mode;
}

void TelemetryThread::SetKeyboardLight(KbdBacklight light) {
    Logger::Info(L"Keyboard", L"Keyboard backlight set to: " + LenovoWmi::KbdToString(light));
    std::lock_guard<std::mutex> lock(s_mutex);
    s_data.kbdLight = light;
}

void TelemetryThread::WorkerLoop() {
    bool lastAc = true;
    bool lastWinSaver = false;

    while (s_running) {
        BatterySnapshot bat = BatteryController::Query();
        DWORD hz = DisplayController::GetCurrentRefreshRate();
        CpuBoostMode boost = CpuController::GetBoostMode(!bat.isAcOnline);
        GpuSnapshot gpu = GpuController::Query();
        ThermalMode therm = LenovoWmi::GetThermalMode();
        GpuMuxMode mux = LenovoWmi::GetMuxMode();
        KbdBacklight kbd = LenovoWmi::GetKeyboardLight();
        WindowsPowerStatus winPow = WindowsPowerController::Query();
        // Auto-switch with debouncing
        if (AppConfig::IsAutoSwitchEnabled()) {
            if (bat.isAcOnline != lastAc) {
                s_acStateCounter++;
                if (s_acStateCounter >= 2) {
                    lastAc = bat.isAcOnline;
                    s_acStateCounter = 0;
                    Logger::Info(L"PowerState", lastAc ? L"AC Power Connected (Mains)" : L"Switched to Battery Power (DC)");
                    if (!lastAc) {
                        ApplyEcoProfile();
                    } else {
                        ApplyPerformanceProfile();
                    }
                }
            } else {
                s_acStateCounter = 0;
            }
        }

        // Monitor Windows Energy Saver changes from Windows OS
        if (winPow.isEnergySaverActive != lastWinSaver) {
            lastWinSaver = winPow.isEnergySaverActive;
            Logger::Info(L"WindowsPower", lastWinSaver ? L"Windows Energy Saver is ACTIVE" 
                                                       : L"Windows Energy Saver is OFF");
        }

        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_data.battery = bat;
            s_data.refreshRate = hz;
            s_data.cpuBoost = boost;
            s_data.gpu = gpu;
            s_data.thermalMode = therm;
            s_data.muxMode = mux;
            s_data.kbdLight = kbd;
            s_data.winPower = winPow;
        }

        // Notify UI to repaint without blocking
        if (s_notifyHwnd) {
            PostMessage(s_notifyHwnd, WM_USER + 2, 0, 0);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
}
