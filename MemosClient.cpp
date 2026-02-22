#include "MemosClient.h"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <fstream>
#include <shellapi.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

MemosClient::MemosClient(const Config& config) : m_config(config) {
}

MemosClient::~MemosClient() {
}

void MemosClient::UpdateConfig(const Config& newConfig) {
    m_config = newConfig;
}

bool MemosClient::LoadConfig(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open()) return false;
    
    try {
        nlohmann::json j;
        f >> j;

        // Helper to get wstring from json
        auto getWStr = [&](const std::string& key, const std::wstring& def) {
            std::string s = j.value(key, "");
            if (s.empty()) return def;
            int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
            std::wstring ws(len, 0);
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
            if (!ws.empty() && ws.back() == 0) ws.pop_back();
            return ws;
        };

        m_config.serverUrl = getWStr("serverUrl", m_config.serverUrl);
        m_config.authToken = getWStr("authToken", m_config.authToken);
        
        if (j.contains("memoId")) {
            if (j["memoId"].is_number()) m_config.memoId = std::to_string(j["memoId"].get<int>());
            else m_config.memoId = j.value("memoId", m_config.memoId);
        }

        m_config.refreshInterval = j.value("refreshInterval", m_config.refreshInterval);
        m_config.popupFontSize = j.value("popupFontSize", m_config.popupFontSize);
        m_config.panelFontSize = j.value("panelFontSize", m_config.panelFontSize);
        m_config.popupWidth = j.value("popupWidth", m_config.popupWidth);
        m_config.scrollInterval = j.value("scrollInterval", m_config.scrollInterval);
        m_config.setPanelWidth = j.value("setPanelWidth", m_config.setPanelWidth);
        m_config.panelWidth = j.value("panelWidth", m_config.panelWidth);
        m_config.showCompletedPanel = j.value("showCompletedPanel", m_config.showCompletedPanel);
        m_config.showCompletedPopup = j.value("showCompletedPopup", m_config.showCompletedPopup);
        m_config.transitionDuration = j.value("transitionDuration", m_config.transitionDuration);
        m_config.overlayRightMargin = j.value("overlayRightMargin", m_config.overlayRightMargin);

        return true;
    } catch (...) {
        return false;
    }
}

bool MemosClient::SaveConfig(const std::string& filename) {
    auto toUTF8 = [](const std::wstring& ws) {
        if (ws.empty()) return std::string("");
        int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, NULL, 0, NULL, NULL);
        std::string s(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &s[0], len, NULL, NULL);
        if (!s.empty() && s.back() == 0) s.pop_back();
        return s;
    };

    nlohmann::json j;
    j["serverUrl"] = toUTF8(m_config.serverUrl);
    j["authToken"] = toUTF8(m_config.authToken);
    j["memoId"] = m_config.memoId;
    j["refreshInterval"] = m_config.refreshInterval;
    j["popupFontSize"] = m_config.popupFontSize;
    j["panelFontSize"] = m_config.panelFontSize;
    j["popupWidth"] = m_config.popupWidth;
    j["scrollInterval"] = m_config.scrollInterval;
    j["setPanelWidth"] = m_config.setPanelWidth;
    j["panelWidth"] = m_config.panelWidth;
    j["showCompletedPanel"] = m_config.showCompletedPanel;
    j["showCompletedPopup"] = m_config.showCompletedPopup;
    j["transitionDuration"] = m_config.transitionDuration;
    j["overlayRightMargin"] = m_config.overlayRightMargin;

    std::ofstream f(filename);
    if (!f.is_open()) return false;
    f << j.dump(4);
    return true;
}

std::wstring MemosClient::CrackUrl(const std::wstring& url, std::wstring& pathOut) {
    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    
    wchar_t hostName[256];
    wchar_t urlPath[1024];

    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = ARRAYSIZE(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = ARRAYSIZE(urlPath);
    urlComp.dwSchemeLength = 1; // to get scheme

    if (WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp)) {
        pathOut = urlPath;
        return hostName;
    }
    return L"";
}

std::string MemosClient::SendRequest(const std::wstring& method, const std::wstring& path, const std::string& body) {
    std::wstring urlPath;
    std::wstring hostName = CrackUrl(m_config.serverUrl, urlPath);
    
    // Combine base path from URL with API path
    std::wstring fullPath = urlPath;
    if (fullPath.back() == L'/') fullPath.pop_back();
    fullPath += path;

    HINTERNET hSession = WinHttpOpen(L"MemosWindowsClient/1.0", 
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, 
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, hostName.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        // Try HTTP if HTTPS failed (though unlikely for secure servers)
        hConnect = WinHttpConnect(hSession, hostName.c_str(), INTERNET_DEFAULT_HTTP_PORT, 0);
    }

    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, method.c_str(), fullPath.c_str(),
                                            NULL, WINHTTP_NO_REFERER, 
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                            WINHTTP_FLAG_SECURE); // Try Secure first

    if (!hRequest) {
        // Fallback to non-secure?
        hRequest = WinHttpOpenRequest(hConnect, method.c_str(), fullPath.c_str(),
            NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            0);
    }

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Headers
    std::wstring tokenHeader = L"Authorization: Bearer " + m_config.authToken;
    WinHttpAddRequestHeaders(hRequest, tokenHeader.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL bResults = WinHttpSendRequest(hRequest,
                                       WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       (LPVOID)body.c_str(), (DWORD)body.length(),
                                       (DWORD)body.length(), 0);

    std::string response;
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        LPSTR pszOutBuffer;
        
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            pszOutBuffer = new char[dwSize + 1];
            if (!pszOutBuffer) break;

            ZeroMemory(pszOutBuffer, dwSize + 1);

            if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
                response.append(pszOutBuffer, dwDownloaded);
            }
            delete[] pszOutBuffer;
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

std::string MemosClient::GetMemoContent() {
    // Convert string ID to wstring
    int len = MultiByteToWideChar(CP_UTF8, 0, m_config.memoId.c_str(), -1, NULL, 0);
    std::wstring wMemoId(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, m_config.memoId.c_str(), -1, &wMemoId[0], len);
    if (!wMemoId.empty() && wMemoId.back() == 0) wMemoId.pop_back();

    std::wstring path = L"/api/v1/memos/" + wMemoId;
    std::string jsonStr = SendRequest(L"GET", path);
    
    if (jsonStr.empty()) return "";

    try {
        auto j = nlohmann::json::parse(jsonStr);
        // Handle different API versions/structures
        if (j.contains("content")) return j["content"].get<std::string>();
        if (j.contains("memo") && j["memo"].contains("content")) return j["memo"]["content"].get<std::string>();
        // Newer Memos v0.22+ might use 'name' or other structure, but let's assume standard for now or check 'content'
        return ""; 
    } catch (...) {
        return "";
    }
}

void MemosClient::OpenInBrowser() {
    // Construct URL: serverUrl + /m/ + memoId
    // serverUrl usually has no trailing slash or does? w/e
    // wstring conversion
    int len = MultiByteToWideChar(CP_UTF8, 0, m_config.memoId.c_str(), -1, NULL, 0);
    std::wstring wMemoId(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, m_config.memoId.c_str(), -1, &wMemoId[0], len);
    if (!wMemoId.empty() && wMemoId.back() == 0) wMemoId.pop_back();

    std::wstring target = m_config.serverUrl;
    if (!target.empty() && target.back() != L'/') target += L"/";
    target += L"m/" + wMemoId;

    ShellExecuteW(NULL, L"open", target.c_str(), NULL, NULL, SW_SHOW);
}

bool MemosClient::UpdateMemoContent(const std::string& newContent) {
    // Convert string ID to wstring
    int len = MultiByteToWideChar(CP_UTF8, 0, m_config.memoId.c_str(), -1, NULL, 0);
    std::wstring wMemoId(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, m_config.memoId.c_str(), -1, &wMemoId[0], len);
    if (!wMemoId.empty() && wMemoId.back() == 0) wMemoId.pop_back();

    std::wstring path = L"/api/v1/memos/" + wMemoId;
    
    nlohmann::json j;
    j["content"] = newContent;
    
    std::string body = j.dump();
    std::string response = SendRequest(L"PATCH", path, body);
    
    return !response.empty();
}
