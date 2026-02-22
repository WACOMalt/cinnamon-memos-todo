#pragma once
#include <windows.h>

class TaskbarTracker {
public:
    enum class Edge {
        Bottom,
        Top,
        Left,
        Right,
        Unknown
    };

    struct TaskbarInfo {
        HWND hWnd;
        RECT rect;
        Edge edge;
        RECT workingArea; // Monitor working area
        RECT trayRect;    // System Tray / Notification Area
    };

    static TaskbarInfo GetTaskbarInfo();
};
