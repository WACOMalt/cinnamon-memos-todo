#include "TaskbarTracker.h"
#include <shellapi.h>

TaskbarTracker::TaskbarInfo TaskbarTracker::GetTaskbarInfo() {
    TaskbarInfo info = { 0 };
    info.edge = Edge::Unknown;

    // 1. Find the Taskbar Window
    // "Shell_TrayWnd" is the standard taskbar.
    // On multi-monitor setups, secondary taskbars are "Shell_SecondaryTrayWnd".
    // For this simple version, we stick to the primary one.
    info.hWnd = FindWindowW(L"Shell_TrayWnd", NULL);

    if (info.hWnd) {
        GetWindowRect(info.hWnd, &info.rect);

        // 2. Determine Edge and Working Area
        // We can deduce edge by looking at the rect coordinates relative to the screen
        // or using SHAppBarMessage.
        APPBARDATA abd = { 0 };
        abd.cbSize = sizeof(APPBARDATA);
        abd.hWnd = info.hWnd;
        
        // This message retrieves the taskbar position
        SHAppBarMessage(ABM_GETTASKBARPOS, &abd);
        
        switch (abd.uEdge) {
            case ABE_BOTTOM: info.edge = Edge::Bottom; break;
            case ABE_TOP:    info.edge = Edge::Top; break;
            case ABE_LEFT:   info.edge = Edge::Left; break;
            case ABE_RIGHT:  info.edge = Edge::Right; break;
        }

        // 3. Find System Tray (TrayNotifyWnd)
        // On Windows 11, the structure is complex and often XAML, 
        // but TrayNotifyWnd usually still exists as the container for the clock/icons.
        HWND hTray = FindWindowExW(info.hWnd, NULL, L"TrayNotifyWnd", NULL);
        if (hTray) {
            GetWindowRect(hTray, &info.trayRect);
        } else {
            // Fallback: Try to find it inside ReBarWindow32 (Old Win10 style)
            HWND hReBar = FindWindowExW(info.hWnd, NULL, L"ReBarWindow32", NULL);
            if (hReBar) {
                hTray = FindWindowExW(hReBar, NULL, L"TrayNotifyWnd", NULL);
                if (hTray) GetWindowRect(hTray, &info.trayRect);
                else SetRectEmpty(&info.trayRect);
            } else {
                SetRectEmpty(&info.trayRect);
            }
        }

        // Working Area
        HMONITOR hMonitor = MonitorFromWindow(info.hWnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = { 0 };
        mi.cbSize = sizeof(MONITORINFO);
        if (GetMonitorInfoW(hMonitor, &mi)) {
            info.workingArea = mi.rcWork;
        } else {
            // Fallback
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &info.workingArea, 0);
        }
    }

    return info;
}
