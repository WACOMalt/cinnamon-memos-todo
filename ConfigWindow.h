#pragma once
#include <windows.h>
#include <string>
#include <map>
#include "MemosClient.h"

class ConfigWindow {
public:
    ConfigWindow(HINSTANCE hInstance, MemosClient* client);
    ~ConfigWindow();

    bool Create(HWND hParent);
    void Show();
    void Hide();
    HWND GetHwnd() const { return m_hWnd; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void PopulateUI();
    void OnSave();
    void DrawCustomButton(LPDRAWITEMSTRUCT lpDrawItem);
    
    HINSTANCE m_hInstance;
    HWND m_hWnd;
    MemosClient* m_client;
    int m_hoveredButtonId; // Track for hover effects
    
    HBRUSH m_hBrushBack;
    HBRUSH m_hBrushInput;
    HFONT m_hFont;
    HFONT m_hFontLabel;

    // Controls
    HWND m_hEditServer;
    HWND m_hEditToken;
    HWND m_hEditMemoId;
    HWND m_hEditRefresh;
    HWND m_hEditScroll;
    HWND m_hEditPopupSize;
    HWND m_hEditPanelSize;
    HWND m_hEditPopupWidth;
    HWND m_hEditPanelWidth;
    HWND m_hCheckSetPanelWidth;
    HWND m_hCheckShowCompletedPanel;
    HWND m_hCheckShowCompletedPopup;
    HWND m_hEditTransition;
    HWND m_hEditMargin;
    
    HWND m_hBtnSave;
    HWND m_hBtnCancel;
};
