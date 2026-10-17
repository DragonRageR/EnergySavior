#include "../../include/core/TelemetryThread.hpp"
#include "../../include/core/AppConfig.hpp"
#include "../../include/core/Logger.hpp"
#include "../../include/gui/TrayManager.hpp"
#include "../../include/hardware/WindowsPowerController.hpp"
#include <thread>
#include <algorithm>

void TelemetryThread::Start(HWND notifyHwnd) {
    if (s_running) return;
    s_notifyHwnd = notifyHwnd;
    s_running = true;

    Logger::Init();

    // Очистити історію споживання від попередньої сесії
    { std::lock_guard<std::mutex> lock(s_mutex); s_powerHistory.clear(); }

    // Початкова апаратна діагностика
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_data.system = SystemInfo::Detect();
        s_data.isElevated = AppConfig::IsElevated();
        s_data.winPower = WindowsPowerController::Query();

        Logger::Info(L"TelemetryThread", L"Телеметричний потік успішно запущено. Права Адміністратора: " + 
                     std::wstring(s_data.isElevated ? L"ТАК" : L"НІ"));
        Logger::Info(L"SystemInfo", L"Виявлено пристрій: " + s_data.system.manufacturer + L" " + s_data.system.model + 
                     L" | BIOS: " + s_data.system.biosVersion + L" | CPU: " + s_data.system.cpuName);
    }

    std::thread([notifyHwnd]() {
        WorkerLoop();
    }).detach();
}

void TelemetryThread::Stop() {
    s_running = false;
    Logger::Info(L"TelemetryThread", L"Зупинка телеметричного потоку");
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

void TelemetryThread::ApplyEcoProfile() {
    if (s_isSwitching) return;
    s_isSwitching = true;

    Logger::Info(L"Профіль", L"Застосування ЕКО профілю: 60 Hz, Boost Вимкнено, Вент Quiet, Windows Energy Saver");

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
        s_data.winPower.isEnergySaverActive = true;
    }

    if (CanNotify()) {
        TrayManager::ShowBalloon(L"EnergySavior — Еко-режим", L"Увімкнено максимальне енергозбереження:\n• Екран: 60 Hz\n• CPU Boost: Вимкнено\n• Режим Fn+Q: Quiet (Тихий)\n• Windows: Енергозбереження");
        MarkNotificationSent();
    }

    s_isSwitching = false;
}

void TelemetryThread::ApplyPerformanceProfile() {
    if (s_isSwitching) return;
    s_isSwitching = true;

    Logger::Info(L"Профіль", L"Застосування МАКС продуктивності: 144 Hz, Boost Увімкнено, Вент Balance, Windows Збалансований");

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
        s_data.winPower.isEnergySaverActive = false;
    }

    if (CanNotify()) {
        TrayManager::ShowBalloon(L"EnergySavior — Макс. режим", L"Увімкнено максимальну продуктивність:\n• Екран: 144 Hz\n• CPU Boost: Увімкнено\n• Режим Fn+Q: Balance (Збалансований)\n• Windows: Баланс");
        MarkNotificationSent();
    }

    s_isSwitching = false;
}

void TelemetryThread::ToggleMode() {
    bool currentEco = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        currentEco = s_data.isEcoMode;
    }

    if (currentEco) {
        ApplyPerformanceProfile();
    } else {
        ApplyEcoProfile();
    }
}

void TelemetryThread::SetRefreshRate(DWORD hz) {
    Logger::Info(L"Дисплей", L"Встановлення частоти оновлення екрана: " + std::to_wstring(hz) + L" Hz");
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
    Logger::Info(L"Охолодження", L"Режим вентиляторів Fn+Q змінено: " + LenovoWmi::ThermalToString(mode));
    std::lock_guard<std::mutex> lock(s_mutex);
    s_data.thermalMode = mode;
}

void TelemetryThread::SetMuxMode(GpuMuxMode mode) {
    Logger::Info(L"Відеокарта", L"Режим MUX Switch змінено: " + LenovoWmi::MuxToString(mode));
    std::lock_guard<std::mutex> lock(s_mutex);
    s_data.muxMode = mode;
}

void TelemetryThread::SetKeyboardLight(KbdBacklight light) {
    Logger::Info(L"Підсвітка", L"Підсвітка клавіатури встановлена: " + LenovoWmi::KbdToString(light));
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
    bool lastAc = true;
    bool lastWinSaver = false;
    uint64_t loopCount = 0;

    ThermalMode therm = LenovoWmi::GetThermalMode();
    GpuMuxMode mux = LenovoWmi::GetMuxMode();
    KbdBacklight kbd = LenovoWmi::GetKeyboardLight();
    int brightness = DisplayController::GetBrightness();
    std::vector<TopProcessInfo> topProcs = SystemInfo::GetTopMemoryProcesses(6);
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
        if (loopCount % 2 == 0) {
            gpu = GpuController::Query();
        }
        WindowsPowerStatus winPow = WindowsPowerController::Query();

        if (loopCount % 4 == 0) {
            therm = LenovoWmi::GetThermalMode();
            mux = LenovoWmi::GetMuxMode();
            kbd = LenovoWmi::GetKeyboardLight();
            topProcs = SystemInfo::GetTopMemoryProcesses(6);
        }

        // Розрахунок поточної споживаної потужності (Вт)
        float curWatts = 0.0f;
        if (bat.isDischarging && bat.dischargeRateMW > 0 && bat.dischargeRateMW < 150000) {
            curWatts = (float)(bat.dischargeRateMW / 1000.0);
        } else {
            // Робота від 220V: розрахункова сума CPU/чіпсет + dGPU + екран + вентилятори
            float cpuEst = (boost == CpuBoostMode::Aggressive) ? 14.0f : 7.0f;
            float gpuEst = (gpu.isAwake && gpu.powerWatts <= 125.0) ? (float)gpu.powerWatts : 0.0f;
            curWatts = cpuEst + gpuEst + dispWatts + 2.5f;
        }

        // Захист від аномальних сплесків датчиків (макс. 200 Вт для ноутбука)
        if (curWatts > 200.0f) curWatts = 25.0f;
        if (curWatts < 3.0f)   curWatts = 3.0f;

        // Автоперемикач AC/DC
        if (AppConfig::IsAutoSwitchEnabled()) {
            if (bat.isAcOnline != lastAc) {
                s_acStateCounter++;
                if (s_acStateCounter >= 2) {
                    lastAc = bat.isAcOnline;
                    s_acStateCounter = 0;
                    Logger::Info(L"Живлення", lastAc ? L"Підключено живлення від мережі 220V" : L"Перехід на живлення від акумулятора (DC)");
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

        // Відстеження зміни Windows Energy Saver
        if (winPow.isEnergySaverActive != lastWinSaver) {
            lastWinSaver = winPow.isEnergySaverActive;
            Logger::Info(L"WindowsPower", lastWinSaver ? L"Стан Windows Energy Saver: УВІМКНЕНО" 
                                                       : L"Стан Windows Energy Saver: ВИМКНЕНО");
        }

        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_data.battery = bat;
            s_data.refreshRate = hz;
            s_data.brightness = brightness;
            s_data.displayWatts = dispWatts;
            s_data.cpuBoost = boost;
            s_data.gpu = gpu;
            s_data.thermalMode = therm;
            s_data.muxMode = mux;
            s_data.kbdLight = kbd;
            s_data.winPower = winPow;
            s_data.topProcesses = topProcs;

            // Збереження історії споживання (до 900 сек = 15 хв)
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
