#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "MemosClient.h"

class PopupWindow {
public:
    PopupWindow(HINSTANCE hInstance, MemosClient* client);
    ~PopupWindow();

    bool Create(HWND hParent);
    void Show(int x, int y);
    void Hide();
    void SetContent(const std::vector<std::string>& lines);
    HWND GetHwnd() const { return m_hWnd; }
    bool WasJustHidden() const { return (GetTickCount64() - m_lastHideTime) < 200; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void PopulateList();
    void OnSave(bool close = true); // Saves current checkboxes state
    void OnAdd();  // Adds item from edit box
    void OnDelete(int index);
    void OnOpenBrowser();
    void DrawCustomButton(LPDRAWITEMSTRUCT lpDrawItem);
    void RefreshFonts();
    void UpdateSizeAndPosition(); // Helper to resize upwards

    ULONGLONG m_lastHideTime;
    int m_anchorX, m_anchorY;
    // UI Helpers
    HBRUSH m_hBrushBack;
    HBRUSH m_hBrushInput;
    HFONT m_hFont;
    HFONT m_hFontStrike;

    HINSTANCE m_hInstance;
    HWND m_hWnd;
    HWND m_hParent;
    MemosClient* m_client;
    int m_hoveredButtonId; // Track for hover effects
    
    std::vector<std::string> m_lines;
    std::vector<int> m_itemIndices; // Maps UI row 'k' to m_lines 'index'
    std::vector<HWND> m_checkboxes;
    std::vector<HWND> m_labels;
    std::vector<HWND> m_deleteButtons;
    HWND m_hEdit;
    HWND m_hBtnAdd;
    HWND m_hBtnSave;
    HWND m_hBtnBrowser;
};
