#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "browser_target.h"

struct BrowserUpdateInfo {
    std::wstring browserId;          // L"chrome", L"brave", L"edge"
    std::wstring browserName;        // L"Google Chrome", L"Brave Browser", L"Microsoft Edge"
    bool isInstalled = false;
    std::wstring installedVersion;   // e.g. L"154.0.8037.93"
    std::wstring latestVersion;      // e.g. L"155.0.8059.26"
    bool isUpdateLocked = false;     // True if updater policies or services block updates
    bool isUpToDate = false;         // True if installedVersion >= latestVersion
    std::wstring statusText;         // Human-readable summary
};

class UpdateChecker {
public:
    // Query online upstream version and local updater state for a browser
    static BrowserUpdateInfo CheckBrowser(const BrowserTarget& browser);

    // Query all 3 browsers
    static std::vector<BrowserUpdateInfo> CheckAll(const std::vector<BrowserTarget>& browsers);

    // Fetch raw HTTPS response string (using native WinHTTP)
    static std::string FetchHttps(const std::wstring& host, const std::wstring& path, WORD port = 443);

    // Parse version string from JSON key (e.g. "version", "tag_name", "ProductVersion")
    static std::wstring ExtractVersionFromJson(const std::string& json, const std::string& keyName);

    // Compare two version strings (e.g. "154.0.1.2" vs "155.0.0.0")
    // Returns 1 if v1 > v2, -1 if v1 < v2, 0 if v1 == v2
    static int CompareVersions(const std::wstring& v1, const std::wstring& v2);
};
