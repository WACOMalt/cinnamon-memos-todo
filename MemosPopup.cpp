#include "MemosPopup.h"
#include <commctrl.h>
#include <shellapi.h> 
#include <gdiplus.h>
#include <iostream>

#pragma comment(lib, "Comctl32.lib")

#define ID_BTN_SAVE 1001
#define ID_BTN_ADD 1002
#define ID_BTN_BROWSER 1003
#define ID_EDIT_NEW 1004
#define ID_BASE_CHECKBOX 2000
#define ID_BASE_DELETE 3000
#define ID_BASE_LABEL 4000

// Adwaita Dark Palette
#define COL_BG RGB(36, 36, 36)          // Surface
#define COL_TEXT RGB(255, 255, 255)      // Content
#define COL_TEXT_DONE RGB(154, 153, 150) // Dimmed
#define COL_INPUT_BG RGB(45, 45, 45)     // Entry
#define COL_BTN_BG RGB(53, 53, 53)      // Button
#define COL_BTN_HOVER RGB(62, 62, 62)   // Button Hover
#define COL_ACCENT RGB(53, 132, 228)    // Blue accent

PopupWindow::PopupWindow(HINSTANCE hInstance, MemosClient* client) 
    : m_hInstance(hInstance), m_hWnd(NULL), m_client(client), 
      m_hBrushBack(NULL), m_hBrushInput(NULL), m_hFont(NULL), m_hFontStrike(NULL),
      m_hoveredButtonId(-1), m_lastHideTime(0), m_anchorX(0), m_anchorY(0) {
    
    m_hBrushBack = CreateSolidBrush(COL_BG);
    m_hBrushInput = CreateSolidBrush(COL_INPUT_BG);
    RefreshFonts();
}

void PopupWindow::RefreshFonts() {
    if (m_hFont) DeleteObject(m_hFont);
    if (m_hFontStrike) DeleteObject(m_hFontStrike);

    int ptSize = m_client->GetConfig().popupFontSize;
    int height = -MulDiv(ptSize, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72);

    m_hFont = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        
    m_hFontStrike = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, TRUE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

PopupWindow::~PopupWindow() {
    if (m_hBrushBack) DeleteObject(m_hBrushBack);
    if (m_hBrushInput) DeleteObject(m_hBrushInput);
    if (m_hFont) DeleteObject(m_hFont);
    if (m_hFontStrike) DeleteObject(m_hFontStrike);
    if (IsWindow(m_hWnd)) DestroyWindow(m_hWnd);
}

bool PopupWindow::Create(HWND hParent) {
    m_hParent = hParent;
    const wchar_t CLASS_NAME[] = L"MemosPopupClassV14";

    WNDCLASSEXW wc = { };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = PopupWindow::WindowProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = m_hBrushBack; 

    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLASS_NAME,
        L"Memos List",
        WS_POPUP, 
        0, 0, 350, 400,
        hParent, NULL, m_hInstance, this
    );

    return (m_hWnd != NULL);
}

void PopupWindow::Show(int x, int y) {
    RefreshFonts(); 
    
    // Store anchor (bottom-left corner essentially)
    // We want the popup to be ABOVE this y coordinate.
    m_anchorX = x;
    m_anchorY = y;
    
    UpdateSizeAndPosition(); // Set size first so PopulateList sees correct ClientRect
    PopulateList();
    
    ShowWindow(m_hWnd, SW_SHOW);
    SetForegroundWindow(m_hWnd); 
}

void PopupWindow::UpdateSizeAndPosition() {
    const auto& cfg = m_client->GetConfig();
    int popupWidth = cfg.popupWidth;
    int count = (int)m_lines.size();
    
    int itemHeight = cfg.popupFontSize + 20; 
    int totalHeight = 15 + (count * itemHeight) + 40 + 10 + 40 + 15;
    
    if (totalHeight > 700) totalHeight = 700;
    if (totalHeight < 150) totalHeight = 150;
    
    // Position upwards from anchorY
    SetWindowPos(m_hWnd, HWND_TOPMOST, m_anchorX, m_anchorY - totalHeight - 5, popupWidth, totalHeight, SWP_NOACTIVATE | SWP_NOZORDER);
}

void PopupWindow::Hide() {
    OnSave(false); // Auto-save when closing/hiding
    m_lastHideTime = GetTickCount64();
    ShowWindow(m_hWnd, SW_HIDE);
}

void PopupWindow::SetContent(const std::vector<std::string>& lines) {
    m_lines = lines;
}

bool StartsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

void PopupWindow::PopulateList() {
    HWND child = GetWindow(m_hWnd, GW_CHILD);
    while (child) {
        HWND next = GetWindow(child, GW_HWNDNEXT);
        DestroyWindow(child);
        child = next;
    }
    m_checkboxes.clear();
    m_deleteButtons.clear();
    m_labels.clear();

    const auto& cfg = m_client->GetConfig();
    RECT rcClient; GetClientRect(m_hWnd, &rcClient);
    int clientWidth = rcClient.right;
    if (clientWidth == 0) clientWidth = cfg.popupWidth;

    int y = 15;
    int uiIndex = 0;
    int itemHeight = cfg.popupFontSize + 20;
    
    m_itemIndices.clear();

    for (int i = 0; i < (int)m_lines.size(); ++i) {
        const std::string& line = m_lines[i];
        bool isCheckbox = false;
        bool isChecked = false;
        std::string contentStr = line;

        if (StartsWith(line, "- [ ] ")) { isCheckbox = true; contentStr = line.substr(6); }
        else if (StartsWith(line, "- [x] ")) { isCheckbox = true; isChecked = true; contentStr = line.substr(6); }
        else if (StartsWith(line, "- [X] ")) { isCheckbox = true; isChecked = true; contentStr = line.substr(6); }
        else if (StartsWith(line, "☐ ")) { isCheckbox = true; contentStr = line.substr(4); }
        else if (StartsWith(line, "☑ ")) { isCheckbox = true; isChecked = true; contentStr = line.substr(4); }

        if (isChecked && !cfg.showCompletedPopup) continue;

        m_itemIndices.push_back(i); // Map UI index to index 'i' in m_lines

        std::wstring wText;
        int len = MultiByteToWideChar(CP_UTF8, 0, contentStr.c_str(), -1, NULL, 0);
        wText.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, contentStr.c_str(), -1, &wText[0], len);
        if (!wText.empty() && wText.back() == 0) wText.pop_back();

        HWND hCb = CreateWindowW(L"BUTTON", L"", 
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_FLAT,
            12, y + (itemHeight - 20) / 2, 20, 20, 
            m_hWnd, (HMENU)(ID_BASE_CHECKBOX + uiIndex), m_hInstance, NULL);
        if (isChecked) SendMessage(hCb, BM_SETCHECK, BST_CHECKED, 0);
        m_checkboxes.push_back(hCb);
        
        int lblWidth = clientWidth - 50 - 45;
        HWND hLbl = CreateWindowW(L"STATIC", wText.c_str(),
            WS_VISIBLE | WS_CHILD | SS_LEFT | SS_CENTERIMAGE | SS_ENDELLIPSIS | SS_NOTIFY,
            40, y, lblWidth, itemHeight - 4,
            m_hWnd, (HMENU)(ID_BASE_LABEL + uiIndex), m_hInstance, NULL);
        
        SendMessage(hLbl, WM_SETFONT, isChecked ? (WPARAM)m_hFontStrike : (WPARAM)m_hFont, TRUE);
        m_labels.push_back(hLbl);

        // Delete button - BS_OWNERDRAW
        HWND hDel = CreateWindowW(L"BUTTON", L"\xD83D\xDDD1", // Trash can
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            clientWidth - 52, y, 32, itemHeight - 4,
            m_hWnd, (HMENU)(ID_BASE_DELETE + uiIndex), m_hInstance, NULL);
        m_deleteButtons.push_back(hDel);
        
        y += itemHeight;
        uiIndex++;
    }

    y += 10;
    m_hEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        10, y, clientWidth - 67, 30, m_hWnd, (HMENU)ID_EDIT_NEW, m_hInstance, NULL);
    SendMessage(m_hEdit, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    m_hBtnAdd = CreateWindowW(L"BUTTON", L"+", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
        clientWidth - 52, y, 32, 30, m_hWnd, (HMENU)ID_BTN_ADD, m_hInstance, NULL);

    y += 45;
    m_hBtnBrowser = CreateWindowW(L"BUTTON", L"Open Browser", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 
        10, y, clientWidth - 20, 35, m_hWnd, (HMENU)ID_BTN_BROWSER, m_hInstance, NULL);
}

void PopupWindow::DrawCustomButton(LPDRAWITEMSTRUCT lp) {
    Gdiplus::Graphics g(lp->hDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

    RECT rc = lp->rcItem;
    Gdiplus::RectF rectF((Gdiplus::REAL)rc.left, (Gdiplus::REAL)rc.top, 
                         (Gdiplus::REAL)(rc.right - rc.left), (Gdiplus::REAL)(rc.bottom - rc.top));
    
    bool isHovered = (m_hoveredButtonId == (int)lp->CtlID);
    bool isPressed = (lp->itemState & ODS_SELECTED);

    // Adwaita Style: Rounded Rect
    float radius = 6.0f;
    Gdiplus::GraphicsPath path;
    path.AddArc(rectF.X, rectF.Y, radius * 2, radius * 2, 180, 90);
    path.AddArc(rectF.X + rectF.Width - radius * 2, rectF.Y, radius * 2, radius * 2, 270, 90);
    path.AddArc(rectF.X + rectF.Width - radius * 2, rectF.Y + rectF.Height - radius * 2, radius * 2, radius * 2, 0, 90);
    path.AddArc(rectF.X, rectF.Y + rectF.Height - radius * 2, radius * 2, radius * 2, 90, 90);
    path.CloseFigure();

    COLORREF col = COL_BTN_BG;
    if (isPressed) col = COL_ACCENT;
    else if (isHovered) col = COL_BTN_HOVER;

    Gdiplus::SolidBrush br(Gdiplus::Color(255, GetRValue(col), GetGValue(col), GetBValue(col)));
    g.FillPath(&br, &path);

    // Text
    int len = GetWindowTextLengthW(lp->hwndItem);
    if (len > 0) {
        std::wstring text(len + 1, 0);
        GetWindowTextW(lp->hwndItem, &text[0], len + 1);
        
        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font font(&ff, 10, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush textBr(Gdiplus::Color(255, 255, 255, 255));
        
        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentCenter);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        
        g.DrawString(text.c_str(), -1, &font, rectF, &format, &textBr);
    }
}

void PopupWindow::OnSave(bool close) {
    std::string newContent;
    
    // SYNC: Update the full line list based on visible checkboxes
    for (size_t k = 0; k < m_checkboxes.size(); ++k) {
        if (k >= m_itemIndices.size()) break;
        int dataIdx = m_itemIndices[k];
        if (dataIdx >= (int)m_lines.size()) continue;
        
        LRESULT state = SendMessage(m_checkboxes[k], BM_GETCHECK, 0, 0);
        bool checked = (state == BST_CHECKED);
        std::string prefix = checked ? "☑ " : "☐ ";
        
        // Extract text from current line (skipping old prefix)
        std::string oldLine = m_lines[dataIdx];
        std::string content;
        if (StartsWith(oldLine, "- [ ] ")) content = oldLine.substr(6);
        else if (StartsWith(oldLine, "- [x] ")) content = oldLine.substr(6);
        else if (StartsWith(oldLine, "- [X] ")) content = oldLine.substr(6);
        else if (StartsWith(oldLine, "☐ ")) content = oldLine.substr(4);
        else if (StartsWith(oldLine, "☑ ")) content = oldLine.substr(4);
        else content = oldLine;
        
        m_lines[dataIdx] = prefix + content;
    }

    for(size_t i=0; i < m_lines.size(); ++i) {
        newContent += m_lines[i];
        if(i < m_lines.size() - 1) newContent += "\n";
    }

    if (m_client->UpdateMemoContent(newContent)) {
        // Notify parent (OverlayWindow) to refresh its data
        if (m_hParent) {
            PostMessage(m_hParent, WM_TIMER, 3, 0); // 3 is ID_TIMER_FETCH
        }

        if (close) Hide();
        else {
            PopulateList();
            UpdateSizeAndPosition();
        }
    }
}

void PopupWindow::OnAdd() {
    int len = GetWindowTextLengthW(m_hEdit);
    if (len > 0) {
        std::wstring wText(len + 1, 0);
        GetWindowTextW(m_hEdit, &wText[0], len + 1);
        int uLen = WideCharToMultiByte(CP_UTF8, 0, wText.c_str(), -1, NULL, 0, NULL, NULL);
        std::string uText(uLen, 0);
        WideCharToMultiByte(CP_UTF8, 0, wText.c_str(), -1, &uText[0], uLen, NULL, NULL);
        if (!uText.empty() && uText.back() == 0) uText.pop_back();

        m_lines.push_back("☐ " + uText);
        SetWindowTextW(m_hEdit, L""); // Clear input
        PopulateList(); // Rebuild UI to include new checkbox
        OnSave(false);  // Save with new state 
    }
}

void PopupWindow::OnDelete(int uiIndex) {
    if (uiIndex >= 0 && uiIndex < (int)m_itemIndices.size()) {
        int dataIdx = m_itemIndices[uiIndex];
        if (dataIdx >= 0 && dataIdx < (int)m_lines.size()) {
            m_lines.erase(m_lines.begin() + dataIdx);
            OnSave(false);
        }
    }
}

LRESULT CALLBACK PopupWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    PopupWindow* pThis = (PopupWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg) {
    case WM_NCCREATE:
    {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pCreate->lpCreateParams);
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        SetBkColor(hdc, COL_BG);
        bool isDimmed = false;
        if (pThis) {
            for (size_t i=0; i<pThis->m_labels.size(); ++i) {
                if (pThis->m_labels[i] == hCtrl) {
                    if (SendMessage(pThis->m_checkboxes[i], BM_GETCHECK, 0, 0) == BST_CHECKED) isDimmed = true;
                    break;
                }
            }
        }
        SetTextColor(hdc, isDimmed ? COL_TEXT_DONE : COL_TEXT);
        return (INT_PTR)pThis->m_hBrushBack;
    }
    case WM_CTLCOLORBTN:
        SetBkColor((HDC)wParam, COL_BG);
        return (INT_PTR)pThis->m_hBrushBack;
    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)wParam, COL_TEXT);
        SetBkColor((HDC)wParam, COL_INPUT_BG);
        return (INT_PTR)pThis->m_hBrushInput;

    case WM_DRAWITEM:
        if (pThis) pThis->DrawCustomButton((LPDRAWITEMSTRUCT)lParam);
        return TRUE;

    case WM_MOUSEMOVE:
    {
        POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
        HWND hUnder = ChildWindowFromPoint(hwnd, pt);
        int newHover = -1;
        if (hUnder && hUnder != hwnd) {
            newHover = GetDlgCtrlID(hUnder);
        }

        if (newHover != pThis->m_hoveredButtonId) {
            int oldHover = pThis->m_hoveredButtonId;
            pThis->m_hoveredButtonId = newHover;
            
            // Redraw old and new buttons
            if (oldHover != -1) InvalidateRect(GetDlgItem(hwnd, oldHover), NULL, TRUE);
            if (newHover != -1) {
                InvalidateRect(GetDlgItem(hwnd, newHover), NULL, TRUE);
                // Track mouse leave
                TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
        }
        break;
    }
    case WM_MOUSELEAVE:
        if (pThis && pThis->m_hoveredButtonId != -1) {
            int old = pThis->m_hoveredButtonId;
            pThis->m_hoveredButtonId = -1;
            InvalidateRect(GetDlgItem(hwnd, old), NULL, TRUE);
        }
        break;

    case WM_COMMAND:
        if (!pThis) break;
        if (LOWORD(wParam) == ID_BTN_ADD) pThis->OnAdd();
        else if (LOWORD(wParam) == ID_BTN_BROWSER) pThis->m_client->OpenInBrowser();
        else if (LOWORD(wParam) >= ID_BASE_DELETE && LOWORD(wParam) < ID_BASE_LABEL) pThis->OnDelete(LOWORD(wParam) - ID_BASE_DELETE);
        else if (LOWORD(wParam) >= ID_BASE_CHECKBOX && LOWORD(wParam) < ID_BASE_DELETE) pThis->OnSave(false);
        else if (LOWORD(wParam) >= ID_BASE_LABEL && HIWORD(wParam) == STN_CLICKED) {
            int idx = LOWORD(wParam) - ID_BASE_LABEL;
            HWND hCb = pThis->m_checkboxes[idx];
            LRESULT state = SendMessage(hCb, BM_GETCHECK, 0, 0);
            SendMessage(hCb, BM_SETCHECK, (state == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED, 0);
            pThis->OnSave(false);
        }
        break;

    case WM_ACTIVATE:
        // Task: Close when clicking outside
        if (LOWORD(wParam) == WA_INACTIVE && pThis) pThis->Hide();
        break;
        
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH hBorder = CreateSolidBrush(RGB(100, 100, 100));
        FrameRect(hdc, &rc, hBorder);
        DeleteObject(hBorder);
        EndPaint(hwnd, &ps);
        break;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
