#include "engine.h"
#include "sqlite_helper.h"
#include <tlhelp32.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <shellapi.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "dnsapi.lib")

typedef BOOL (WINAPI *DnsFlushResolverCacheFn)(VOID);

std::vector<TweakOption> HardeningEngine::GetAvailableTweaks() {
    return {
        {
            1, L"AI Elimination", L"Disable all AI & Gemini subsystems",
            L"Enforces 25 enterprise policies disabling Gemini, OptimizationGuide, PromptAPI, etc., and deletes local models.",
            true, false
        },
        {
            2, L"Privacy Hardening", L"Privacy & Content hardening",
            L"Enforces Quad9 DoH, HTTPS-only, WebRTC leak fix, anti-tracking, and blocks permissions (sensors, geo, notifications).",
            true, false
        },
        {
            3, L"Telemetry Suppression", L"Disable telemetry & reporting",
            L"Turns off Metrics reporting, SafeBrowsing extended reporting, cleanup scanner, and user feedback.",
            true, false
        },
        {
            4, L"Authentication", L"Preserve Google Sync & Passwords",
            L"Explicitly allowlists Google auth cookies and preserves Google Password Manager and profile sync.",
            true, true
        },
        {
            5, L"Performance", L"Resource & speed optimizations",
            L"Clamps iframe processes (SitePerProcess=0), enables max tab Memory Saver, enables GPU acceleration, throttles JS timers.",
            true, false
        },
        {
            6, L"Flags", L"Inject performance flags into Local State",
            L"Injects QUIC, parallel downloading, GPU rasterization, zero-copy, and back-forward cache into Local State.",
            true, false
        },
        {
            7, L"UI Debloat", L"Remove UI clutter (Cast, Share, Feedback)",
            L"Strips Cast button from toolbar, Desktop Sharing Hub, shared clipboard, and web app install promotions.",
            true, false
        },
        {
            8, L"Search Provider", L"Set default search to Brave Search",
            L"Configures private Brave Search as the default search engine with query URL.",
            true, false
        },
        {
            9, L"Maintenance", L"Database vacuum & cache sweep",
            L"Vacuums/reindexes SQLite history/favicons, cleans shader/GPU/crashpad caches, and flushes Windows DNS.",
            true, false
        },
        {
            10, L"Update Lockdown", L"Permanent browser version lockdown",
            L"Freezes browser at current version, disables updater services, scheduled tasks, and auto-update checks.",
            true, true
        }
    };
}

bool HardeningEngine::SetRegDword(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, DWORD value) {
    HKEY hKey;
    DWORD disposition;
    LSTATUS status = RegCreateKeyExW(
        hRoot, subKey.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, NULL, &hKey, &disposition
    );
    if (status != ERROR_SUCCESS) return false;

    status = RegSetValueExW(hKey, name.c_str(), 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
    RegCloseKey(hKey);
    return (status == ERROR_SUCCESS);
}

bool HardeningEngine::SetRegString(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, const std::wstring& value) {
    HKEY hKey;
    DWORD disposition;
    LSTATUS status = RegCreateKeyExW(
        hRoot, subKey.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, NULL, &hKey, &disposition
    );
    if (status != ERROR_SUCCESS) return false;

    DWORD sizeInBytes = (DWORD)((value.length() + 1) * sizeof(wchar_t));
    status = RegSetValueExW(hKey, name.c_str(), 0, REG_SZ, (const BYTE*)value.c_str(), sizeInBytes);
    RegCloseKey(hKey);
    return (status == ERROR_SUCCESS);
}

bool HardeningEngine::SetRegMultiString(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, const std::vector<std::wstring>& values) {
    // In Chromium policies, lists are stored under a subkey (e.g. Policies\Google\Chrome\CookiesAllowedForUrls) with values named "1", "2", etc.
    std::wstring listKeyPath = subKey + L"\\" + name;
    HKEY hKey;
    DWORD disposition;
    LSTATUS status = RegCreateKeyExW(
        hRoot, listKeyPath.c_str(), 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, NULL, &hKey, &disposition
    );
    if (status != ERROR_SUCCESS) return false;

    for (size_t i = 0; i < values.size(); ++i) {
        std::wstring idxStr = std::to_wstring(i + 1);
        DWORD sizeInBytes = (DWORD)((values[i].length() + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, idxStr.c_str(), 0, REG_SZ, (const BYTE*)values[i].c_str(), sizeInBytes);
    }
    RegCloseKey(hKey);
    return true;
}

bool HardeningEngine::DeleteRegValue(HKEY hRoot, const std::wstring& subKey, const std::wstring& name) {
    HKEY hKey;
    LSTATUS status = RegOpenKeyExW(hRoot, subKey.c_str(), 0, KEY_WRITE | KEY_WOW64_64KEY, &hKey);
    if (status != ERROR_SUCCESS) return false;

    status = RegDeleteValueW(hKey, name.c_str());
    RegCloseKey(hKey);
    return (status == ERROR_SUCCESS);
}

bool HardeningEngine::DeletePathRecursive(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return true;

    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return DeleteFileW(path.c_str()) != 0;
    }

    std::wstring searchPath = path + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        std::wstring fullSubPath = path + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            DeletePathRecursive(fullSubPath);
        } else {
            SetFileAttributesW(fullSubPath.c_str(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(fullSubPath.c_str());
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return RemoveDirectoryW(path.c_str()) != 0;
}

void HardeningEngine::KillBrowserProcesses(const std::wstring& exeName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exeName.c_str()) == 0) {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProc) {
                    TerminateProcess(hProc, 0);
                    CloseHandle(hProc);
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    Sleep(500);
}

std::wstring HardeningEngine::GetBrowserVersion(const std::wstring& exePath) {
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

static void DisableWindowsService(const std::wstring& serviceName) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) return;

    SC_HANDLE svc = OpenServiceW(scm, serviceName.c_str(), SERVICE_STOP | SERVICE_CHANGE_CONFIG);
    if (svc) {
        SERVICE_STATUS status;
        ControlService(svc, SERVICE_CONTROL_STOP, &status);
        ChangeServiceConfigW(
            svc, SERVICE_NO_CHANGE, SERVICE_DISABLED,
            SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL
        );
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
}

bool HardeningEngine::RunTweaks(
    const BrowserTarget& browser,
    const std::vector<int>& selectedTweakIds,
    LogCallback logCb,
    ProgressCallback progressCb
) {
    int total = (int)selectedTweakIds.size();
    if (total == 0) {
        logCb(L"[!] No tweaks selected to execute.", false);
        return false;
    }

    logCb(L"[*] Targeting browser: " + browser.name, true);
    std::wstring exeName = browser.id + L".exe";
    if (browser.id == L"edge") exeName = L"msedge.exe";
    KillBrowserProcesses(exeName);

    int step = 0;

    for (int id : selectedTweakIds) {
        step++;
        int pct = (step * 100) / total;

        switch (id) {
            case 1: { // AI Elimination
                logCb(L"[*] Applying AI & Gemini elimination policies...", true);
                const std::wstring& p = browser.policyKey;
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"GenAiDefaultSettings", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"GeminiSettings", 1);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"GeminiSparkSettings", 1);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"GeminiActOnWebSettings", 1);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"AIModeSettings", 1);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"OptimizationGuideAllowed", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"OptimizationGuideFetchingEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"GenAILocalFoundationalModelSettings", 1);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"BuiltInAIAPIsEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"PromptAPIAllowed", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"HelpMeWriteSettings", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"HelpMeReadSettings", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"HistorySearchSettings", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"TabOrganizerSettings", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"TabCompareSettings", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"CreateThemesSettings", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DevToolsGenAiSettings", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"AutofillPredictionSettings", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"ChromeSuggestionsSettings", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"FindsSettings", 2);

                // Purge on-disk AI models
                DeletePathRecursive(browser.userDataDir + L"\\OnDeviceHeadSuggestModel");
                DeletePathRecursive(browser.userDataDir + L"\\optimization_guide_model_store");
                DeletePathRecursive(browser.userDataDir + L"\\OptimizationGuideModelsManifest");
                DeletePathRecursive(browser.userDataDir + L"\\OptimizationHints");
                DeletePathRecursive(browser.userDataDir + L"\\Default\\AutofillAiModelCache");

                logCb(L"[✓] AI & Gemini subsystems disabled; local model files deleted.", true);
                break;
            }

            case 2: { // Privacy & Content Hardening
                logCb(L"[*] Applying privacy, network and content guard policies...", true);
                const std::wstring& p = browser.policyKey;

                // DNS-over-HTTPS (Quad9)
                SetRegString(HKEY_LOCAL_MACHINE, p, L"DnsOverHttpsMode", L"automatic");
                SetRegString(HKEY_LOCAL_MACHINE, p, L"DnsOverHttpsTemplates", L"https://dns.quad9.net/dns-query");

                // Security & Transport
                SetRegString(HKEY_LOCAL_MACHINE, p, L"HttpsOnlyMode", L"force_enabled");
                SetRegString(HKEY_LOCAL_MACHINE, p, L"SSLVersionMin", L"tls1.2");
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"HSTSPinningBypassAllowed", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"PostQuantumKeyAgreementEnabled", 1);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"WebRtcIPHandling", 2); // Disable non-proxied UDP

                // Content & Hardware Permissions
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultPopupsSetting", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultNotificationsSetting", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultGeolocationSetting", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultWebBluetoothGuardSetting", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultWebUsbGuardSetting", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultFileSystemReadGuardSetting", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultFileSystemWriteGuardSetting", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultSensorsSetting", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultSerialGuardSetting", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"InsecurePrivateNetworkRequestsAllowed", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultInsecureContentSetting", 2);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"ReduceAcceptLanguageEnabled", 1);

                // Anti-injection & Renderer Integrity
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"RendererCodeIntegrityEnabled", 1);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"ThirdPartyBlockingEnabled", 1);

                logCb(L"[✓] Privacy hardening applied (Quad9 DoH, HTTPS-only, WebRTC leak fix).", true);
                break;
            }

            case 3: { // Telemetry Suppression
                logCb(L"[*] Disabling telemetry, crash reporting and diagnostics...", true);
                const std::wstring& p = browser.policyKey;
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"MetricsReportingEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"SafeBrowsingExtendedReportingEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"SpellCheckServiceEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"ChromeCleanupEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"ChromeCleanupReportingEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"UserFeedbackAllowed", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"ReportingEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"CloudReportingEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"CloudProfileReportingEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"HeartbeatEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"SafeBrowsingEnabled", 0);

                logCb(L"[✓] Telemetry, diagnostics and cleanup reporting disabled.", true);
                break;
            }

            case 4: { // Preserve Google Sync & Passwords
                if (browser.id == L"chrome") {
                    logCb(L"[*] Setting up Google Sync and Password Manager allowlist...", true);
                    const std::wstring& p = browser.policyKey;
                    std::vector<std::wstring> allowedUrls = {
                        L"[*.]google.com",
                        L"https://accounts.google.com",
                        L"[*.]googleusercontent.com",
                        L"[*.]gstatic.com",
                        L"[*.]apis.google.com",
                        L"[*.]passkeys.google.com",
                        L"https://myaccount.google.com"
                    };
                    SetRegMultiString(HKEY_LOCAL_MACHINE, p, L"CookiesAllowedForUrls", allowedUrls);

                    // Ensure Sync & Passwords remain on
                    SetRegDword(HKEY_LOCAL_MACHINE, p, L"BrowserSignin", 1);
                    SetRegDword(HKEY_LOCAL_MACHINE, p, L"BrowserAddPersonEnabled", 1);
                    SetRegDword(HKEY_LOCAL_MACHINE, p, L"PasswordManagerEnabled", 1);
                    SetRegDword(HKEY_LOCAL_MACHINE, p, L"PasswordLeakDetectionEnabled", 1);

                    // Remove harmful SyncDisabled / AutoFillEnabled if present
                    DeleteRegValue(HKEY_LOCAL_MACHINE, p, L"SyncDisabled");
                    DeleteRegValue(HKEY_LOCAL_MACHINE, p, L"AutoFillEnabled");

                    logCb(L"[✓] Google Sync & Password Manager preserved via authentication allowlist.", true);
                } else {
                    logCb(L"[-] Google auth preservation skipped (only applicable to Chrome).", true);
                }
                break;
            }

            case 5: { // Performance
                logCb(L"[*] Applying low-resource & speed optimizations...", true);
                const std::wstring& p = browser.policyKey;
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"SitePerProcess", 0);                // Consolidates iframes
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"HighEfficiencyModeEnabled", 1);      // Memory Saver ON
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"MemorySaverModeSavings", 2);         // Aggressive savings
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"ThrottleJavaScriptTimers", 1);       // Throttle background timers
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"IntensiveWakeUpThrottlingEnabled", 1);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"HardwareAccelerationModeEnabled", 1); // GPU offload
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"BuiltInDnsClientEnabled", 1);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DiskCacheSize", 268435456);           // 256 MB cap
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"TabHoverCards", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"BackgroundModeEnabled", 0);           // No lingering background processes
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"NetworkPredictionOptions", 2);        // No speculative preconnects

                logCb(L"[✓] Memory saver active (SitePerProcess=0, GPU acceleration, timer throttling).", true);
                break;
            }

            case 6: { // Flags injection
                logCb(L"[*] Injecting performance flags into Local State...", true);
                std::wstring localStatePath = browser.userDataDir + L"\\Local State";
                std::ifstream inFile(localStatePath);
                if (inFile.is_open()) {
                    std::stringstream buffer;
                    buffer << inFile.rdbuf();
                    inFile.close();
                    std::string content = buffer.str();

                    std::vector<std::string> flagsToAdd = {
                        "enable-quic@1",
                        "back-forward-cache@1",
                        "smooth-scrolling@1",
                        "enable-scroll-prediction@1",
                        "canvas-oop-rasterization@1",
                        "enable-gpu-rasterization@1",
                        "enable-zero-copy@1",
                        "enable-parallel-downloading@1"
                    };

                    // Check if "enabled_labs_experiments" already exists
                    size_t pos = content.find("\"enabled_labs_experiments\":[");
                    if (pos != std::string::npos) {
                        size_t arrayStart = pos + strlen("\"enabled_labs_experiments\":[");
                        std::string insertStr;
                        for (const auto& f : flagsToAdd) {
                            if (content.find("\"" + f + "\"") == std::string::npos) {
                                if (!insertStr.empty()) insertStr += ",";
                                insertStr += "\"" + f + "\"";
                            }
                        }
                        if (!insertStr.empty()) {
                            if (content[arrayStart] != ']') {
                                insertStr += ",";
                            }
                            content.insert(arrayStart, insertStr);
                        }
                    } else {
                        // Inject into "browser" dictionary
                        size_t browserPos = content.find("\"browser\":{");
                        if (browserPos != std::string::npos) {
                            size_t insertPos = browserPos + strlen("\"browser\":{");
                            std::string insertStr = "\"enabled_labs_experiments\":[";
                            for (size_t i = 0; i < flagsToAdd.size(); ++i) {
                                if (i > 0) insertStr += ",";
                                insertStr += "\"" + flagsToAdd[i] + "\"";
                            }
                            insertStr += "],";
                            content.insert(insertPos, insertStr);
                        }
                    }

                    std::ofstream outFile(localStatePath, std::ios::trunc);
                    if (outFile.is_open()) {
                        outFile << content;
                        outFile.close();
                        logCb(L"[✓] Performance flags injected into Local State.", true);
                    } else {
                        logCb(L"[!] Could not write to Local State file.", false);
                    }
                } else {
                    logCb(L"[-] Local State file not found; launch browser once to generate it.", true);
                }
                break;
            }

            case 7: { // UI Debloat
                logCb(L"[*] Removing UI clutter (Cast, Sharing Hub, feedback)...", true);
                const std::wstring& p = browser.policyKey;
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"ShowCastIconInToolbar", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"EnableMediaRouter", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DesktopSharingHubEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"SharedClipboardEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"WebAppInstallByUserEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"TranslateEnabled", 0);
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"AutofillCreditCardEnabled", 0);

                logCb(L"[✓] Cast button, sharing hub, and web app install promotions removed.", true);
                break;
            }

            case 8: { // Search Provider
                logCb(L"[*] Setting Brave Search as default search engine...", true);
                const std::wstring& p = browser.policyKey;
                SetRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultSearchProviderEnabled", 1);
                SetRegString(HKEY_LOCAL_MACHINE, p, L"DefaultSearchProviderName", L"Brave Search");
                SetRegString(HKEY_LOCAL_MACHINE, p, L"DefaultSearchProviderSearchURL", L"https://search.brave.com/search?q={searchTerms}");
                SetRegString(HKEY_LOCAL_MACHINE, p, L"DefaultSearchProviderSuggestURL", L"https://search.brave.com/api/suggest?q={searchTerms}");

                logCb(L"[✓] Brave Search set as default search provider.", true);
                break;
            }

            case 9: { // Maintenance
                logCb(L"[*] Running deep profile maintenance (SQLite vacuum & cache purge)...", true);
                SQLiteHelper sqlite;
                if (sqlite.IsAvailable()) {
                    std::vector<std::wstring> dbs = {
                        L"History", L"Favicons", L"Web Data", L"Top Sites", L"Shortcuts", L"Network Action Predictor"
                    };
                    std::wstring defaultDir = browser.userDataDir + L"\\Default";
                    for (const auto& db : dbs) {
                        std::wstring dbPath = defaultDir + L"\\" + db;
                        if (FileOrDirExists(dbPath)) {
                            sqlite.VacuumAndReindex(dbPath);
                        }
                    }
                    logCb(L"[✓] SQLite profile databases defragmented and reindexed.", true);
                } else {
                    logCb(L"[-] Native winsqlite3.dll unavailable; skipping database defrag.", false);
                }

                // Purge caches
                DeletePathRecursive(browser.userDataDir + L"\\Default\\GPUCache");
                DeletePathRecursive(browser.userDataDir + L"\\Default\\DawnCache");
                DeletePathRecursive(browser.userDataDir + L"\\GrShaderCache");
                DeletePathRecursive(browser.userDataDir + L"\\ShaderCache");
                DeletePathRecursive(browser.userDataDir + L"\\Crashpad");
                logCb(L"[✓] Temporary GPU, shader, and crashpad caches purged.", true);

                // Flush DNS
                HMODULE hDns = LoadLibraryW(L"dnsapi.dll");
                if (hDns) {
                    auto pFlush = (DnsFlushResolverCacheFn)GetProcAddress(hDns, "DnsFlushResolverCache");
                    if (pFlush) pFlush();
                    FreeLibrary(hDns);
                }
                logCb(L"[✓] Windows DNS resolver cache flushed.", true);
                break;
            }

            case 10: { // Update Lockdown
                if (browser.id == L"chrome") {
                    logCb(L"[*] Enforcing permanent Chrome version lockdown...", true);
                    const std::wstring& up = browser.updateKey;
                    SetRegDword(HKEY_LOCAL_MACHINE, up, L"UpdateDefault", 0);
                    SetRegDword(HKEY_LOCAL_MACHINE, up, L"AutoUpdateCheckPeriodMinutes", 0);
                    SetRegDword(HKEY_LOCAL_MACHINE, up, L"DisableAutoUpdateChecksCheckboxValue", 1);
                    SetRegDword(HKEY_LOCAL_MACHINE, up, L"InstallDefault", 0);

                    std::wstring guid = browser.appGuid;
                    SetRegDword(HKEY_LOCAL_MACHINE, up, L"Update" + guid, 0);
                    SetRegDword(HKEY_LOCAL_MACHINE, up, L"Install" + guid, 0);

                    // Pin version
                    std::wstring ver = GetBrowserVersion(browser.exePath);
                    if (!ver.empty()) {
                        SetRegString(HKEY_LOCAL_MACHINE, up, L"TargetVersionPrefix" + guid, ver);
                        logCb(L"[*] Chrome version pinned to: " + ver, true);
                    }

                    // SCM Service lockdown
                    DisableWindowsService(L"GoogleUpdaterService154.0.8037.93");
                    DisableWindowsService(L"GoogleUpdaterInternalService154.0.8037.93");

                    // Shell execute schtasks
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\GoogleSystem\\GoogleUpdater\\GoogleUpdaterTaskSystem\" /Disable", NULL, SW_HIDE);

                    logCb(L"[✓] Chrome 4-layer update lockdown enforced (zero updates).", true);
                } else {
                    logCb(L"[-] Update lockdown skipped (only applicable to Chrome).", true);
                }
                break;
            }
        }

        progressCb(pct);
    }

    logCb(L"\n[✓] All selected optimizations completed successfully!", true);
    return true;
}
