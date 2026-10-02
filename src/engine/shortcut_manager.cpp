#include "shortcut_manager.h"
#include <shlobj.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

static const wchar_t* kHardenedArgs = L"--no-default-browser-check --no-pings --disable-search-engine-choice-screen --disable-breakpad --disable-domain-reliability --disable-features=MediaEngagementBypassAutoplayPolicies,PreloadMediaEngagementData";

static const wchar_t* kGamingArgs = 
    L"--disable-frame-rate-limit "
    L"--disable-gpu-vsync "
    L"--disable-background-timer-throttling "
    L"--disable-backgrounding-occluded-windows "
    L"--enable-zero-copy "
    L"--ignore-gpu-blocklist "
    L"--enable-gpu-rasterization "
    L"--enable-oop-rasterization "
    L"--num-raster-threads=4 "
    L"--disable-renderer-backgrounding "
    L"--disable-features=CalculateNativeWinOcclusion,IntensiveWakeUpThrottling,MediaEngagementBypassAutoplayPolicies,PreloadMediaEngagementData "
    L"--no-default-browser-check "
    L"--no-pings "
    L"--disable-breakpad "
    L"--disable-domain-reliability "
    L"--disk-cache-size=104857600";

std::wstring ShortcutManager::GetHardenedArguments() {
    return kHardenedArgs;
}

std::wstring ShortcutManager::GetGamingArguments() {
    return kGamingArgs;
}

static void ScanDirectoryForShortcuts(
    const std::wstring& dirPath,
    const std::wstring& exeName,
    std::vector<std::wstring>& outLinks,
    int depth = 0
) {
    if (depth > 2 || !PathExists(dirPath)) return;

    std::wstring searchPattern = dirPath + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        std::wstring fullPath = dirPath + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                continue;
            ScanDirectoryForShortcuts(fullPath, exeName, outLinks, depth + 1);
        } else {
            std::wstring file = fd.cFileName;
            if (file.length() > 4 && _wcsicmp(file.c_str() + file.length() - 4, L".lnk") == 0) {
                std::wstring fileLower = file;
                std::transform(fileLower.begin(), fileLower.end(), fileLower.begin(), ::tolower);
                if (fileLower.find(L"gaming mode") != std::wstring::npos) continue;

                std::wstring exeLower = exeName;
                std::transform(exeLower.begin(), exeLower.end(), exeLower.begin(), ::tolower);

                bool match = false;
                if (exeLower.find(L"chrome") != std::wstring::npos && fileLower.find(L"chrome") != std::wstring::npos) match = true;
                else if (exeLower.find(L"brave") != std::wstring::npos && fileLower.find(L"brave") != std::wstring::npos) match = true;
                else if (exeLower.find(L"edge") != std::wstring::npos && fileLower.find(L"edge") != std::wstring::npos) match = true;

                if (!match) {
                    IShellLinkW* psl = NULL;
                    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl))) {
                        IPersistFile* ppf = NULL;
                        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf))) {
                            if (SUCCEEDED(ppf->Load(fullPath.c_str(), STGM_READ))) {
                                wchar_t targetPath[MAX_PATH] = { 0 };
                                psl->GetPath(targetPath, MAX_PATH, NULL, SLGP_RAWPATH);
                                std::wstring targetStr = targetPath;
                                std::transform(targetStr.begin(), targetStr.end(), targetStr.begin(), ::tolower);
                                if (!targetStr.empty() && targetStr.find(exeLower) != std::wstring::npos) match = true;
                            }
                            ppf->Release();
                        }
                        psl->Release();
                    }
                }

                if (match) {
                    outLinks.push_back(fullPath);
                }
            }
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

std::vector<std::wstring> ShortcutManager::FindBrowserShortcuts(const BrowserTarget& browser) {
    std::vector<std::wstring> results;
    CoInitialize(NULL);

    std::wstring exeName = L"chrome.exe";
    if (browser.id == L"brave") exeName = L"brave.exe";
    else if (browser.id == L"edge") exeName = L"msedge.exe";
    else if (!browser.exePath.empty()) {
        size_t lastSlash = browser.exePath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            exeName = browser.exePath.substr(lastSlash + 1);
        }
    }

    std::vector<int> csidls = {
        CSIDL_DESKTOPDIRECTORY,
        CSIDL_COMMON_DESKTOPDIRECTORY,
        CSIDL_PROGRAMS,
        CSIDL_COMMON_PROGRAMS
    };

    for (int csidl : csidls) {
        wchar_t path[MAX_PATH] = { 0 };
        if (SHGetFolderPathW(NULL, csidl, NULL, 0, path) == S_OK) {
            ScanDirectoryForShortcuts(path, exeName, results);
        }
    }

    // Check TaskBar pinned shortcuts
    wchar_t appData[MAX_PATH] = { 0 };
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData) == S_OK) {
        std::wstring tb = std::wstring(appData) + L"\\Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar";
        ScanDirectoryForShortcuts(tb, exeName, results);
    }

    return results;
}

bool ShortcutManager::HardenShortcuts(const BrowserTarget& browser, EngineLogCallback logCb) {
    CoInitialize(NULL);
    logCb(L"Scanning desktop and Start Menu for " + browser.name + L" shortcuts...", L"INFO");
    auto shortcuts = FindBrowserShortcuts(browser);
    logCb(L"Found " + std::to_wstring(shortcuts.size()) + L" shortcut candidate(s).", L"INFO");

    if (shortcuts.empty()) {
        logCb(L"[i] No desktop or Start Menu shortcuts found for " + browser.name + L".", L"INFO");
        return true;
    }

    int modified = 0;
    for (const auto& lnk : shortcuts) {
        IShellLinkW* psl = NULL;
        if (FAILED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl)))
            continue;

        IPersistFile* ppf = NULL;
        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf))) {
            DWORD attrs = GetFileAttributesW(lnk.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
                SetFileAttributesW(lnk.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
            }

            if (SUCCEEDED(ppf->Load(lnk.c_str(), STGM_READ))) {
                wchar_t args[2048] = { 0 };
                psl->GetArguments(args, 2048);

                std::wstring argsStr = args;
                if (argsStr.find(L"--no-pings") == std::wstring::npos) {
                    if (!argsStr.empty()) argsStr += L" ";
                    argsStr += kHardenedArgs;

                    psl->SetArguments(argsStr.c_str());
                    if (SUCCEEDED(ppf->Save(lnk.c_str(), TRUE))) {
                        modified++;
                        logCb(L"  [✓] Injected privacy flags into: " + lnk, L"INFO");
                    }
                }
            }
            ppf->Release();
        }
        psl->Release();
    }

    logCb(L"[✓] Shortcut hardening complete: " + std::to_wstring(modified) + L" shortcuts updated with startup privacy flags.", L"SUCCESS");
    return true;
}

bool ShortcutManager::RestoreShortcuts(const BrowserTarget& browser, EngineLogCallback logCb) {
    CoInitialize(NULL);
    auto shortcuts = FindBrowserShortcuts(browser);

    if (shortcuts.empty()) {
        logCb(L"[i] No shortcuts found for " + browser.name + L".", L"INFO");
        return true;
    }

    std::vector<std::wstring> flagsToRemove = {
        L"--no-pings",
        L"--disable-search-engine-choice-screen",
        L"--disable-breakpad",
        L"--disable-domain-reliability",
        L"--disable-features=MediaEngagementBypassAutoplayPolicies,PreloadMediaEngagementData"
    };

    int restored = 0;
    for (const auto& lnk : shortcuts) {
        IShellLinkW* psl = NULL;
        if (FAILED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl)))
            continue;

        IPersistFile* ppf = NULL;
        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf))) {
            DWORD attrs = GetFileAttributesW(lnk.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
                SetFileAttributesW(lnk.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
            }

            if (SUCCEEDED(ppf->Load(lnk.c_str(), STGM_READ))) {
                wchar_t args[2048] = { 0 };
                psl->GetArguments(args, 2048);

                std::wstring argsStr = args;
                bool changed = false;

                for (const auto& f : flagsToRemove) {
                    size_t pos = argsStr.find(f);
                    while (pos != std::wstring::npos) {
                        argsStr.erase(pos, f.length());
                        changed = true;
                        pos = argsStr.find(f);
                    }
                }

                // Trim leftover whitespace
                while (!argsStr.empty() && argsStr.front() == L' ') argsStr.erase(0, 1);
                while (!argsStr.empty() && argsStr.back() == L' ') argsStr.pop_back();

                if (changed) {
                    psl->SetArguments(argsStr.c_str());
                    if (SUCCEEDED(ppf->Save(lnk.c_str(), TRUE))) {
                        restored++;
                        logCb(L"  [✓] Restored standard shortcut: " + lnk, L"INFO");
                    }
                }
            }
            ppf->Release();
        }
        psl->Release();
    }

    logCb(L"[✓] Shortcut restoration complete: " + std::to_wstring(restored) + L" shortcuts reverted to default.", L"SUCCESS");
    return true;
}

bool ShortcutManager::LaunchSanitizedProfile(const BrowserTarget& browser, EngineLogCallback logCb) {
    if (browser.exePath.empty() || !PathExists(browser.exePath)) {
        logCb(L"[X] Browser executable not found: " + browser.exePath, L"ERROR");
        return false;
    }

    wchar_t tempPath[MAX_PATH] = { 0 };
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring sanitizedDir = std::wstring(tempPath) + L"ChromeDebloater_Sanitized_" + browser.id;

    std::wstring args = L"--user-data-dir=\"" + sanitizedDir + L"\" " +
                        L"--incognito --no-first-run --no-default-browser-check " +
                        kHardenedArgs +
                        L" --disable-sync --disable-features=OptimizationGuideModelDownloading,OptimizationHints";

    logCb(L"[*] Launching ephemeral sanitized session for " + browser.name + L"...", L"ACTION");
    HINSTANCE res = ShellExecuteW(NULL, L"open", browser.exePath.c_str(), args.c_str(), NULL, SW_SHOWNORMAL);

    if ((INT_PTR)res > 32) {
        logCb(L"[✓] Sanitized browser session active (Profile in %TEMP%, zero telemetry, auto-ephemeral).", L"SUCCESS");
        return true;
    } else {
        logCb(L"[X] Failed to launch browser process (Error code: " + std::to_wstring((INT_PTR)res) + L").", L"ERROR");
        return false;
    }
}

static bool CreateShortcutLink(
    const std::wstring& lnkPath,
    const std::wstring& targetExe,
    const std::wstring& args,
    const std::wstring& workingDir,
    const std::wstring& description
) {
    IShellLinkW* psl = NULL;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl)))
        return false;

    psl->SetPath(targetExe.c_str());
    psl->SetArguments(args.c_str());
    psl->SetWorkingDirectory(workingDir.c_str());
    psl->SetDescription(description.c_str());
    psl->SetIconLocation(targetExe.c_str(), 0);

    IPersistFile* ppf = NULL;
    bool success = false;
    if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf))) {
        if (SUCCEEDED(ppf->Save(lnkPath.c_str(), TRUE))) {
            success = true;
        }
        ppf->Release();
    }
    psl->Release();
    return success;
}

bool ShortcutManager::InstallGamingMode(const BrowserTarget& browser, EngineLogCallback logCb) {
    if (browser.exePath.empty() || !PathExists(browser.exePath)) {
        logCb(L"[X] Browser executable not found: " + browser.exePath, L"ERROR");
        return false;
    }

    CoInitialize(NULL);
    logCb(L"═══════════════════════════════════════════════════════════════", L"INFO");
    logCb(L"Installing Hardened Ultra Gaming Mode for " + browser.name + L"...", L"ACTION");

    // Extract directory and exeName
    std::wstring workingDir;
    std::wstring exeName = L"chrome.exe";
    size_t lastSlash = browser.exePath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        workingDir = browser.exePath.substr(0, lastSlash);
        exeName = browser.exePath.substr(lastSlash + 1);
    }

    // 1. Get Desktop and Start Menu paths
    wchar_t desktopPath[MAX_PATH] = { 0 };
    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopPath);

    wchar_t programsPath[MAX_PATH] = { 0 };
    SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, programsPath);

    std::wstring lnkFileName = browser.name + L" (Gaming Mode).lnk";
    std::wstring batFileName = browser.name + L" Gaming Mode.bat";

    // 2. Create Desktop Shortcut (.lnk)
    if (desktopPath[0] != 0) {
        std::wstring desktopLnk = std::wstring(desktopPath) + L"\\" + lnkFileName;
        if (CreateShortcutLink(desktopLnk, browser.exePath, kGamingArgs, workingDir,
                               browser.name + L" - Hardened Ultra Gaming Mode (Unlocked FPS, Zero VSync)")) {
            logCb(L"  [✓] Created Desktop Gaming Shortcut: " + desktopLnk, L"SUCCESS");
        }
    }

    // 3. Create Start Menu Shortcut (.lnk)
    if (programsPath[0] != 0) {
        std::wstring progLnk = std::wstring(programsPath) + L"\\" + lnkFileName;
        if (CreateShortcutLink(progLnk, browser.exePath, kGamingArgs, workingDir,
                               browser.name + L" - Hardened Ultra Gaming Mode (Unlocked FPS, Zero VSync)")) {
            logCb(L"  [✓] Created Start Menu Gaming Shortcut: " + progLnk, L"SUCCESS");
        }
    }

    // 4. Create Standalone Batch Launcher (.bat) requested by user
    if (desktopPath[0] != 0) {
        std::wstring desktopBat = std::wstring(desktopPath) + L"\\" + batFileName;
        std::wofstream batOut(desktopBat, std::ios::trunc);
        if (batOut.is_open()) {
            batOut << L"@echo off\r\n";
            batOut << L"title " << browser.name << L" - Hardened Ultra Gaming Mode\r\n";
            batOut << L"echo ========================================================\r\n";
            batOut << L"echo   Launching " << browser.name << L" in Hardened Gaming Mode\r\n";
            batOut << L"echo ========================================================\r\n";
            batOut << L"echo [*] Terminating lingering background instances...\r\n";
            batOut << L"taskkill /f /im " << exeName << L" 2>nul\r\n";
            batOut << L"echo [*] Launching with unlocked FPS, zero VSync ^& low-resource clamps...\r\n";
            batOut << L"start \"\" \"" << browser.exePath << L"\" " << kGamingArgs << L"\r\n";
            batOut << L"exit\r\n";
            batOut.close();
            logCb(L"  [✓] Created Standalone Batch Launcher: " + desktopBat, L"SUCCESS");
        }
    }

    // 5. Apply Gaming Mode Registry Enterprise Policies
    const std::wstring& p = browser.policyKey;
    TweakEngine::WriteDualRegDword(p, L"HardwareAccelerationModeEnabled", 1);
    TweakEngine::WriteDualRegDword(p, L"RendererProcessLimit", 4);
    TweakEngine::WriteDualRegDword(p, L"DiskCacheSize", 104857600); // 100MB clamp
    TweakEngine::WriteDualRegDword(p, L"NetworkPredictionOptions", 2); // Disable prefetching for lowest ping
    TweakEngine::WriteDualRegDword(p, L"BackgroundModeEnabled", 0); // No background apps when closed
    TweakEngine::WriteDualRegDword(p, L"HighEfficiencyModeEnabled", 1); // Aggressive tab sleeping
    TweakEngine::WriteDualRegDword(p, L"DefaultBrowserSettingEnabled", 0); // Silence default check
    TweakEngine::WriteDualRegDword(p, L"HideFirstRunExperience", 1); // Silence first run splash
    TweakEngine::WriteDualRegDword(p, L"PromotionsEnabled", 0);
    TweakEngine::WriteDualRegDword(p, L"SearchEngineChoiceScreenNavigationCondition", 0);
    logCb(L"  [✓] Enforced Gaming Policies: 4-Renderer clamp, 100MB cache, zero-latency network prediction & no background apps.", L"INFO");

    // 6. Touch First Run Marker
    if (PathExists(browser.userDataDir)) {
        std::wstring frPath = browser.userDataDir + L"\\First Run";
        if (!PathExists(frPath)) {
            std::ofstream fr(frPath, std::ios::binary);
            if (fr.is_open()) fr.close();
            logCb(L"  [✓] First Run marker created (silences onboarding & pin promos).", L"INFO");
        }

        // 7. Preferences JSON Patching
        std::wstring prefPath = browser.userDataDir + L"\\Default\\Preferences";
        if (PathExists(prefPath)) {
            std::ifstream in(prefPath, std::ios::binary);
            if (in.is_open()) {
                std::stringstream ss;
                ss << in.rdbuf();
                in.close();
                std::string s = ss.str();
                bool changed = false;

                auto replaceAll = [&](const std::string& from, const std::string& to) {
                    size_t pos = 0;
                    while ((pos = s.find(from, pos)) != std::string::npos) {
                        s.replace(pos, from.length(), to);
                        pos += to.length();
                        changed = true;
                    }
                };

                replaceAll("\"check_default_browser\":true", "\"check_default_browser\":false");
                replaceAll("\"has_seen_welcome_page\":false", "\"has_seen_welcome_page\":true");
                replaceAll("\"taskbar_pinning_promo_dismissed\":false", "\"taskbar_pinning_promo_dismissed\":true");

                if (changed) {
                    std::ofstream out(prefPath, std::ios::binary | std::ios::trunc);
                    if (out.is_open()) {
                        out << s;
                        out.close();
                        logCb(L"  [✓] Profile preferences patched for gaming: default nag & promos dismissed.", L"INFO");
                    }
                }
            }
        }
    }

    logCb(L"[✓] Chrome Gaming Mode installed and active! Double-click '" + lnkFileName + L"' or '" + batFileName + L"' on Desktop to game.", L"SUCCESS");
    return true;
}
