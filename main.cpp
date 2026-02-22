#include <windows.h>
#include <fstream>
#include "json.hpp"
#include "OverlayWindow.h"
#include "MemosClient.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // MessageBoxW(NULL, L"MemosTodo Windows - v13", L"Launch Status", MB_OK);

    // 1. Load Config
    // 1. Load Config
    MemosClient::Config clientConfig;
    MemosClient client(clientConfig);
    
    if (!client.LoadConfig("config.json")) {
        // Create default config
        client.SaveConfig("config.json");

        // Notify and Open
        int msgboxID = MessageBoxW(NULL, 
            L"config.json was missing or invalid and has been created.\n\nWould you like to open it now to configure your server details?", 
            L"Configuration Required", 
            MB_ICONINFORMATION | MB_YESNO);

        if (msgboxID == IDYES) {
            ShellExecuteW(NULL, L"open", L"config.json", NULL, NULL, SW_SHOW);
        }
        return 0;
    }

    // 2. Init UI
    OverlayWindow overlay(hInstance, &client);
    
    if (!overlay.Create()) {
        MessageBoxW(NULL, L"Failed to create Overlay Window", L"Error", MB_ICONERROR);
        return 0;
    }

    overlay.Show();

    // 3. Message Loop
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
