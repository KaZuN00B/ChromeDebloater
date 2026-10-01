#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <shlobj.h>

struct BrowserTarget {
    std::wstring id;            // L"chrome", L"brave", L"edge"
    std::wstring name;          // L"Google Chrome", L"Brave Browser", L"Microsoft Edge"
    std::wstring policyKey;     // L"SOFTWARE\\Policies\\Google\\Chrome"
    std::wstring updateKey;     // L"SOFTWARE\\Policies\\Google\\Update"
    std::wstring appGuid;       // L"{8A69D345-D564-463C-AFF1-A69D9E530F96}"
    std::wstring exePath;       // Path to executable
    std::wstring userDataDir;   // Path to User Data
    std::wstring version;       // e.g. L"154.0.8037.93"
    bool isInstalled = false;
};

inline std::wstring GetKnownFolderLocalApp() {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path))) {
        std::wstring res(path);
        CoTaskMemFree(path);
        return res;
    }
    return L"";
}

inline std::wstring GetPFX86() {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFilesX86, 0, NULL, &path))) {
        std::wstring res(path);
        CoTaskMemFree(path);
        return res;
    }
    return L"C:\\Program Files (x86)";
}

inline std::wstring GetPF() {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, NULL, &path))) {
        std::wstring res(path);
        CoTaskMemFree(path);
        return res;
    }
    return L"C:\\Program Files";
}

inline bool PathExists(const std::wstring& path) {
    if (path.empty()) return false;
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES);
}

inline std::wstring GetAppPathFromRegistry(const wchar_t* exeName) {
    HKEY roots[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    std::wstring sub = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + std::wstring(exeName);
    for (HKEY root : roots) {
        HKEY hKey;
        if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
            wchar_t path[MAX_PATH] = { 0 };
            DWORD size = sizeof(path);
            if (RegQueryValueExW(hKey, NULL, 0, NULL, (LPBYTE)path, &size) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                std::wstring clean = path;
                if (!clean.empty() && clean.front() == L'"' && clean.back() == L'"') {
                    clean = clean.substr(1, clean.length() - 2);
                }
                if (PathExists(clean)) return clean;
            }
            RegCloseKey(hKey);
        }
    }
    return L"";
}

inline std::wstring GetVersionFromRegistry(HKEY root, const std::wstring& subKey) {
    HKEY hKey;
    if (RegOpenKeyExW(root, subKey.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[128] = { 0 };
        DWORD size = sizeof(buf);
        if (RegQueryValueExW(hKey, L"pv", 0, NULL, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return buf;
        }
        size = sizeof(buf);
        if (RegQueryValueExW(hKey, L"version", 0, NULL, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return buf;
        }
        RegCloseKey(hKey);
    }
    return L"";
}

inline std::wstring ExtractFileVersion(const std::wstring& exePath) {
    if (exePath.empty() || !PathExists(exePath)) return L"";
    DWORD dummy;
    DWORD size = GetFileVersionInfoSizeW(exePath.c_str(), &dummy);
    if (size == 0) return L"";

    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(exePath.c_str(), 0, size, data.data())) return L"";

    VS_FIXEDFILEINFO* pFileInfo = nullptr;
    UINT len = 0;
    if (VerQueryValueW(data.data(), L"\\", (LPVOID*)&pFileInfo, &len) && pFileInfo) {
        wchar_t buf[64];
        swprintf_s(buf, L"%d.%d.%d.%d",
            HIWORD(pFileInfo->dwProductVersionMS),
            LOWORD(pFileInfo->dwProductVersionMS),
            HIWORD(pFileInfo->dwProductVersionLS),
            LOWORD(pFileInfo->dwProductVersionLS)
        );
        return buf;
    }
    return L"";
}

inline std::vector<BrowserTarget> DetectAllBrowsers() {
    std::vector<BrowserTarget> targets;
    std::wstring localApp = GetKnownFolderLocalApp();
    std::wstring pf = GetPF();
    std::wstring pf86 = GetPFX86();

    // ─────────────────────────────────────────────────────────────────────────
    // 1. Google Chrome (All channels & install locations)
    // ─────────────────────────────────────────────────────────────────────────
    BrowserTarget chrome;
    chrome.id = L"chrome";
    chrome.name = L"Google Chrome";
    chrome.policyKey = L"SOFTWARE\\Policies\\Google\\Chrome";
    chrome.updateKey = L"SOFTWARE\\Policies\\Google\\Update";
    chrome.appGuid = L"{8A69D345-D564-463C-AFF1-A69D9E530F96}";
    chrome.userDataDir = localApp + L"\\Google\\Chrome\\User Data";

    std::vector<std::wstring> chromeCandidates = {
        GetAppPathFromRegistry(L"chrome.exe"),
        pf + L"\\Google\\Chrome\\Application\\chrome.exe",
        pf86 + L"\\Google\\Chrome\\Application\\chrome.exe",
        localApp + L"\\Google\\Chrome\\Application\\chrome.exe",
        localApp + L"\\Google\\Chrome SxS\\Application\\chrome.exe",
        pf + L"\\Google\\Chrome Beta\\Application\\chrome.exe"
    };

    for (const auto& path : chromeCandidates) {
        if (!path.empty() && PathExists(path)) {
            chrome.exePath = path;
            chrome.isInstalled = true;
            break;
        }
    }
    if (!chrome.isInstalled && PathExists(chrome.userDataDir)) {
        chrome.isInstalled = true;
        chrome.exePath = chromeCandidates[1];
    }
    if (chrome.isInstalled) {
        chrome.version = ExtractFileVersion(chrome.exePath);
        if (chrome.version.empty()) {
            chrome.version = GetVersionFromRegistry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Google\\Update\\Clients\\" + chrome.appGuid);
        }
    }
    targets.push_back(chrome);

    // ─────────────────────────────────────────────────────────────────────────
    // 2. Brave Browser (All channels & install locations)
    // ─────────────────────────────────────────────────────────────────────────
    BrowserTarget brave;
    brave.id = L"brave";
    brave.name = L"Brave Browser";
    brave.policyKey = L"SOFTWARE\\Policies\\BraveSoftware\\Brave";
    brave.updateKey = L"SOFTWARE\\Policies\\BraveSoftware\\Update";
    brave.appGuid = L"{AFE6A462-EE30-4225-B0AF-60F950922C44}";
    brave.userDataDir = localApp + L"\\BraveSoftware\\Brave-Browser\\User Data";

    std::vector<std::wstring> braveCandidates = {
        GetAppPathFromRegistry(L"brave.exe"),
        pf + L"\\BraveSoftware\\Brave-Browser\\Application\\brave.exe",
        pf86 + L"\\BraveSoftware\\Brave-Browser\\Application\\brave.exe",
        localApp + L"\\BraveSoftware\\Brave-Browser\\Application\\brave.exe",
        localApp + L"\\BraveSoftware\\Brave-Browser-Beta\\Application\\brave.exe",
        localApp + L"\\BraveSoftware\\Brave-Browser-Nightly\\Application\\brave.exe"
    };

    for (const auto& path : braveCandidates) {
        if (!path.empty() && PathExists(path)) {
            brave.exePath = path;
            brave.isInstalled = true;
            break;
        }
    }
    if (!brave.isInstalled && PathExists(brave.userDataDir)) {
        brave.isInstalled = true;
        brave.exePath = braveCandidates[1];
    }
    if (brave.isInstalled) {
        brave.version = ExtractFileVersion(brave.exePath);
        if (brave.version.empty()) {
            brave.version = GetVersionFromRegistry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\BraveSoftware\\Update\\Clients\\" + brave.appGuid);
        }
    }
    targets.push_back(brave);

    // ─────────────────────────────────────────────────────────────────────────
    // 3. Microsoft Edge (All channels & install locations)
    // ─────────────────────────────────────────────────────────────────────────
    BrowserTarget edge;
    edge.id = L"edge";
    edge.name = L"Microsoft Edge";
    edge.policyKey = L"SOFTWARE\\Policies\\Microsoft\\Edge";
    edge.updateKey = L"SOFTWARE\\Policies\\Microsoft\\EdgeUpdate";
    edge.appGuid = L"{F3017226-FE2A-4295-8BDF-F600A0E7E5A4}";
    edge.userDataDir = localApp + L"\\Microsoft\\Edge\\User Data";

    std::vector<std::wstring> edgeCandidates = {
        GetAppPathFromRegistry(L"msedge.exe"),
        pf86 + L"\\Microsoft\\Edge\\Application\\msedge.exe",
        pf + L"\\Microsoft\\Edge\\Application\\msedge.exe",
        localApp + L"\\Microsoft\\Edge\\Application\\msedge.exe",
        localApp + L"\\Microsoft\\Edge SxS\\Application\\msedge.exe",
        localApp + L"\\Microsoft\\Edge Beta\\Application\\msedge.exe",
        localApp + L"\\Microsoft\\Edge Dev\\Application\\msedge.exe"
    };

    for (const auto& path : edgeCandidates) {
        if (!path.empty() && PathExists(path)) {
            edge.exePath = path;
            edge.isInstalled = true;
            break;
        }
    }
    if (!edge.isInstalled && PathExists(edge.userDataDir)) {
        edge.isInstalled = true;
        edge.exePath = edgeCandidates[1];
    }
    if (edge.isInstalled) {
        edge.version = ExtractFileVersion(edge.exePath);
        if (edge.version.empty()) {
            edge.version = GetVersionFromRegistry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\" + edge.appGuid);
        }
    }
    targets.push_back(edge);

    return targets;
}
