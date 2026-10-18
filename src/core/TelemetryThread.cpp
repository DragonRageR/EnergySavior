#include "../../include/core/TelemetryThread.hpp"
#include "../../include/core/AppConfig.hpp"
#include "../../include/core/Logger.hpp"
#include "../../include/gui/TrayManager.hpp"
#include "../../include/hardware/WindowsPowerController.hpp"
#include "../../include/hardware/GpuController.hpp"
#include <thread>
#include <objbase.h>

void TelemetryThread::Start(HWND notifyHwnd) {
    if (s_running) return;
    s_notifyHwnd = notifyHwnd;
    s_running = true;

    Logger::Init();

    // Очищуємо історію потужності при кожному новому старті
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_powerHistory.clear();
    }

    // Початкове опитування системи
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_data.system = SystemInfo::Detect();
        s_data.iGpuName = s_data.system.iGpuName;
        s_data.isElevated = AppConfig::IsElevated();
        s_data.winPower = WindowsPowerController::Query();

        Logger::Info(L"TelemetryThread", L"Фоновий потік запущено. Права адміністратора: " + 
                     std::wstring(s_data.isElevated ? L"ТАК" : L"НІ"));
        Logger::Info(L"SystemInfo", L"Виробник: " + s_data.system.manufacturer + L" " + s_data.system.model + 
                     L" | CPU: " + s_data.system.cpuName + L" | iGPU: " + s_data.iGpuName);
    }

    std::thread([notifyHwnd]() {
        WorkerLoop();
    }).detach();
}

void TelemetryThread::Stop() {
    s_running = false;
    Logger::Info(L"TelemetryThread", L"Потік зупиняється");
}

TelemetryData TelemetryThread::GetData() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_data;
}

bool TelemetryThread::CanNotify() {
    DWORD now = GetTickCount();
    return (now - s_lastNotificationTime) > 3000;
}

void TelemetryThread::MarkNotificationSent() {
    s_lastNotificationTime = GetTickCount();
}

void TelemetryThread::ApplySaviorProfile() {
    if (s_isSwitching) return;
    s_isSwitching = true;

    Logger::Info(L"Автоматика", L"Перемикання в режим SAVIOR (екстремальна економія): 60 Hz, Boost вимкнено, підсвітка OFF, профіль Quiet, Windows Saver");

    DisplayController::SetRefreshRate(60);
    CpuController::ApplyEcoProfile();
    LenovoWmi::SetThermalMode(ThermalMode::Quiet);
    LenovoWmi::SetKeyboardLight(KbdBacklight::Off);
    WindowsPowerController::ApplyEcoProfile();

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_data.refreshRate = 60;
        s_data.cpuBoost = CpuBoostMode::Disabled;
        s_data.thermalMode = ThermalMode::Quiet;
        s_data.kbdLight = KbdBacklight::Off;
        s_data.powerProfile = AppPowerProfile::Savior;
        s_data.isEcoMode = true;
        s_data.winPower.isEnergySaverActive = true;
    }

    TrayManager::UpdateIcon(AppPowerProfile::Savior);

    if (CanNotify()) {
        TrayManager::ShowBalloon(L"EnergySavior • РЕЖИМ SAVIOR", L"Активовано режим екстремального енергозбереження:\n• Екран: 60 Hz\n• CPU Boost: Вимкнено (Eco-throttle)\n• Охолодження: Тихий (Quiet)\n• Підсвітка клавіатури: Вимкнено\n• Windows: Енергозбереження");
        MarkNotificationSent();
    }

    s_isSwitching = false;
}

void TelemetryThread::ApplyEcoProfile() {
    if (s_isSwitching) return;
    s_isSwitching = true;

    Logger::Info(L"Автоматика", L"Перемикання в ЕКО режим: 60 Hz, Boost вимкнено, профіль Quiet, Windows Energy Saver");

    DisplayController::SetRefreshRate(60);
    CpuController::ApplyEcoProfile();
    LenovoWmi::SetThermalMode(ThermalMode::Quiet);
    WindowsPowerController::ApplyEcoProfile();

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_data.refreshRate = 60;
        s_data.cpuBoost = CpuBoostMode::Disabled;
        s_data.thermalMode = ThermalMode::Quiet;
        s_data.powerProfile = AppPowerProfile::Eco;
        s_data.isEcoMode = true;
        s_data.winPower.isEnergySaverActive = true;
    }

    TrayManager::UpdateIcon(AppPowerProfile::Eco);

    if (CanNotify()) {
        TrayManager::ShowBalloon(L"EnergySavior • ЕКО РЕЖИМ", L"Увімкнено максимальне енергозбереження:\n• Екран: 60 Hz\n• CPU Boost: Вимкнено\n• Охолодження: Quiet (Тихий)\n• Windows: Енергозбереження");
        MarkNotificationSent();
    }

    s_isSwitching = false;
}

void TelemetryThread::ApplyPerformanceProfile() {
    if (s_isSwitching) return;
    s_isSwitching = true;

    Logger::Info(L"Автоматика", L"Перемикання в режим продуктивності: 144 Hz, Boost агресивний, профіль Balance, Windows збалансований");

    DisplayController::SetRefreshRate(144);
    CpuController::ApplyPerformanceProfile();
    LenovoWmi::SetThermalMode(ThermalMode::Balance);
    WindowsPowerController::ApplyPerformanceProfile();

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_data.refreshRate = 144;
        s_data.cpuBoost = CpuBoostMode::Aggressive;
        s_data.thermalMode = ThermalMode::Balance;
        s_data.powerProfile = AppPowerProfile::Max;
        s_data.isEcoMode = false;
        s_data.winPower.isEnergySaverActive = false;
    }

    TrayManager::UpdateIcon(AppPowerProfile::Max);

    if (CanNotify()) {
        TrayManager::ShowBalloon(L"EnergySavior • МАКС. РЕЖИМ", L"Увімкнено продуктивний профіль:\n• Екран: 144 Hz\n• CPU Boost: Агресивний\n• Охолодження: Balance (Збалансований)\n• Windows: Збалансований");
        MarkNotificationSent();
    }

    s_isSwitching = false;
}

void TelemetryThread::ApplyCustomProfile() {
    if (s_isSwitching) return;
    s_isSwitching = true;

    DWORD hz = AppConfig::GetCustomRefreshRate();
    int boostInt = AppConfig::GetCustomCpuBoost();
    int thermalInt = AppConfig::GetCustomThermalMode();
    int kbdInt = AppConfig::GetCustomKbdBacklight();
    bool winSaver = AppConfig::GetCustomWinEnergySaver();

    Logger::Info(L"Автоматика", L"Перемикання в CUSTOM режим: " + std::to_wstring(hz) + L" Hz, Boost=" + std::to_wstring(boostInt) + L", Thermal=" + std::to_wstring(thermalInt));

    DisplayController::SetRefreshRate(hz);
    CpuBoostMode boostMode = (boostInt == 0) ? CpuBoostMode::Disabled : ((boostInt == 2) ? CpuBoostMode::Aggressive : CpuBoostMode::Enabled);
    CpuController::SetBoostMode(boostMode, false);
    CpuController::SetBoostMode(boostMode, true);
    ThermalMode thermalMode = static_cast<ThermalMode>(thermalInt);
    LenovoWmi::SetThermalMode(thermalMode);
    KbdBacklight kbdLight = static_cast<KbdBacklight>(kbdInt);
    LenovoWmi::SetKeyboardLight(kbdLight);

    if (winSaver) {
        WindowsPowerController::ApplyEcoProfile();
    } else {
        WindowsPowerController::ApplyPerformanceProfile();
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_data.refreshRate = hz;
        s_data.cpuBoost = boostMode;
        s_data.thermalMode = thermalMode;
        s_data.kbdLight = kbdLight;
        s_data.powerProfile = AppPowerProfile::Custom;
        s_data.isEcoMode = (hz == 60 && boostInt == 0);
        s_data.winPower.isEnergySaverActive = winSaver;
    }

    TrayManager::UpdateIcon(AppPowerProfile::Custom);

    if (CanNotify()) {
        TrayManager::ShowBalloon(L"EnergySavior • ВЛАСНИЙ РЕЖИМ", L"Застосовано налаштування Custom-профілю:\n• Екран: " + std::to_wstring(hz) + L" Hz\n• CPU Boost: " + CpuController::ToString(boostMode) + L"\n• Охолодження: " + LenovoWmi::ThermalToString(thermalMode));
        MarkNotificationSent();
    }

    s_isSwitching = false;
}

void TelemetryThread::SetProfile(AppPowerProfile profile) {
    switch (profile) {
        case AppPowerProfile::Savior: ApplySaviorProfile(); break;
        case AppPowerProfile::Eco:    ApplyEcoProfile(); break;
        case AppPowerProfile::Max:    ApplyPerformanceProfile(); break;
        case AppPowerProfile::Custom: ApplyCustomProfile(); break;
    }
}

void TelemetryThread::CycleProfile() {
    AppPowerProfile cur = AppPowerProfile::Max;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        cur = s_data.powerProfile;
    }

    AppPowerProfile next;
    if (cur == AppPowerProfile::Savior) next = AppPowerProfile::Eco;
    else if (cur == AppPowerProfile::Eco) next = AppPowerProfile::Max;
    else if (cur == AppPowerProfile::Max) next = AppPowerProfile::Custom;
    else next = AppPowerProfile::Savior;

    SetProfile(next);
}

void TelemetryThread::ToggleMode() {
    CycleProfile();
}

void TelemetryThread::SetRefreshRate(DWORD hz) {
    Logger::Info(L"Дисплей", L"Встановлення частоти оновлення: " + std::to_wstring(hz) + L" Hz");
    DisplayController::SetRefreshRate(hz);
    std::lock_guard<std::mutex> lock(s_mutex);
    s_data.refreshRate = hz;
}

void TelemetryThread::SetCpuBoost(CpuBoostMode mode) {
    Logger::Info(L"Процесор", L"Режим Intel Turbo Boost змінено: " + CpuController::ToString(mode));
    CpuController::SetBoostMode(mode, false);
    CpuController::SetBoostMode(mode, true);
    std::lock_guard<std::mutex> lock(s_mutex);
    s_data.cpuBoost = mode;
}

void TelemetryThread::SetThermalMode(ThermalMode mode) {
    Logger::Info(L"Охолодження", L"Режим охолодження змінено: " + LenovoWmi::ThermalToString(mode));
    std::lock_guard<std::mutex> lock(s_mutex);
    s_data.thermalMode = mode;
}

void TelemetryThread::SetMuxMode(GpuMuxMode mode) {
    Logger::Info(L"Відеокарта", L"Режим MUX Switch змінено: " + LenovoWmi::MuxToString(mode));
    std::lock_guard<std::mutex> lock(s_mutex);
    s_data.muxMode = mode;
}

void TelemetryThread::SetKeyboardLight(KbdBacklight light) {
    Logger::Info(L"Клавіатура", L"Підсвічування клавіатури: " + LenovoWmi::KbdToString(light));
    std::lock_guard<std::mutex> lock(s_mutex);
    s_data.kbdLight = light;
}

std::vector<float> TelemetryThread::GetPowerHistory(size_t maxSeconds) {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_powerHistory.empty()) {
        return std::vector<float>();
    }
    if (s_powerHistory.size() <= maxSeconds) {
        return s_powerHistory;
    }
    return std::vector<float>(s_powerHistory.end() - maxSeconds, s_powerHistory.end());
}

void TelemetryThread::WorkerLoop() {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    bool lastAc = true;
    bool lastWinSaver = false;
    uint64_t loopCount = 0;

    ThermalMode therm = LenovoWmi::GetThermalMode();
    GpuMuxMode mux = LenovoWmi::GetMuxMode();
    KbdBacklight kbd = LenovoWmi::GetKeyboardLight();
    int brightness = DisplayController::GetBrightness();
    std::vector<TopProcessInfo> topProcs = SystemInfo::GetTopMemoryProcesses(6);
    std::vector<TopProcessInfo> cpuProcs;
    std::vector<TopProcessInfo> iGpuProcs;
    std::wstring iGpuName = SystemInfo::DetectIGpuName();
    GpuSnapshot gpu = GpuController::Query();

    auto nextTick = std::chrono::steady_clock::now();

    while (s_running) {
        nextTick += std::chrono::milliseconds(1000);

        BatterySnapshot bat = BatteryController::Query();
        DWORD hz = DisplayController::GetCurrentRefreshRate();

        if (loopCount % 2 == 0) {
            brightness = DisplayController::GetBrightness();
        }
        float dispWatts = DisplayController::CalculateDisplayWatts(hz, brightness);
        CpuBoostMode boost = CpuController::GetBoostMode(!bat.isAcOnline);
        
        float sysCpu = SystemInfo::GetSystemCpuUsagePercent();
        float cpuWatts = 4.5f + (sysCpu / 100.0f) * (boost == CpuBoostMode::Aggressive ? 42.0f : 20.0f);

        if (loopCount % 2 == 0) {
            gpu = GpuController::Query();
            cpuProcs = SystemInfo::GetTopCpuProcesses(12, cpuWatts);
            std::vector<DWORD> dPids;
            for (const auto& gp : gpu.processes) dPids.push_back(gp.pid);
            iGpuProcs = SystemInfo::GetIGpuProcesses(dPids, 12);
            iGpuName = SystemInfo::DetectIGpuName();
        }

        WindowsPowerStatus winPow = WindowsPowerController::Query();

        if (loopCount % 4 == 0) {
            therm = LenovoWmi::GetThermalMode();
            mux = LenovoWmi::GetMuxMode();
            kbd = LenovoWmi::GetKeyboardLight();
            topProcs = SystemInfo::GetTopMemoryProcesses(6);
        }

        float iGpuWatts = 1.2f + (iGpuProcs.size() > 0 ? 0.8f : 0.0f);

        // Розрахунок потужності (Ватт):
        // - На батареї (DC): реальний фізичний розряд батареї від сенсора (mW -> W)
        // - На мережі (AC 220V): детальний розрахунок суми компонентів (CPU + dGPU + Екран + iGPU + RAM/Плата)
        float curWatts = 0.0f;
        if (!bat.isAcOnline && bat.isDischarging && bat.dischargeRateMW > 0 && bat.dischargeRateMW < 150000) {
            curWatts = (float)(bat.dischargeRateMW / 1000.0);
        } else {
            float gpuEst = (gpu.isAwake && gpu.powerWatts <= 125.0) ? (float)gpu.powerWatts : 0.0f;
            curWatts = cpuWatts + gpuEst + dispWatts + iGpuWatts + 2.5f;
        }

        // Захист від аномальних сплесків
        if (curWatts > 150.0f) curWatts = 150.0f;
        if (curWatts < 2.0f)   curWatts = 2.0f;

        // Автоперемикання режимів при зміні живлення AC/DC
        if (AppConfig::IsAutoSwitchEnabled()) {
            if (bat.isAcOnline != lastAc) {
                s_acStateCounter++;
                if (s_acStateCounter >= 2) {
                    lastAc = bat.isAcOnline;
                    s_acStateCounter = 0;
                    Logger::Info(L"Автоматика", lastAc ? L"Підключено до мережі 220V" : L"Перехід на живлення від батареї (DC)");
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

        // Моніторинг статусу Windows Energy Saver
        if (winPow.isEnergySaverActive != lastWinSaver) {
            lastWinSaver = winPow.isEnergySaverActive;
            Logger::Info(L"WindowsPower", lastWinSaver ? L"Стан Windows Energy Saver: Увімкнено" 
                                                       : L"Стан Windows Energy Saver: Вимкнено");
        }

        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_data.battery = bat;
            s_data.refreshRate = hz;
            s_data.brightness = brightness;
            s_data.displayWatts = dispWatts;
            s_data.cpuBoost = boost;
            s_data.cpuUsagePercent = sysCpu;
            s_data.cpuWatts = cpuWatts;
            s_data.iGpuName = iGpuName;
            s_data.iGpuWatts = iGpuWatts;
            s_data.gpu = gpu;
            s_data.thermalMode = therm;
            s_data.muxMode = mux;
            s_data.kbdLight = kbd;
            s_data.winPower = winPow;
            s_data.topProcesses = topProcs;
            s_data.cpuProcesses = cpuProcs;
            s_data.iGpuProcesses = iGpuProcs;

            // Додаємо в історію потужності (до 900 сек = 15 хв)
            s_powerHistory.push_back(curWatts);
            if (s_powerHistory.size() > 900) {
                s_powerHistory.erase(s_powerHistory.begin());
            }
        }

        if (s_notifyHwnd) {
            PostMessage(s_notifyHwnd, WM_USER + 2, 0, 0);
        }

        loopCount++;

        auto now = std::chrono::steady_clock::now();
        if (now < nextTick) {
            std::this_thread::sleep_until(nextTick);
        } else {
            nextTick = now;
        }
    }

    CoUninitialize();
}

void TelemetryThread::RefreshPowerStatus() {
    WindowsPowerStatus st = WindowsPowerController::Query();
    GpuSnapshot gpu = GpuController::Query();
    std::vector<TopProcessInfo> topProcs = SystemInfo::GetTopMemoryProcesses(6);
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_data.winPower = st;
        s_data.gpu = gpu;
        s_data.topProcesses = topProcs;
    }
    if (s_notifyHwnd) {
        PostMessage(s_notifyHwnd, WM_USER + 2, 0, 0);
    }
}
