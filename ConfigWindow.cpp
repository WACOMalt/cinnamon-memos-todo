#include "ConfigWindow.h"
#include <commctrl.h>
#include <gdiplus.h>
#include <iostream>

#define ID_BTN_SAVE_CFG 5001
#define ID_BTN_CANCEL_CFG 5002

// Adwaita Dark Palette
#define COL_BG RGB(36, 36, 36)          // Surface
#define COL_TEXT RGB(255, 255, 255)      // Content
#define COL_INPUT_BG RGB(45, 45, 45)     // Entry
#define COL_BTN_BG RGB(53, 53, 53)      // Button
#define COL_BTN_HOVER RGB(62, 62, 62)   // Button Hover
#define COL_ACCENT RGB(53, 132, 228)    // Blue accent

ConfigWindow::ConfigWindow(HINSTANCE hInstance, MemosClient* client)
    : m_hInstance(hInstance), m_client(client), m_hWnd(NULL), m_hoveredButtonId(-1) {
    m_hBrushBack = CreateSolidBrush(COL_BG);
    m_hBrushInput = CreateSolidBrush(COL_INPUT_BG);
    m_hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    m_hFontLabel = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

ConfigWindow::~ConfigWindow() {
    if (m_hBrushBack) DeleteObject(m_hBrushBack);
    if (m_hBrushInput) DeleteObject(m_hBrushInput);
    if (m_hFont) DeleteObject(m_hFont);
    if (m_hFontLabel) DeleteObject(m_hFontLabel);
}

bool ConfigWindow::Create(HWND hParent) {
    const wchar_t CLASS_NAME[] = L"MemosConfigClass";

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = m_hBrushBack;

    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(
        0, CLASS_NAME, L"Configure Memos-Todo",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, 
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 650,
        hParent, NULL, m_hInstance, this
    );

    return (m_hWnd != NULL);
}

void ConfigWindow::Show() {
    PopulateUI();
    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);
}

void ConfigWindow::Hide() {
    ShowWindow(m_hWnd, SW_HIDE);
}

HWND CreateLabel(const wchar_t* text, int x, int y, int w, HWND parent, HFONT font) {
    HWND h = CreateWindowW(L"STATIC", text, WS_VISIBLE | WS_CHILD, x, y, w, 20, parent, NULL, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)font, TRUE);
    return h;
}

HWND CreateEdit(const wchar_t* text, int x, int y, int w, HWND parent, HFONT font) {
    HWND h = CreateWindowW(L"EDIT", text, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, x, y, w, 24, parent, NULL, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)font, TRUE);
    return h;
}

HWND CreateCheck(const wchar_t* text, bool checked, int x, int y, int w, HWND parent, HFONT font) {
    HWND h = CreateWindowW(L"BUTTON", text, WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, x, y, w, 24, parent, NULL, NULL, NULL);
    SendMessage(h, WM_SETFONT, (WPARAM)font, TRUE);
    if (checked) SendMessage(h, BM_SETCHECK, BST_CHECKED, 0);
    return h;
}

void ConfigWindow::PopulateUI() {
    // Clear old children if any? Actually we create once or rebuild?
    // Let's just destroy and rebuild for simplicity or check if created.
    HWND child = GetWindow(m_hWnd, GW_CHILD);
    while (child) {
        HWND next = GetWindow(child, GW_HWNDNEXT);
        DestroyWindow(child);
        child = next;
    }

    const MemosClient::Config& cfg = m_client->GetConfig();
    int y = 20;
    int xLeft = 20;
    int xRight = 200;
    int wRight = 200;

    CreateLabel(L"Server URL:", xLeft, y, 150, m_hWnd, m_hFontLabel);
    m_hEditServer = CreateEdit(cfg.serverUrl.c_str(), xRight, y, wRight, m_hWnd, m_hFont);
    y += 40;

    CreateLabel(L"Auth Token:", xLeft, y, 150, m_hWnd, m_hFontLabel);
    m_hEditToken = CreateEdit(cfg.authToken.c_str(), xRight, y, wRight, m_hWnd, m_hFont);
    y += 40;

    CreateLabel(L"Memo ID:", xLeft, y, 150, m_hWnd, m_hFontLabel);
    std::wstring wMemoId;
    int len = MultiByteToWideChar(CP_UTF8, 0, cfg.memoId.c_str(), -1, NULL, 0);
    wMemoId.resize(len);
    MultiByteToWideChar(CP_UTF8, 0, cfg.memoId.c_str(), -1, &wMemoId[0], len);
    if (!wMemoId.empty() && wMemoId.back() == 0) wMemoId.pop_back();
    m_hEditMemoId = CreateEdit(wMemoId.c_str(), xRight, y, wRight, m_hWnd, m_hFont);
    y += 50;

    CreateLabel(L"Refresh interval (min):", xLeft, y, 170, m_hWnd, m_hFont);
    m_hEditRefresh = CreateEdit(std::to_wstring(cfg.refreshInterval).c_str(), xRight, y, 60, m_hWnd, m_hFont);
    y += 40;

    CreateLabel(L"Scroll interval (sec):", xLeft, y, 170, m_hWnd, m_hFont);
    m_hEditScroll = CreateEdit(std::to_wstring(cfg.scrollInterval).c_str(), xRight, y, 60, m_hWnd, m_hFont);
    y += 40;

    CreateLabel(L"Popup Font Size (pt):", xLeft, y, 170, m_hWnd, m_hFont);
    m_hEditPopupSize = CreateEdit(std::to_wstring(cfg.popupFontSize).c_str(), xRight, y, 60, m_hWnd, m_hFont);
    y += 40;

    CreateLabel(L"Panel Font Size (pt):", xLeft, y, 170, m_hWnd, m_hFont);
    m_hEditPanelSize = CreateEdit(std::to_wstring(cfg.panelFontSize).c_str(), xRight, y, 60, m_hWnd, m_hFont);
    y += 40;

    CreateLabel(L"Popup Width (px):", xLeft, y, 170, m_hWnd, m_hFont);
    m_hEditPopupWidth = CreateEdit(std::to_wstring(cfg.popupWidth).c_str(), xRight, y, 60, m_hWnd, m_hFont);
    y += 40;

    m_hCheckSetPanelWidth = CreateCheck(L"Set Fixed Panel Width", cfg.setPanelWidth, xLeft, y, 300, m_hWnd, m_hFont);
    y += 35;

    CreateLabel(L"Panel Width (px):", xLeft, y, 170, m_hWnd, m_hFont);
    m_hEditPanelWidth = CreateEdit(std::to_wstring(cfg.panelWidth).c_str(), xRight, y, 60, m_hWnd, m_hFont);
    y += 45;

    m_hCheckShowCompletedPanel = CreateCheck(L"Show completed tasks in panel", cfg.showCompletedPanel, xLeft, y, 300, m_hWnd, m_hFont);
    y += 35;

    m_hCheckShowCompletedPopup = CreateCheck(L"Show completed tasks in popup", cfg.showCompletedPopup, xLeft, y, 300, m_hWnd, m_hFont);
    y += 45;

    CreateLabel(L"Overlay Right Margin (px):", xLeft, y, 170, m_hWnd, m_hFont);
    m_hEditMargin = CreateEdit(std::to_wstring(cfg.overlayRightMargin).c_str(), xRight, y, 60, m_hWnd, m_hFont);
    y += 50;

    m_hBtnSave = CreateWindowW(L"BUTTON", L"Save Config", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 
        150, y, 140, 35, m_hWnd, (HMENU)ID_BTN_SAVE_CFG, NULL, NULL);
}

void ConfigWindow::DrawCustomButton(LPDRAWITEMSTRUCT lp) {
    Gdiplus::Graphics g(lp->hDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

    RECT rc = lp->rcItem;
    Gdiplus::RectF rectF((Gdiplus::REAL)rc.left, (Gdiplus::REAL)rc.top, 
                         (Gdiplus::REAL)(rc.right - rc.left), (Gdiplus::REAL)(rc.bottom - rc.top));
    
    bool isHovered = (m_hoveredButtonId == (int)lp->CtlID);
    bool isPressed = (lp->itemState & ODS_SELECTED);

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

std::string ToUTF8(HWND hWnd) {
    int len = GetWindowTextLengthW(hWnd);
    if (len == 0) return "";
    std::wstring ws(len + 1, 0);
    GetWindowTextW(hWnd, &ws[0], len + 1);
    int uLen = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, NULL, 0, NULL, NULL);
    std::string s(uLen, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &s[0], uLen, NULL, NULL);
    if (!s.empty() && s.back() == 0) s.pop_back();
    return s;
}

std::wstring ToWStr(HWND hWnd) {
    int len = GetWindowTextLengthW(hWnd);
    if (len == 0) return L"";
    std::wstring ws(len + 1, 0);
    GetWindowTextW(hWnd, &ws[0], len + 1);
    if (!ws.empty() && ws.back() == 0) ws.pop_back();
    return ws;
}

int ToInt(HWND hWnd) {
    return _wtoi(ToWStr(hWnd).c_str());
}

bool IsChecked(HWND hWnd) {
    return SendMessage(hWnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void ConfigWindow::OnSave() {
    MemosClient::Config cfg = m_client->GetConfig();
    cfg.serverUrl = ToWStr(m_hEditServer);
    cfg.authToken = ToWStr(m_hEditToken);
    cfg.memoId = ToUTF8(m_hEditMemoId);
    cfg.refreshInterval = ToInt(m_hEditRefresh);
    cfg.scrollInterval = ToInt(m_hEditScroll);
    cfg.popupFontSize = ToInt(m_hEditPopupSize);
    cfg.panelFontSize = ToInt(m_hEditPanelSize);
    cfg.popupWidth = ToInt(m_hEditPopupWidth);
    cfg.panelWidth = ToInt(m_hEditPanelWidth);
    cfg.setPanelWidth = IsChecked(m_hCheckSetPanelWidth);
    cfg.showCompletedPanel = IsChecked(m_hCheckShowCompletedPanel);
    cfg.showCompletedPopup = IsChecked(m_hCheckShowCompletedPopup);
    cfg.overlayRightMargin = ToInt(m_hEditMargin);

    m_client->UpdateConfig(cfg);
    m_client->SaveConfig("config.json");
    
    // Notify application to reload (e.g. restart timers or redraw)
    // For now we'll just suggest a restart or trigger updates
    Hide();

    // Trigger immediate refresh in Overlay if available?
    // main.cpp usually owns everything. 
    // Actually the parent is OverlayWindow. 
    HWND parent = GetParent(m_hWnd);
    if (parent) PostMessage(parent, WM_TIMER, 3, 0); // Trigger Fetch
}

LRESULT CALLBACK ConfigWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ConfigWindow* pThis = (ConfigWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

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
        SetTextColor(hdc, COL_TEXT);
        SetBkColor(hdc, COL_BG);
        return (INT_PTR)pThis->m_hBrushBack;
    }
    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, COL_TEXT);
        SetBkColor(hdc, COL_INPUT_BG);
        return (INT_PTR)pThis->m_hBrushInput;
    }
    case WM_CTLCOLORBTN:
        return (INT_PTR)pThis->m_hBrushBack;

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

        if (pThis && newHover != pThis->m_hoveredButtonId) {
            int oldHover = pThis->m_hoveredButtonId;
            pThis->m_hoveredButtonId = newHover;
            
            if (oldHover != -1) InvalidateRect(GetDlgItem(hwnd, oldHover), NULL, TRUE);
            if (newHover != -1) {
                InvalidateRect(GetDlgItem(hwnd, newHover), NULL, TRUE);
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
        if (LOWORD(wParam) == ID_BTN_SAVE_CFG) pThis->OnSave();
        break;

    case WM_CLOSE:
        pThis->Hide();
        return 0;

    case WM_DESTROY:
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
