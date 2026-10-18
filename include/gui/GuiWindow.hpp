#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>

enum class GuiTab {
    Dashboard = 0,     // Головна панель
    Graphs = 1,        // Графіки та моніторинг
    CpuProcesses = 2,  // Енергозатратні процеси на CPU
    IGpuProcesses = 3, // Процеси на iGPU (Вбудована графіка)
    Devices = 4,       // Дискретна графіка dGPU та пристрої
    Settings = 5       // Налаштування користувацького профілю та трею
};

enum class ProcessFilter {
    GpuProcesses = 0,
    TopSystem = 1
};

struct GuiButton {
    int id = 0;
    Gdiplus::Rect bounds;
    std::wstring text;
    bool isHovered = false;
    bool isActive = false;
    Gdiplus::Color activeColor = Gdiplus::Color(255, 0, 229, 255);
    Gdiplus::Color hoverColor = Gdiplus::Color(255, 33, 41, 54);
    Gdiplus::Color normalColor = Gdiplus::Color(255, 22, 27, 34);
    int tab = -1; // -1: persistent across all tabs, 0: Dashboard, 1: Graphs, 2: CpuProcesses, 3: IGpuProcesses, 4: Devices, 5: Settings
};

class GuiWindow {
private:
    static inline HWND s_hWnd = NULL;
    static inline std::vector<GuiButton> s_buttons;
    static inline int s_hoveredBtnId = -1;
    static inline GuiTab s_currentTab = GuiTab::Dashboard;
    static inline size_t s_selectedGraphSeconds = 60; // 30, 60, 300, 900
    static inline ProcessFilter s_processFilter = ProcessFilter::GpuProcesses;

    static void DrawRoundedRect(Gdiplus::Graphics& g, const Gdiplus::Rect& rc, int radius, 
                                const Gdiplus::Brush& brush, const Gdiplus::Pen* pen = nullptr);
    static void DrawCard(Gdiplus::Graphics& g, int x, int y, int w, int h, const std::wstring& title = L"");

    static void PaintDashboard(Gdiplus::Graphics& g, const struct TelemetryData& td, int w, int h);
    static void PaintGraphs(Gdiplus::Graphics& g, const struct TelemetryData& td, int w, int h);
    static void PaintCpuProcesses(Gdiplus::Graphics& g, const struct TelemetryData& td, int w, int h);
    static void PaintIGpuProcesses(Gdiplus::Graphics& g, const struct TelemetryData& td, int w, int h);
    static void PaintDevices(Gdiplus::Graphics& g, const struct TelemetryData& td, int w, int h);
    static void PaintSettings(Gdiplus::Graphics& g, const struct TelemetryData& td, int w, int h);

public:
    static void InitButtons();
    static void Paint(HWND hWnd, HDC hdc);
    // Малювання готового знімка без опитування обладнання; також для відтворюваних тестів.
    static void Paint(HWND hWnd, HDC hdc, const struct TelemetryData& td);
    static bool HandleClick(HWND hWnd, int x, int y);
    static void HandleMouseMove(HWND hWnd, int x, int y);
    static void SetTab(GuiTab tab);
    static GuiTab GetTab() { return s_currentTab; }
    static const std::vector<GuiButton>& GetButtons() { return s_buttons; }
};
