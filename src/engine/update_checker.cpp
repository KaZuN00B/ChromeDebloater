#include "update_checker.h"
#include "audit_engine.h"
#include <winhttp.h>
#include <sstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")

std::string UpdateChecker::FetchHttps(const std::wstring& host, const std::wstring& path, WORD port) {
    std::string response;

    HINTERNET hSession = WinHttpOpen(
        L"ChromeDebloater-UpdateChecker/3.2 (Windows NT; x64)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) return "";

    // Set fast 4-second timeouts so network disconnects never hang the app
    WinHttpSetTimeouts(hSession, 3000, 3000, 4000, 4000);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        path.c_str(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // GitHub API requires Accept header and recommends User-Agent
    LPCWSTR headers = L"Accept: application/json\r\nUser-Agent: ChromeDebloater\r\n";
    BOOL sent = WinHttpSendRequest(hRequest, headers, (DWORD)-1L, NULL, 0, 0, 0);
    if (sent && WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD dwSize = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            std::vector<char> buffer(dwSize + 1, 0);
            DWORD dwDownloaded = 0;
            if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                response.append(buffer.data(), dwDownloaded);
            }
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

std::wstring UpdateChecker::ExtractVersionFromJson(const std::string& json, const std::string& keyName) {
    if (json.empty()) return L"";

    // Search for "keyName":" or "keyName": "
    std::string needle1 = "\"" + keyName + "\":\"";
    std::string needle2 = "\"" + keyName + "\": \"";
    size_t pos = json.find(needle1);
    size_t offset = needle1.length();

    if (pos == std::string::npos) {
        pos = json.find(needle2);
        offset = needle2.length();
    }
    if (pos == std::string::npos) return L"";

    size_t start = pos + offset;
    // Skip optional 'v' prefix (e.g. v1.73.91)
    if (start < json.length() && (json[start] == 'v' || json[start] == 'V')) {
        start++;
    }

    size_t end = json.find("\"", start);
    if (end == std::string::npos) return L"";

    std::string verStr = json.substr(start, end - start);
    // Convert to wstring
    return std::wstring(verStr.begin(), verStr.end());
}

int UpdateChecker::CompareVersions(const std::wstring& v1, const std::wstring& v2) {
    if (v1 == v2) return 0;
    if (v1.empty() && !v2.empty()) return -1;
    if (!v1.empty() && v2.empty()) return 1;

    std::wstringstream ss1(v1), ss2(v2);
    std::wstring part1, part2;

    while (true) {
        bool has1 = (bool)std::getline(ss1, part1, L'.');
        bool has2 = (bool)std::getline(ss2, part2, L'.');

        if (!has1 && !has2) break;

        int num1 = has1 ? std::stoi(part1) : 0;
        int num2 = has2 ? std::stoi(part2) : 0;

        if (num1 > num2) return 1;
        if (num1 < num2) return -1;
    }
    return 0;
}

BrowserUpdateInfo UpdateChecker::CheckBrowser(const BrowserTarget& browser) {
    BrowserUpdateInfo info;
    info.browserId = browser.id;
    info.browserName = browser.name;
    info.isInstalled = browser.isInstalled;
    info.installedVersion = browser.version;

    if (!browser.isInstalled) {
        info.statusText = L"Not installed on this machine";
        return info;
    }

    // 1. Check local lockdown state (Registry & Services)
    DWORD lockVal = 1;
    bool hklmLocked = AuditEngine::ReadRegDword(HKEY_LOCAL_MACHINE, browser.updateKey, L"UpdateDefault", lockVal) && (lockVal == 0);
    DWORD hkcuLock = 1;
    bool hkcuLocked = AuditEngine::ReadRegDword(HKEY_CURRENT_USER, browser.updateKey, L"UpdateDefault", hkcuLock) && (hkcuLock == 0);
    info.isUpdateLocked = (hklmLocked || hkcuLocked);

    // 2. Query upstream release APIs
    std::wstring latest;
    if (browser.id == L"chrome") {
        std::string json = FetchHttps(L"chromiumdash.appspot.com", L"/fetch_releases?channel=Stable&platform=Windows&num=1");
        latest = ExtractVersionFromJson(json, "version");
    } else if (browser.id == L"brave") {
        std::string json = FetchHttps(L"api.github.com", L"/repos/brave/brave-browser/releases/latest");
        latest = ExtractVersionFromJson(json, "tag_name");
        if (latest.empty()) {
            latest = ExtractVersionFromJson(json, "name");
        }
    } else if (browser.id == L"edge") {
        std::string json = FetchHttps(L"edgeupdates.microsoft.com", L"/api/products");
        latest = ExtractVersionFromJson(json, "ProductVersion");
    }

    info.latestVersion = latest;

    // 3. Determine update status
    if (latest.empty()) {
        if (info.isUpdateLocked) {
            info.statusText = L"Frozen at v" + info.installedVersion + L" (Lockdown Active · Offline/API Timeout)";
        } else {
            info.statusText = L"Installed: v" + info.installedVersion + L" (Upstream API unreachable)";
        }
        info.isUpToDate = false;
        return info;
    }

    int cmp = CompareVersions(info.installedVersion, latest);
    info.isUpToDate = (cmp >= 0);

    if (info.isUpdateLocked) {
        if (info.isUpToDate) {
            info.statusText = L"Protected & Frozen at latest v" + info.installedVersion;
        } else {
            info.statusText = L"Locked at v" + info.installedVersion + L" (Upstream v" + latest + L" blocked by policy)";
        }
    } else {
        if (info.isUpToDate) {
            info.statusText = L"Up to date (v" + info.installedVersion + L" matches latest upstream)";
        } else {
            info.statusText = L"Newer version available: v" + latest + L" (Current: v" + info.installedVersion + L")";
        }
    }

    return info;
}

std::vector<BrowserUpdateInfo> UpdateChecker::CheckAll(const std::vector<BrowserTarget>& browsers) {
    std::vector<BrowserUpdateInfo> results;
    for (const auto& b : browsers) {
        results.push_back(CheckBrowser(b));
    }
    return results;
}
