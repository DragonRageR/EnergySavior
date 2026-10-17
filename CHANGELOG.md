# 📜 Журнал змін (Changelog) — EnergyControl

Усі помітні зміни у цьому проекті документуються відповідно до стандарту [Keep a Changelog](https://keepachangelog.com/uk/1.0.0/) та дотримуються [Семантичного версіонування (SemVer)](https://semver.org/).

---

## [2.1.0] — 2026-10-17

### 🌟 Додано (Added)
* **Інтеграція з режимом енергозбереження та енергоконтролю Windows (`WindowsPowerController`):**
  * Автоматичне виявлення стану вбудованого **Windows Energy Saver (Battery Saver)** через `SYSTEM_POWER_STATUS::SystemStatusFlag`.
  * Зчитування та перемикання **Windows Power Mode (Overlay Schemes)** через API `powrprof.dll`:
    * Режим Еко: встановлює `Best Power Efficiency` (0) та синхронізується з режимом енергозбереження Windows.
    * Режим Продуктивність: встановлює `Best Performance` (2).
  * Динамічний бейдж у шапці програми: **`[ WIN SAVER: ON ]`** (зелений) або **`[ WIN SAVER: OFF ]`** (нейтральний).
* **Повноцінна багатопотокова система логування (`Logger`):**
  * Потокобезпечний запис подій у файл `%LOCALAPPDATA%\EnergyControl\energy_control.log`.
  * Запис усіх апаратних подій з мілісекундними мітками часу: старт/стоп, детекція заліза, перемикання AC/DC, зміна частоти екрану, режим охолодження, стан MUX, завершення процесів GPU.
  * Автоматична ротація файлу логів при досягненні розміру **2 MB** (створюється резервна копія `.old`).
  * Кнопка швидкого відкриття журналу подій **`[!] Журнал подій (Лог)`** у вікні програми та пункт у контекстному меню трею (відкриває лог у Блокноті в 1 клік).
* **Єдиний файл правил та стандартів розробки (`DEVELOPMENT.md`):**
  * Повний опис архітектури ядра, правила нульового блокування UI (Zero-Lag), потокобезпеки, обробки Unicode та інструкція з додавання нових модулів.
* **Налаштування контролю версій Git:**
  * Файл `.gitignore` для виключення бінарних файлів, об'єктних файлів та тимчасових даних.
  * Інструкція відкату змін (Rollback Guide).

### 🔄 Змінено (Changed)
* Розширено габарити вікна до **880 x 720 px** для ідеального розміщення бейджа Windows Saver та кнопки логів.
* Оновлено опис у картках режимів з урахуванням профілю живлення Windows.

### 🐛 Виправлено (Fixed)
* Виправлено підтримку шляхів файлу логування з широкими символами через `_wfopen`.

---

## [2.0.0] — 2026-10-17

### 🌟 Додано (Added)
* **Багатопотокова архітектура телеметрії (`TelemetryThread`):**
  * Винесення всіх опитувань заліза (Win32 IOCTL, NVML, Windows Power API, WMI) в окремий фоновий потік із періодичністю 1.5 с.
  * Забезпечено плавну роботу інтерфейсу на частоті 60+ FPS без підвисань та фризів.
* **Універсальна детекція конфігурації (`SystemInfo`):**
  * Автоматичне зчитування виробника, моделі ноутбука, версії BIOS та назви процесора з SMBIOS / Registry (замість зашитих констант).
* **Контролер MUX Switch (Відеокарти):**
  * Підтримка режимів `Hybrid`, `iGPU Only` (повне знеструмлення dGPU для економії) та `dGPU Only` (прямий вивід для ігор).
* **Контролер підсвітки клавіатури:**
  * Циклічне перемикання рівнів підсвітки (Вимкнено, Низька, Висока) через фірмовий WMI-інтерфейс.
* **Моніторинг зовнішніх дисплеїв:**
  * Виявлення підключених моніторів (HDMI / Type-C) та індикація утримання dGPU активною через зовнішній відеовихід.
* **Безпечний Watchdog процесів dGPU:**
  * Фіксований контейнер для виводу списку програм з PID та захистом від переповнення рядків (`StringTrimmingEllipsisWord`).
* **Вбудований маніфест UAC (`requireAdministrator`):**
  * Гарантований запуск з правами адміністратора та зелений бейдж `[ АДМІНІСТРАТОР ]`.
* **Інтеграція в автозапуск Диспетчера завдань:**
  * Реєстрація у `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` з коректним екрануванням лапок (виправлено помилку `Windows не може знайти C:\Program`).

### 🐛 Виправлено (Fixed)
* Повністю усунено виклики консолі `system()` та `_popen()`, які викликали спалахи чорних вікон cmd/powershell та просідання нижче 1 FPS.
* Виправлено спам сповіщень Windows шляхом скидання прапорця `NIF_INFO` після відображення та додавання 5-секундного лімітера.
* Додано захист від мульти-кліків (Debounce) за допомогою атомарного блокування.
* Замінено зламані квадрати емодзі (`⯑`) у GDI+ на чіткі Unicode бейджі: `[ECO]`, `[MAX]`, `[V]`, `[!]`, `[OK]`.
* Модульний рефакторинг: код розділено на директорії `include/core`, `include/hardware`, `include/gui`, `src/core`, `src/hardware`, `src/gui`, а застарілі монолітні файли видалено.

---

## [1.0.0] — 2026-10-17

### 🌟 Додано (Added)
* Початковий монолітний прототип інтерфейсу та трей-агента для керування частотою оновлення (60/144 Hz) та режимами Fn+Q.

---

## ↩️ Інструкція з відкату змін (Rollback Guide)

У разі виникнення будь-яких проблем у нових версіях, проект підтримує миттєвий відкат за допомогою Git:

### 1. Перегляд історії змін:
```bash
git log --oneline --graph --decorate
```

### 2. Повернення до стабільної версії 2.0.0:
```bash
git checkout v2.0.0
```

### 3. Відкат останнього коміту зі збереженням файлів:
```bash
git reset --soft HEAD~1
```

### 4. Повний відкат до попереднього робочого стану:
```bash
git reset --hard HEAD~1
```

---

## 🏷 Рекомендовані повідомлення для комітів на GitHub

Якщо ви завантажуєте проект на GitHub, використовуйте такі готові повідомлення для комітів:

### Для поточної версії (v2.1.0):
```git
git commit -m "feat: add Windows Energy Saver sync, thread-safe logger, and development guidelines

- Integrate Windows 10/11 Energy Saver (Battery Saver) detection via SYSTEM_POWER_STATUS
- Add Windows Power Mode control (Power Efficiency / Best Performance) via powrprof
- Implement thread-safe Logger with 2MB file rotation and 1-click in-app viewer
- Add DEVELOPMENT.md architectural guidelines and clean code rules
- Expand GUI layout to 880x720 with Win Saver status badge
- Tag: v2.1.0"
```

### Для версії v2.0.0:
```git
git commit -m "feat(core): overhaul to multi-threaded architecture with universal hardware support

- Move telemetry to background worker thread (60+ FPS zero-lag UI)
- Implement universal hardware detection (SMBIOS, Registry, CPU, GPU)
- Add Lenovo MUX Switch (Hybrid / iGPU / dGPU) and keyboard light controls
- Fix notification spam and debounce rapid toggles
- Fix Task Manager autostart quotation bug
- Embed requireAdministrator manifest
- Modularize codebase into include/ and src/ subdirectories
- Tag: v2.0.0"
```
