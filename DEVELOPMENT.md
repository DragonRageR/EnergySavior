# 📘 Інструкція та правила розробки EnergySavior (Development & Architecture Guide)

Цей документ визначає єдиний стандарт архітектури, правила коду, додавання нового функціоналу та підготовки релізів для проекту **EnergySavior**.

---

## 🏛 1. Загальна архітектура проекту

Проект побудовано за модульним принципом з розділенням відповідальності (Separation of Concerns):

```
EnergySavior/
├── include/
│   ├── core/                      # Ядро: конфігурація, логування, фонова телеметрія
│   │   ├── AppConfig.hpp          # Читання/запис config.ini, UAC elevation, автозапуск
│   │   ├── Logger.hpp             # Багатопотоковий логер з ротацією (energy_savior.log)
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
├── EnergySavior.exe.manifest      # Маніфест для Windows Activation Context
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
* Лог-файл зберігається у `%LOCALAPPDATA%\EnergySavior\energy_control.log` та має автоматичну ротацію при досягненні 2 MB.

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

## 🚀 4. Збирання та профілі оптимізації (Production Build System)

Збирання виконується за допомогою `build.bat` з вибором цільового профілю (аналог `cargo build --release` у Rust):

### 1. Release профіль (Максимальна швидкодія):
```powershell
.\build.bat release
# або просто:
.\build.bat
```
* **Прапорці оптимізації:**
  * `-O3`: Агресивна оптимізація компілятора, автовекторизація та планування інструкцій процесора.
  * `-flto`: Link-Time Optimization (міжмодульне інлайнінгування функцій та видалення мертвого коду).
  * `-march=x86-64-v2`: Використання інструкцій сучасних процесорів (SSE4.2, PopCount, CMPXCHG16B).
  * `-DNDEBUG`: Видалення налагоджувальних перевірок для максимальної швидкості.
  * `-s`: Стрипінг символів (розмір бінарника скорочено до ~155 KB).
  * `-ldbghelp -lcrypt32`: Підтримка генерації minidump та Base64 шифрування.

### 2. Debug профіль (Повне налагодження):
```powershell
.\build.bat debug
```
Генерує `EnergySavior_debug.exe` з налагоджувальними символами (`-g3`, `-O0`, `-DDEBUG`).

---

## 🧪 5. Автоматизоване тестування (Automated Unit Tests)

У проекті реалізовано незалежний тестовий фреймворк C++20 (`tests/TestRunner.hpp`), що покриває всі ключові модулі:
* `AppConfig` (версіонування, збереження параметрів у config.ini).
* `Logger` (створення файлів, безпечний багатопотоковий запис, рівні логів).
* `BatteryController` (математика розряду, розрахунок залишкового часу, захист від ділення на нуль).
* `CpuController` (конвертація станів Intel Turbo Boost, перевірка безпеки зчитування).
* `WindowsPowerController` (перевірка схем живлення Windows).
* `LenovoWmi` (валідація рядкових констант MUX, підсвітки, Thermal Fn+Q).
* `GpuController` (моніторинг температур, потужності та процесів).
* `CrashHandler` (перевірка кодів винятків SEH).

### Запуск тестів:
```powershell
.\build.bat test
# або напряму:
.\run_tests.bat
```
Усі тести виконуються автоматично за ~50 мс з детальним кольоровим звітом.

---

## 🛡️ 6. Обробка помилок та діагностика крашів (Crash Reporting & SEH)

У модуль `CrashHandler` інтегровано перехоплення системних помилок процесора та пам'яті:
* **Structured Exception Handling (SEH):** Перехоплює `EXCEPTION_ACCESS_VIOLATION (0xC0000005)`, `EXCEPTION_STACK_OVERFLOW`, ділення на нуль тощо.
* **Генерація Minidump (.dmp):** Викликає `MiniDumpWriteDump` з бібліотеки `dbghelp.dll`. Отриманий файл можна відкрити у WinDbg або Visual Studio для точного встановлення рядка збою.
* **Стек викликів (Stack Backtrace):** Автоматичний запис адрес інструкцій та зміщень модулів (`EnergySavior.exe+0x837e`).
* **Збереження звіту:** Створюється `%LOCALAPPDATA%\EnergySavior\crash_report_YYYY-MM-DD_HH-MM-SS.log`.
* **Діалогове вікно:** Користувачеві виводиться системне вікно з поясненням збою та кнопкою швидкого переходу до файлів у Провіднику для надсилання репорту.
* **Тестування краш-хендлера:**
  ```powershell
  .\EnergySavior.exe --test-crash
  ```

---

## 🐧 7. Тестування у VirtualBox (Linux) та крос-платформність

Проект має чітке розділення на ядро (`src/core/`) та апаратні драйвери (`src/hardware/`):
* Для тестування компіляції та юніт-тестів на Linux використовується `CMakeLists.txt`:
  ```bash
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build
  ctest --test-dir build --output-on-failure
  ```
* Крос-компіляція під Windows з Linux здійснюється за допомогою `x86_64-w64-mingw32-g++`.

---

## 📦 8. Правила оформлення комітів та версіонування (CalVer)

* **Стандарт версіонування:** Calendar Versioning у форматі `<стадія>-<рік>.<місяць>[.<патч>]` (наприклад: `alpha-2026.10`).
* До відкриття публічного стабільного релізу версії мають префікс `alpha-` або `beta-`.
* Стабільні релізи позначаються як `vYYYY.MM` (наприклад: `v2026.10`).
* Використовуйте стандарт [Conventional Commits](https://www.conventionalcommits.org/):
  * `feat:` Новий функціонал.
  * `fix:` Виправлення помилки або крашу.
  * `test:` Додавання або оновлення юніт-тестів.
  * `perf:` Оптимізація швидкодії або прапорців збірки.
  * `docs:` Оновлення документації.

---

## 🇺🇦 9. Мовний стандарт та українізація (Language & Localization)

* Усі інтерфейсні тексти, назви вкладок, сповіщення в треї та підказки пишуться виключно українською мовою.
* Коментарі в коді та документація ведуться українською мовою.
* Журнал подій (`Logger`) записує дії та зміни станів українською мовою.


## Перевірка та збирання виправлень інтерфейсу

- Використовувати наявні `GuiWindow`, `tests/TestMain.cpp` та `build.bat`; звичайні виправлення оновлюють кореневий `EnergySavior.exe`, без альтернативних назв виконуваних файлів.
- Послідовність: `build.bat test --no-pause`, перегляд усіх вкладок у `build/ui-previews`, потім `build.bat release --no-pause`. Помилка тестів зупиняє підготовку збірки.
- Знімки для перевірки мають містити довгі назви обладнання й процесів, заповнені показники та масштабування 100–200%. Порожній початковий екран не замінює перевірку даних.
- Текст у колонках обмежувати `RectF`. Пояснення переносити в межах картки; короткі статуси формулювати так, щоб вони читалися повністю.
- Апаратні тести перевіряють контракт результату; відсутність доступних процесів в ізольованому сеансі є допустимим результатом. Не позначати приблизне визначення GPU як підтверджений факт.
