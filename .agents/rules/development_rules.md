# EnergyControl Development Rules

When modifying or expanding the EnergyControl codebase:
1. **Zero-Lag UI Rule:** Never execute slow or blocking commands (`system()`, `_popen()`, PowerShell, slow file I/O) on the UI thread. All background work belongs in `TelemetryThread` or detached worker threads.
2. **Multi-Thread Safety:** Any state shared between the worker thread and the UI must be protected with `std::mutex` in `TelemetryThread`.
3. **Strict Unicode UTF-16:** Use `std::wstring`, `wchar_t`, and wide Win32 APIs (`CreateWindowExW`, `Shell_NotifyIconW`, etc.).
4. **Structured Logging:** Log all state changes and hardware transitions via `Logger::Info()`, `Logger::Warn()`, `Logger::Error()`.
5. **Clean Layout:** In `GuiWindow.cpp`, keep non-overlapping bounding rectangles for all buttons, labels, and cards. Do not use raw emojis that render as broken glyph boxes (`⯑`) in GDI+; use text badges (`[ECO]`, `[MAX]`, `[V]`, `[!]`, `[OK]`).
6. **Keep Architecture Modular:** Place core engine code in `include/core/` and `src/core/`, hardware modules in `include/hardware/` and `src/hardware/`, and UI/tray code in `include/gui/` and `src/gui/`.
