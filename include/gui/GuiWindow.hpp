#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>

struct GuiButton {
    int id = 0;
    Gdiplus::Rect bounds;
    std::wstring text;
    bool isHovered = false;
    bool isActive = false;
    Gdiplus::Color activeColor = Gdiplus::Color(255, 0, 229, 255);
    Gdiplus::Color hoverColor = Gdiplus::Color(255, 33, 41, 54);
    Gdiplus::Color normalColor = Gdiplus::Color(255, 22, 27, 34);
};

class GuiWindow {
private:
    static inline HWND s_hWnd = NULL;
    static inline std::vector<GuiButton> s_buttons;
    static inline int s_hoveredBtnId = -1;

    static void DrawRoundedRect(Gdiplus::Graphics& g, const Gdiplus::Rect& rc, int radius, 
                                const Gdiplus::Brush& brush, const Gdiplus::Pen* pen = nullptr);
    static void DrawCard(Gdiplus::Graphics& g, int x, int y, int w, int h, const std::wstring& title = L"");

public:
    static void InitButtons();
    static void Paint(HWND hWnd, HDC hdc);
    static void HandleClick(HWND hWnd, int x, int y);
    static void HandleMouseMove(HWND hWnd, int x, int y);
};
