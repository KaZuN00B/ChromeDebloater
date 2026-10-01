#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <shlobj.h>

struct BrowserTarget {
    std::wstring id;            // L"chrome", L"brave", L"edge"
    std::wstring name;          // L"Google Chrome"
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
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES);
}

inline std::wstring ExtractFileVersion(const std::wstring& exePath) {
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

    // 1. Google Chrome
    BrowserTarget chrome;
    chrome.id = L"chrome";
    chrome.name = L"Google Chrome";
    chrome.policyKey = L"SOFTWARE\\Policies\\Google\\Chrome";
    chrome.updateKey = L"SOFTWARE\\Policies\\Google\\Update";
    chrome.appGuid = L"{8A69D345-D564-463C-AFF1-A69D9E530F96}";
    chrome.userDataDir = localApp + L"\\Google\\Chrome\\User Data";
    
    std::wstring chrome1 = pf + L"\\Google\\Chrome\\Application\\chrome.exe";
    std::wstring chrome2 = pf86 + L"\\Google\\Chrome\\Application\\chrome.exe";
    if (PathExists(chrome1)) {
        chrome.exePath = chrome1;
        chrome.isInstalled = true;
    } else if (PathExists(chrome2)) {
        chrome.exePath = chrome2;
        chrome.isInstalled = true;
    } else {
        chrome.exePath = chrome1;
        chrome.isInstalled = PathExists(chrome.userDataDir);
    }
    if (chrome.isInstalled && PathExists(chrome.exePath)) {
        chrome.version = ExtractFileVersion(chrome.exePath);
    }
    targets.push_back(chrome);

    // 2. Brave Browser
    BrowserTarget brave;
    brave.id = L"brave";
    brave.name = L"Brave Browser";
    brave.policyKey = L"SOFTWARE\\Policies\\BraveSoftware\\Brave";
    brave.updateKey = L"SOFTWARE\\Policies\\BraveSoftware\\Update";
    brave.appGuid = L"{AFE6A462-EE30-4225-B0AF-60F950922C44}";
    brave.userDataDir = localApp + L"\\BraveSoftware\\Brave-Browser\\User Data";
    std::wstring brave1 = pf + L"\\BraveSoftware\\Brave-Browser\\Application\\brave.exe";
    std::wstring brave2 = pf86 + L"\\BraveSoftware\\Brave-Browser\\Application\\brave.exe";
    if (PathExists(brave1)) {
        brave.exePath = brave1;
        brave.isInstalled = true;
    } else if (PathExists(brave2)) {
        brave.exePath = brave2;
        brave.isInstalled = true;
    } else {
        brave.exePath = brave1;
        brave.isInstalled = PathExists(brave.userDataDir);
    }
    if (brave.isInstalled && PathExists(brave.exePath)) {
        brave.version = ExtractFileVersion(brave.exePath);
    }
    targets.push_back(brave);

    // 3. Microsoft Edge
    BrowserTarget edge;
    edge.id = L"edge";
    edge.name = L"Microsoft Edge";
    edge.policyKey = L"SOFTWARE\\Policies\\Microsoft\\Edge";
    edge.updateKey = L"SOFTWARE\\Policies\\Microsoft\\EdgeUpdate";
    edge.appGuid = L"{F3017226-FE2A-4295-8BDF-F600A0E7E5A4}";
    edge.userDataDir = localApp + L"\\Microsoft\\Edge\\User Data";
    std::wstring edge1 = pf86 + L"\\Microsoft\\Edge\\Application\\msedge.exe";
    std::wstring edge2 = pf + L"\\Microsoft\\Edge\\Application\\msedge.exe";
    if (PathExists(edge1)) {
        edge.exePath = edge1;
        edge.isInstalled = true;
    } else if (PathExists(edge2)) {
        edge.exePath = edge2;
        edge.isInstalled = true;
    } else {
        edge.exePath = edge1;
        edge.isInstalled = PathExists(edge.userDataDir);
    }
    if (edge.isInstalled && PathExists(edge.exePath)) {
        edge.version = ExtractFileVersion(edge.exePath);
    }
    targets.push_back(edge);

    return targets;
}
