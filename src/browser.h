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
    bool isInstalled;
};

inline std::wstring GetKnownFolderPathLocalApp() {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path))) {
        std::wstring res(path);
        CoTaskMemFree(path);
        return res;
    }
    return L"";
}

inline std::wstring GetProgramFilesX86Path() {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFilesX86, 0, NULL, &path))) {
        std::wstring res(path);
        CoTaskMemFree(path);
        return res;
    }
    return L"C:\\Program Files (x86)";
}

inline std::wstring GetProgramFilesPath() {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, NULL, &path))) {
        std::wstring res(path);
        CoTaskMemFree(path);
        return res;
    }
    return L"C:\\Program Files";
}

inline bool FileOrDirExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES);
}

inline std::vector<BrowserTarget> DetectBrowsers() {
    std::vector<BrowserTarget> targets;
    std::wstring localApp = GetKnownFolderPathLocalApp();
    std::wstring pf = GetProgramFilesPath();
    std::wstring pf86 = GetProgramFilesX86Path();

    // 1. Google Chrome
    BrowserTarget chrome;
    chrome.id = L"chrome";
    chrome.name = L"Google Chrome";
    chrome.policyKey = L"SOFTWARE\\Policies\\Google\\Chrome";
    chrome.updateKey = L"SOFTWARE\\Policies\\Google\\Update";
    chrome.appGuid = L"{8A69D345-D564-463C-AFF1-A69D9E530F96}";
    chrome.userDataDir = localApp + L"\\Google\\Chrome\\User Data";
    
    std::wstring chromeExe1 = pf + L"\\Google\\Chrome\\Application\\chrome.exe";
    std::wstring chromeExe2 = pf86 + L"\\Google\\Chrome\\Application\\chrome.exe";
    if (FileOrDirExists(chromeExe1)) {
        chrome.exePath = chromeExe1;
        chrome.isInstalled = true;
    } else if (FileOrDirExists(chromeExe2)) {
        chrome.exePath = chromeExe2;
        chrome.isInstalled = true;
    } else {
        chrome.exePath = chromeExe1;
        chrome.isInstalled = FileOrDirExists(chrome.userDataDir);
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
    std::wstring braveExe1 = pf + L"\\BraveSoftware\\Brave-Browser\\Application\\brave.exe";
    std::wstring braveExe2 = pf86 + L"\\BraveSoftware\\Brave-Browser\\Application\\brave.exe";
    if (FileOrDirExists(braveExe1)) {
        brave.exePath = braveExe1;
        brave.isInstalled = true;
    } else if (FileOrDirExists(braveExe2)) {
        brave.exePath = braveExe2;
        brave.isInstalled = true;
    } else {
        brave.exePath = braveExe1;
        brave.isInstalled = FileOrDirExists(brave.userDataDir);
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
    std::wstring edgeExe1 = pf86 + L"\\Microsoft\\Edge\\Application\\msedge.exe";
    std::wstring edgeExe2 = pf + L"\\Microsoft\\Edge\\Application\\msedge.exe";
    if (FileOrDirExists(edgeExe1)) {
        edge.exePath = edgeExe1;
        edge.isInstalled = true;
    } else if (FileOrDirExists(edgeExe2)) {
        edge.exePath = edgeExe2;
        edge.isInstalled = true;
    } else {
        edge.exePath = edgeExe1;
        edge.isInstalled = FileOrDirExists(edge.userDataDir);
    }
    targets.push_back(edge);

    return targets;
}
