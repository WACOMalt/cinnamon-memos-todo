#pragma once
#include <string>
#include <vector>
#include <functional>
#include "json.hpp" // nlohmann/json.hpp

class MemosClient {
public:
    struct Config {
        std::wstring serverUrl = L"https://demo.usememos.com/";
        std::wstring authToken;
        std::string memoId = "1";
        int refreshInterval = 10;     // minutes
        int popupFontSize = 11;       // pt
        int panelFontSize = 10;       // pt
        int popupWidth = 340;         // px
        int scrollInterval = 5;       // seconds
        bool setPanelWidth = false;
        int panelWidth = 150;         // px
        bool showCompletedPanel = true;
        bool showCompletedPopup = true;
        int transitionDuration = 300; // ms
        int overlayRightMargin = 400; // px
    };

    MemosClient(const Config& config);
    ~MemosClient();

    const Config& GetConfig() const { return m_config; }
    void UpdateConfig(const Config& newConfig);
    bool LoadConfig(const std::string& filename);
    bool SaveConfig(const std::string& filename);

    // Callback for async operations or just simple sync for now
    std::string GetMemoContent();
    bool UpdateMemoContent(const std::string& newContent);
    void OpenInBrowser();

private:
    Config m_config;
    
    // Helpers
    std::string SendRequest(const std::wstring& method, const std::wstring& path, const std::string& body = "");
    std::wstring CrackUrl(const std::wstring& url, std::wstring& pathOut);
};
