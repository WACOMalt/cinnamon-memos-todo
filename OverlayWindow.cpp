#include "OverlayWindow.h"
#include "TaskbarTracker.h"
#include "ConfigWindow.h"
#include <iostream>
#include <dwmapi.h>

#pragma comment (lib,"Gdiplus.lib")

#define ID_TIMER_UPDATE 1
#define ID_TIMER_SCROLL 2
#define ID_TIMER_FETCH  3
#define IDI_ICON1 101

#define IDM_CONFIGURE 6001
#define IDM_EXIT 6002

OverlayWindow::OverlayWindow(HINSTANCE hInstance, MemosClient* client)
    : m_hInstance(hInstance), m_hWnd(NULL), m_client(client), 
      m_popup(nullptr), m_configWindow(nullptr), m_currentLineIndex(0),
      m_lastX(-1), m_lastY(-1), m_lastW(-1), m_lastH(-1) {
    
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
    
    m_displayText = L"Loading...";
}

OverlayWindow::~OverlayWindow() {
    if (m_popup) delete m_popup;
    if (m_configWindow) delete m_configWindow;
    Gdiplus::GdiplusShutdown(m_gdiplusToken);
}

bool OverlayWindow::Create() {
    const wchar_t CLASS_NAME[] = L"MemosOverlayClass";

    WNDCLASSEXW wc = { };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = OverlayWindow::WindowProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hbrBackground = NULL;

    RegisterClassExW(&wc);

    HWND hTaskbar = TaskbarTracker::GetTaskbarInfo().hWnd;

    m_hWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TOPMOST,
        CLASS_NAME,
        L"Memos Overlay",
        WS_POPUP,
        0, 0, 200, 30,
        hTaskbar, NULL, m_hInstance, this
    );

    if (m_hWnd == NULL) return false;

    // We used UpdateLayeredWindow, so we don't call SetLayeredWindowAttributes

    m_popup = new PopupWindow(m_hInstance, m_client);
    m_popup->Create(m_hWnd);

    m_configWindow = new ConfigWindow(m_hInstance, m_client);
    m_configWindow->Create(m_hWnd);

    RefreshTimers();
    UpdatePosition();
    PostMessage(m_hWnd, WM_TIMER, ID_TIMER_FETCH, 0);

    return true;
}

void OverlayWindow::RefreshTimers() {
    const auto& cfg = m_client->GetConfig();
    
    SetTimer(m_hWnd, ID_TIMER_UPDATE, 1000, NULL); // Reverting to 1s to stop flashing
    SetTimer(m_hWnd, ID_TIMER_SCROLL, cfg.scrollInterval * 1000, NULL);
    SetTimer(m_hWnd, ID_TIMER_FETCH, cfg.refreshInterval * 60000, NULL);
}

void OverlayWindow::Show() {
    ShowWindow(m_hWnd, SW_SHOWNOACTIVATE);
}

void OverlayWindow::Hide() {
    ShowWindow(m_hWnd, SW_HIDE);
}

void OverlayWindow::UpdatePosition() {
    TaskbarTracker::TaskbarInfo info = TaskbarTracker::GetTaskbarInfo();
    if (info.hWnd) {
        const auto& cfg = m_client->GetConfig();
        int width = 250;
        int height = info.rect.bottom - info.rect.top;
        if (height <= 0) height = 48; // Fallback for Win11
        int rightMargin = cfg.overlayRightMargin;
        
        int x = 0, y = 0;

        if (info.edge == TaskbarTracker::Edge::Bottom || info.edge == TaskbarTracker::Edge::Top) {
             if (!IsRectEmpty(&info.trayRect)) {
                 x = info.trayRect.left - width - 10;
             } else {
                 x = info.workingArea.right - width - rightMargin; 
             }
             y = info.rect.top;
        } else {
            width = info.rect.right - info.rect.left;
            x = info.rect.left;
            y = info.rect.top + 100;
            height = 30;
        }

        if (x != m_lastX || y != m_lastY || width != m_lastW || height != m_lastH) {
            SetWindowPos(m_hWnd, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
            m_lastX = x; m_lastY = y; m_lastW = width; m_lastH = height;
            Redraw(true); // Force redraw on move
        } else {
            // Re-assert topmost even if not moving, in case taskbar was clicked
            SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
}

void OverlayWindow::Redraw(bool force) {
    if (!force && m_displayText == m_lastRedrawnText) return;
    m_lastRedrawnText = m_displayText;

    RECT rect;
    GetWindowRect(m_hWnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    {
        Gdiplus::Graphics graphics(hdcMem);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
        
        // Background: Nearly transparent for clickability (Alpha 1)
        graphics.Clear(Gdiplus::Color(1, 0, 0, 0)); 

        // Visual Dividers
        Gdiplus::Pen dividerPen(Gdiplus::Color(100, 150, 150, 150), 1.0f);
        graphics.DrawLine(&dividerPen, 0, 10, 0, height - 10);
        graphics.DrawLine(&dividerPen, width - 1, 10, width - 1, height - 10);

        const auto& cfg = m_client->GetConfig();
        Gdiplus::FontFamily fontFamily(L"Segoe UI");
        Gdiplus::FontStyle style = Gdiplus::FontStyleRegular;
        bool isCompleted = (m_displayText.find((wchar_t)0x2611) != std::wstring::npos) || 
                           (m_displayText.find(L"[x]") != std::wstring::npos) || 
                           (m_displayText.find(L"[X]") != std::wstring::npos);
        if (isCompleted) style = Gdiplus::FontStyleStrikeout;

        Gdiplus::Font font(&fontFamily, (Gdiplus::REAL)cfg.panelFontSize, style, Gdiplus::UnitPoint);
        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentNear);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

        // 1. Draw Drop Shadow
        Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(160, 0, 0, 0));
        Gdiplus::RectF shadowRect(9, 1, (Gdiplus::REAL)width - 18, (Gdiplus::REAL)height);
        graphics.DrawString(m_displayText.c_str(), -1, &font, shadowRect, &format, &shadowBrush);

        // 2. Main Text
        Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 255, 255, 255));
        Gdiplus::RectF layoutRect(8, 0, (Gdiplus::REAL)width - 18, (Gdiplus::REAL)height);
        graphics.DrawString(m_displayText.c_str(), -1, &font, layoutRect, &format, &textBrush);
    }

    POINT ptSrc = { 0, 0 };
    SIZE sizeWnd = { width, height };
    POINT ptDest = { rect.left, rect.top };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    UpdateLayeredWindow(m_hWnd, hdcScreen, &ptDest, &sizeWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

void OverlayWindow::OnRightClick(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_CONFIGURE, L"Configure...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

    SetForegroundWindow(m_hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, x, y, 0, m_hWnd, NULL);
    DestroyMenu(hMenu);
}

void OverlayWindow::OnCommand(int id) {
    if (id == IDM_CONFIGURE) {
        if (m_configWindow) m_configWindow->Show();
    } else if (id == IDM_EXIT) {
        PostQuitMessage(0);
    }
}

void OverlayWindow::OnTimer() {
    // Handled in WndProc
}

void OverlayWindow::OnClick() {
    if (m_popup) {
        if (IsWindowVisible(m_popup->GetHwnd())) {
            m_popup->Hide();
        } else {
            if (m_popup->WasJustHidden()) return;
            
            // Sync before showing to avoid overwriting with stale data
            FetchContent();

            m_popup->SetContent(m_memoLines);

            // Calculate position: Just above the OverlayWindow
            RECT rect;
            GetWindowRect(m_hWnd, &rect);
            m_popup->Show(rect.left, rect.top); // Popup will handle "stacking upwards" logic
        }
    }
}

LRESULT CALLBACK OverlayWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* pThis = NULL;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (OverlayWindow*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (OverlayWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        switch (uMsg) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (pThis) pThis->Redraw();
        EndPaint(hwnd, &ps);
        return 0;
    }
        case WM_TIMER:
            if (wParam == ID_TIMER_UPDATE) {
                pThis->UpdatePosition();
            } else if (wParam == ID_TIMER_SCROLL) {
                if (!pThis->m_memoLines.empty()) {
                    const auto& cfg = pThis->m_client->GetConfig();
                    
                    // Try to find the next valid line
                    size_t initialIndex = pThis->m_currentLineIndex;
                    bool found = false;

                    for (size_t i = 0; i < pThis->m_memoLines.size(); ++i) {
                         pThis->m_currentLineIndex = (pThis->m_currentLineIndex + 1) % pThis->m_memoLines.size();
                         std::string line = pThis->m_memoLines[pThis->m_currentLineIndex];
                         
                         bool isCompleted = (line.find("- [x] ") == 0 || line.find("- [X] ") == 0 || line.find("☑ ") == 0);
                         if (isCompleted && !cfg.showCompletedPanel) continue;
                         
                         found = true;
                         break;
                    }

                    if (found) {
                        std::string line = pThis->m_memoLines[pThis->m_currentLineIndex];
                        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &line[0], (int)line.size(), NULL, 0);
                        std::wstring wstrTo(size_needed, 0);
                        MultiByteToWideChar(CP_UTF8, 0, &line[0], (int)line.size(), &wstrTo[0], size_needed);
                        pThis->m_displayText = wstrTo;
                        pThis->Redraw();
                    } else if (!pThis->m_memoLines.empty() && !cfg.showCompletedPanel) {
                        // All tasks completed and hidden
                        pThis->m_displayText = L"All tasks completed!";
                        pThis->Redraw();
                    }
                }
            } else if (wParam == ID_TIMER_FETCH) {
                pThis->FetchContent();
                pThis->RefreshTimers();
            }
            return 0;
            return 0;
        case WM_LBUTTONUP:
            pThis->OnClick();
            return 0;
        case WM_RBUTTONUP:
            {
                POINT pt; GetCursorPos(&pt);
                pThis->OnRightClick(pt.x, pt.y);
            }
            return 0;
        case WM_COMMAND:
            pThis->OnCommand(LOWORD(wParam));
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void OverlayWindow::FetchContent() {
    std::string content = m_client->GetMemoContent();
    if (!content.empty()) {
        m_memoLines.clear();
        size_t start = 0, end;
        while ((end = content.find('\n', start)) != std::string::npos) {
            m_memoLines.push_back(content.substr(start, end - start));
            start = end + 1;
        }
        m_memoLines.push_back(content.substr(start));

        if (!m_memoLines.empty()) {
            const auto& cfg = m_client->GetConfig();
            if (m_currentLineIndex >= m_memoLines.size()) m_currentLineIndex = 0;

            // Find valid line
            size_t initialIndex = m_currentLineIndex;
            bool found = false;
            for (size_t i = 0; i < m_memoLines.size(); ++i) {
                    std::string line = m_memoLines[m_currentLineIndex];
                    bool isCompleted = (line.find("- [x] ") == 0 || line.find("- [X] ") == 0 || line.find("☑ ") == 0);
                    if (isCompleted && !cfg.showCompletedPanel) {
                        m_currentLineIndex = (m_currentLineIndex + 1) % m_memoLines.size();
                        continue;
                    }
                    found = true;
                    break;
            }

            if (found) {
                std::string line = m_memoLines[m_currentLineIndex];
                int size_needed = MultiByteToWideChar(CP_UTF8, 0, &line[0], (int)line.size(), NULL, 0);
                std::wstring wstrTo(size_needed, 0);
                MultiByteToWideChar(CP_UTF8, 0, &line[0], (int)line.size(), &wstrTo[0], size_needed);
                m_displayText = wstrTo;
                Redraw();
            } else if (!cfg.showCompletedPanel) {
                    m_displayText = L"All tasks completed!";
                    Redraw();
            } else {
                // Should theoretically not happen if list not empty and showCompletedPanel is true
                    std::string line = m_memoLines[0];
                    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &line[0], (int)line.size(), NULL, 0);
                    std::wstring wstrTo(size_needed, 0);
                    MultiByteToWideChar(CP_UTF8, 0, &line[0], (int)line.size(), &wstrTo[0], size_needed);
                    m_displayText = wstrTo;
                    Redraw();
            }
        }
    }
}
