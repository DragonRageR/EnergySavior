# 📘 Інструкція та правила розробки EnergyControl (Development & Architecture Guide)

Цей документ визначає єдиний стандарт архітектури, правила коду, додавання нового функціоналу та підготовки релізів для проекту **EnergyControl**.

---

## 🏛 1. Загальна архітектура проекту

Проект побудовано за модульним принципом з розділенням відповідальності (Separation of Concerns):

```
EnergyControl/
├── include/
│   ├── core/                      # Ядро: конфігурація, логування, фонова телеметрія
│   │   ├── AppConfig.hpp          # Читання/запис config.ini, UAC elevation, автозапуск
│   │   ├── Logger.hpp             # Багатопотоковий логер з ротацією (energy_control.log)
│   │   ├── SystemInfo.hpp         # Динамічне зчитування SMBIOS / Registry
│   │   └── TelemetryThread.hpp    # Фоновий воркер-потік, кешування та debounce
│   ├── hardware/                  # Апаратні контролери (Hardware Abstraction Layer)
│   │   ├── BatteryController.hpp  # Win32 IOCTL / SetupAPI моніторинг батареї
│   │   ├── CpuController.hpp      # Windows Power API (Intel/AMD Turbo Boost)
│   │   ├── DisplayController.hpp  # ChangeDisplaySettings (60 Hz / 144 Hz)
│   │   ├── GpuController.hpp      # NVML моніторинг дискретної GPU та процесів
│   │   ├── LenovoWmi.hpp          # Lenovo WMI (Thermal Fn+Q, MUX Switch, Підсвітка)
│   │   └── WindowsPowerController.hpp # Синхронізація з Windows Energy Saver та Power Mode
│   └── gui/                       # Графічний інтерфейс та системний трей
│       ├── GuiWindow.hpp          # GDI+ рендеринг, double buffering, обробка кліків
│       └── TrayManager.hpp        # Іконка в треї, бейджі [ECO]/[MAX], меню
├── src/                           # Реалізація відповідних модулів
│   ├── core/
│   ├── hardware/
│   └── gui/
├── res/                           # Ресурси та маніфести
│   ├── app.manifest               # UAC requireAdministrator та DPI awareness
│   ├── app.rc                     # Windows Resource script
│   └── app.res                    # Зкомпільований ресурс (COFF)
├── EnergyControl.exe.manifest     # Маніфест для Windows Activation Context
├── main.cpp                       # Точка входу (WinMain), черга повідомлень Windows
├── build.bat                      # Скрипт компіляції (GCC MinGW-w64 C++20)
├── install_autostart.bat          # Реєстрація автозапуску (Run key + Task Scheduler)
├── uninstall_autostart.bat        # Видалення автозапуску
├── CHANGELOG.md                   # Історія версій та інструкції відкату
└── DEVELOPMENT.md                 # Цей документ
```

---

## 📐 2. Головні правила розробки (Code Guidelines)

### Правило 1: Нульове блокування UI-потоку (Zero-Lag UI Rule)
* **Категорично заборонено** викликати в головному потоці вікна (у `WM_PAINT`, `WM_LBUTTONDOWN`, `WM_TIMER`):
  * Виклики `system()`, `_popen()`, `powershell.exe` чи `cmd.exe`.
  * Важкі файлові операції чи синхронні WMI-запити.
* Усі запити до датчиків заліза повинні відбуватися **тільки** у фоновому потоці `TelemetryThread` (інтервал 1.5 с).
* Метод `GuiWindow::Paint()` повинен виключно зчитувати готовий кешований знімок `TelemetryThread::GetData()` за 0.0 мс.

### Правило 2: Потокобезпека (Thread Safety)
* Доступ до спільного стану `s_data` у `TelemetryThread` завжди повинен бути захищений м'ютексом `std::lock_guard<std::mutex> lock(s_mutex);`.
* Для запобігання зацикленню або нашаруванню швидких перемикань використовується атомарний прапорець `std::atomic<bool> s_isSwitching` (Debounce).

### Правило 3: Чистий Unicode UTF-16
* Увесь проект компілюється з `#define UNICODE` та `#define _UNICODE`.
* Для тексту використовується виключно `std::wstring`, `wchar_t` та широкі Win32 API (`CreateWindowExW`, `DrawTextW`, `Shell_NotifyIconW`).
* Для запису логів у файл проводиться конвертація в UTF-8 через `WideCharToMultiByte(CP_UTF8, ...)`, що запобігає будь-яким проблемам з кодуванням.

### Правило 4: Обов'язкове логування (Structured Logging)
* Будь-яка зміна апаратного стану (підключення AC/DC, зміна частоти екрану, режим охолодження, стан MUX, завершення процесів GPU) **повинна логуватися** через `Logger::Info()`, `Logger::Warn()` або `Logger::Error()`.
* Лог-файл зберігається у `%LOCALAPPDATA%\EnergyControl\energy_control.log` та має автоматичну ротацію при досягненні 2 MB.

### Правило 5: Візуальні стандарти GDI+
* Обов'язкове подвійне буферування (`CreateCompatibleDC` + `CreateCompatibleBitmap` + `BitBlt`) для усунення мерехтіння.
* Використовувати згладжування: `g.SetSmoothingMode(SmoothingModeAntiAlias);` та `g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);`.
* Кожен елемент інтерфейсу (кнопка, текст, бейдж) повинен мати жорстко обмежену прямокутну область (`Rect` або `RectF`) для запобігання накладанню тексту при різних DPI або довгих назвах процесів.

---

## 🔌 3. Як додати новий апаратний контролер

1. **Створіть заголовок** `include/hardware/YourController.hpp`:
   ```cpp
   #pragma once
   #include <windows.h>
   #include <string>

   struct YourSnapshot {
       bool isSupported = false;
       int currentValue = 0;
   };

   class YourController {
   public:
       static YourSnapshot Query();
       static bool SetValue(int val);
   };
   ```

2. **Реалізуйте логіку** у `src/hardware/YourController.cpp`:
   - Використовуйте нативні Win32 API або приховані процеси `CreateProcessW` з `CREATE_NO_WINDOW`.
   - Додайте логування: `Logger::Info(L"YourModule", L"Action performed");`.

3. **Додайте дані у знімок телеметрії**:
   - У `include/core/TelemetryThread.hpp` додайте поле у структуру `TelemetryData`.
   - У `src/core/TelemetryThread.cpp` у функції `WorkerLoop()` додайте опитування контролера.

4. **Додайте кнопку або віджет у GUI**:
   - У `GuiWindow::InitButtons()` зареєструйте новий `GuiButton` з унікальним ID.
   - У `GuiWindow::Paint()` відобразіть стан або текст кнопки.
   - У `GuiWindow::HandleClick()` додайте обробник у `switch (btn.id)`.

---

## 🚀 4. Збирання та перевірка

Збирання здійснюється за допомогою `build.bat` або команди:
```powershell
g++ -O2 -std=c++20 main.cpp src/core/*.cpp src/hardware/*.cpp src/gui/*.cpp -lsetupapi -lpowrprof -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -mwindows -o EnergyControl.exe
```

### Прапорці компілятора:
* `-O2`: Оптимізація швидкості виконання.
* `-std=c++20`: Стандарт C++20 (концепти, сучасні потоки).
* `-mwindows`: Компіляція підсистеми GUI (без консольного вікна).
* Бібліотеки: `setupapi` (батарея), `powrprof` (живлення Windows), `gdiplus` / `gdi32` (графіка), `user32` / `shell32` (вікна та трей), `ole32` (COM/WMI).

---

## 📦 5. Правила оформлення комітів (Git Commit Convention)

Використовуйте стандарт [Conventional Commits](https://www.conventionalcommits.org/):

| Префікс | Призначення | Приклад |
|---|---|---|
| `feat:` | Новий функціонал | `feat: add Windows Energy Saver and Power Mode integration` |
| `fix:` | Виправлення помилки | `fix: resolve notification spam and debounce rapid toggles` |
| `perf:` | Оптимізація швидкодії | `perf: migrate telemetry to background worker thread (60+ FPS UI)` |
| `refactor:` | Рефакторинг структури | `refactor: split monolithic codebase into modular core/hardware/gui` |
| `docs:` | Оновлення документації | `docs: add DEVELOPMENT.md and CHANGELOG.md` |
