#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <gdiplus.h>
#include "MemosClient.h"
#include "MemosPopup.h"

class ConfigWindow; // Forward declaration

class OverlayWindow {
public:
    OverlayWindow(HINSTANCE hInstance, MemosClient* client);
    ~OverlayWindow();

    bool Create();
    void Show();
    void Hide();
    void Update(); // Called by timer in main loop or internally

    HWND GetHwnd() const { return m_hWnd; }
    void UpdatePosition();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void Redraw(bool force = false);
    void FetchContent(); // Helper to fetch and parse
    void OnTimer();
    void OnClick();
    void OnRightClick(int x, int y);
    void OnCommand(int id);
    void RefreshTimers();

    HINSTANCE m_hInstance;
    HWND m_hWnd;
    MemosClient* m_client;
    PopupWindow* m_popup;
    ConfigWindow* m_configWindow;

    std::wstring m_displayText;
    std::vector<std::string> m_memoLines;
    size_t m_currentLineIndex;
    
    // Caching for flicker reduction
    int m_lastX, m_lastY, m_lastW, m_lastH;
    std::wstring m_lastRedrawnText;

    // GDI+
    ULONG_PTR m_gdiplusToken;
};
