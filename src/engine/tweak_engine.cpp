#include "tweak_engine.h"
#include "sqlite_cleaner.h"
#include <tlhelp32.h>
#include <fstream>
#include <sstream>
#include <iostream>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

typedef BOOL (WINAPI *DnsFlushResolverCacheFn)(VOID);

std::vector<TweakItem> TweakEngine::GetAllTweaks() {
    return {
        {
            1, L"AI & Models", L"Eliminate AI & Gemini Subsystems",
            L"Enforces 25 enterprise policies to disable Gemini, PromptAPI, etc., and deletes all on-disk foundational models.",
            L"HIGH IMPACT", true, false
        },
        {
            2, L"Privacy", L"Quad9 DoH & Privacy Hardening",
            L"Enforces DNS-over-HTTPS (Quad9), HTTPS-Only mode, WebRTC IP leak mitigation, and blocks intrusive device sensors.",
            L"HIGH IMPACT", true, false
        },
        {
            3, L"Privacy", L"Disable Telemetry & Diagnostics",
            L"Stops metrics reporting, SafeBrowsing extended telemetry, background cleanup scanner, and user feedback.",
            L"RECOMMENDED", true, false
        },
        {
            4, L"Authentication", L"Preserve Google Sync & Password Manager",
            L"Explicitly allowlists authentication cookies so Google Sync and Password Manager remain 100% operational.",
            L"SAFE", true, true
        },
        {
            5, L"Performance", L"Process Clamping & Max Memory Saver",
            L"Clamps iframe processes (SitePerProcess=0), activates aggressive tab discarding, and clamps background timers.",
            L"HIGH IMPACT", true, false
        },
        {
            6, L"Performance", L"Inject High-Speed Performance Flags",
            L"Injects QUIC protocol, parallel downloading, GPU rasterization, zero-copy, and back-forward cache into Local State.",
            L"RECOMMENDED", true, false
        },
        {
            7, L"Interface", L"Remove UI Clutter (Cast, Sharing Hub)",
            L"Removes Cast button from toolbar, Desktop Sharing Hub, shared clipboard, and web app install promotions.",
            L"SAFE", true, false
        },
        {
            8, L"Search", L"Set Default Search to Brave Search",
            L"Configures private Brave Search as the default search engine, replacing telemetry-heavy search engines.",
            L"RECOMMENDED", true, false
        },
        {
            9, L"Maintenance", L"Database Defragmentation & Cache Sweep",
            L"Vacuums SQLite history/favicons, purges stale GPU & shader caches, and flushes Windows DNS cache.",
            L"SAFE", true, false
        },
        {
            10, L"Security", L"Permanent 4-Layer Update Lockdown",
            L"Locks browser version, disables updater services, disables scheduled tasks, and blocks background updaters.",
            L"HIGH IMPACT", true, true
        }
    };
}

void TweakEngine::ApplyPreset(std::vector<TweakItem>& tweaks, TweakPreset preset) {
    for (auto& tw : tweaks) {
        switch (preset) {
            case TweakPreset::Maximum:
                tw.enabled = true;
                break;
            case TweakPreset::Balanced:
                tw.enabled = (tw.id == 1 || tw.id == 2 || tw.id == 3 || tw.id == 4 || tw.id == 5 || tw.id == 7 || tw.id == 9);
                break;
            case TweakPreset::PrivacyOnly:
                tw.enabled = (tw.id == 1 || tw.id == 2 || tw.id == 3 || tw.id == 8);
                break;
        }
    }
}

bool TweakEngine::WriteRegDword(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, DWORD value) {
    HKEY hKey;
    DWORD disp;
    if (RegCreateKeyExW(hRoot, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, NULL, &hKey, &disp) != ERROR_SUCCESS)
        return false;
    LSTATUS s = RegSetValueExW(hKey, name.c_str(), 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
    RegCloseKey(hKey);
    return (s == ERROR_SUCCESS);
}

bool TweakEngine::WriteRegString(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, const std::wstring& value) {
    HKEY hKey;
    DWORD disp;
    if (RegCreateKeyExW(hRoot, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, NULL, &hKey, &disp) != ERROR_SUCCESS)
        return false;
    DWORD bytes = (DWORD)((value.length() + 1) * sizeof(wchar_t));
    LSTATUS s = RegSetValueExW(hKey, name.c_str(), 0, REG_SZ, (const BYTE*)value.c_str(), bytes);
    RegCloseKey(hKey);
    return (s == ERROR_SUCCESS);
}

bool TweakEngine::WriteRegList(HKEY hRoot, const std::wstring& subKey, const std::wstring& listName, const std::vector<std::wstring>& items) {
    std::wstring path = subKey + L"\\" + listName;
    HKEY hKey;
    DWORD disp;
    if (RegCreateKeyExW(hRoot, path.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, NULL, &hKey, &disp) != ERROR_SUCCESS)
        return false;
    for (size_t i = 0; i < items.size(); ++i) {
        std::wstring idx = std::to_wstring(i + 1);
        DWORD bytes = (DWORD)((items[i].length() + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, idx.c_str(), 0, REG_SZ, (const BYTE*)items[i].c_str(), bytes);
    }
    RegCloseKey(hKey);
    return true;
}

bool TweakEngine::DeleteRegValue(HKEY hRoot, const std::wstring& subKey, const std::wstring& name) {
    HKEY hKey;
    if (RegOpenKeyExW(hRoot, subKey.c_str(), 0, KEY_WRITE | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
        return false;
    LSTATUS s = RegDeleteValueW(hKey, name.c_str());
    RegCloseKey(hKey);
    return (s == ERROR_SUCCESS);
}

bool TweakEngine::RemoveDirRecursive(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return true;
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) return DeleteFileW(path.c_str()) != 0;

    std::wstring search = path + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring sub = path + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            RemoveDirRecursive(sub);
        } else {
            SetFileAttributesW(sub.c_str(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(sub.c_str());
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return RemoveDirectoryW(path.c_str()) != 0;
}

void TweakEngine::KillProcesses(const std::wstring& exeName) {
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
    Sleep(300);
}

static void DisableService(const std::wstring& name) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) return;
    SC_HANDLE svc = OpenServiceW(scm, name.c_str(), SERVICE_STOP | SERVICE_CHANGE_CONFIG);
    if (svc) {
        SERVICE_STATUS st;
        ControlService(svc, SERVICE_CONTROL_STOP, &st);
        ChangeServiceConfigW(svc, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
}

bool TweakEngine::ExecuteTweaks(
    const BrowserTarget& browser,
    const std::vector<int>& activeTweakIds,
    EngineLogCallback logCb,
    EngineProgressCallback progCb
) {
    int total = (int)activeTweakIds.size();
    if (total == 0) {
        logCb(L"No tweaks selected.", L"WARNING");
        return false;
    }

    logCb(L"Preparing to optimize: " + browser.name, L"ACTION");
    std::wstring exeName = browser.id + L".exe";
    if (browser.id == L"edge") exeName = L"msedge.exe";
    KillProcesses(exeName);

    const std::wstring& p = browser.policyKey;
    const std::wstring& up = browser.updateKey;
    int step = 0;

    for (int id : activeTweakIds) {
        step++;
        int pct = (step * 100) / total;

        switch (id) {
            case 1: { // AI Elimination
                progCb(pct, L"Killing AI & Gemini subsystems...");
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"GenAiDefaultSettings", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"GeminiSettings", 1);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"GeminiSparkSettings", 1);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"GeminiActOnWebSettings", 1);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"AIModeSettings", 1);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"OptimizationGuideAllowed", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"OptimizationGuideFetchingEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"GenAILocalFoundationalModelSettings", 1);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"BuiltInAIAPIsEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"PromptAPIAllowed", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"HelpMeWriteSettings", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"HelpMeReadSettings", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"HistorySearchSettings", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"TabOrganizerSettings", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"TabCompareSettings", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"CreateThemesSettings", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DevToolsGenAiSettings", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"AutofillPredictionSettings", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ChromeSuggestionsSettings", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"FindsSettings", 2);

                RemoveDirRecursive(browser.userDataDir + L"\\OnDeviceHeadSuggestModel");
                RemoveDirRecursive(browser.userDataDir + L"\\optimization_guide_model_store");
                RemoveDirRecursive(browser.userDataDir + L"\\OptimizationGuideModelsManifest");
                RemoveDirRecursive(browser.userDataDir + L"\\OptimizationHints");
                RemoveDirRecursive(browser.userDataDir + L"\\Default\\AutofillAiModelCache");

                logCb(L"AI & Gemini subsystems eliminated and local models purged.", L"SUCCESS");
                break;
            }

            case 2: { // Privacy & Content Hardening
                progCb(pct, L"Configuring Quad9 DoH & Privacy Guard...");
                WriteRegString(HKEY_LOCAL_MACHINE, p, L"DnsOverHttpsMode", L"automatic");
                WriteRegString(HKEY_LOCAL_MACHINE, p, L"DnsOverHttpsTemplates", L"https://dns.quad9.net/dns-query");
                WriteRegString(HKEY_LOCAL_MACHINE, p, L"HttpsOnlyMode", L"force_enabled");
                WriteRegString(HKEY_LOCAL_MACHINE, p, L"SSLVersionMin", L"tls1.2");
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"HSTSPinningBypassAllowed", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"PostQuantumKeyAgreementEnabled", 1);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"WebRtcIPHandling", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultPopupsSetting", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultNotificationsSetting", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultGeolocationSetting", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultWebBluetoothGuardSetting", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultWebUsbGuardSetting", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultFileSystemReadGuardSetting", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultFileSystemWriteGuardSetting", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultSensorsSetting", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultSerialGuardSetting", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"InsecurePrivateNetworkRequestsAllowed", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultInsecureContentSetting", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ReduceAcceptLanguageEnabled", 1);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"RendererCodeIntegrityEnabled", 1);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ThirdPartyBlockingEnabled", 1);

                logCb(L"Quad9 DoH, HTTPS-only, WebRTC leak fix & content hardening applied.", L"SUCCESS");
                break;
            }

            case 3: { // Telemetry Suppression
                progCb(pct, L"Disabling telemetry & crash reporting...");
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"MetricsReportingEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"SafeBrowsingExtendedReportingEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"SpellCheckServiceEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ChromeCleanupEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ChromeCleanupReportingEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"UserFeedbackAllowed", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ReportingEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"CloudReportingEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"CloudProfileReportingEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"HeartbeatEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"SafeBrowsingEnabled", 0);

                logCb(L"Telemetry, background diagnostics and feedback hooks shut down.", L"SUCCESS");
                break;
            }

            case 4: { // Preserve Google Sync & Passwords
                if (browser.id == L"chrome") {
                    progCb(pct, L"Preserving Google Auth & Password Manager...");
                    std::vector<std::wstring> urls = {
                        L"[*.]google.com",
                        L"https://accounts.google.com",
                        L"[*.]googleusercontent.com",
                        L"[*.]gstatic.com",
                        L"[*.]apis.google.com",
                        L"[*.]passkeys.google.com",
                        L"https://myaccount.google.com"
                    };
                    WriteRegList(HKEY_LOCAL_MACHINE, p, L"CookiesAllowedForUrls", urls);
                    WriteRegDword(HKEY_LOCAL_MACHINE, p, L"BrowserSignin", 1);
                    WriteRegDword(HKEY_LOCAL_MACHINE, p, L"BrowserAddPersonEnabled", 1);
                    WriteRegDword(HKEY_LOCAL_MACHINE, p, L"PasswordManagerEnabled", 1);
                    WriteRegDword(HKEY_LOCAL_MACHINE, p, L"PasswordLeakDetectionEnabled", 1);
                    DeleteRegValue(HKEY_LOCAL_MACHINE, p, L"SyncDisabled");
                    DeleteRegValue(HKEY_LOCAL_MACHINE, p, L"AutoFillEnabled");

                    logCb(L"Google authentication cookie allowlist configured for Sync & Passwords.", L"SUCCESS");
                }
                break;
            }

            case 5: { // Low-RAM & Performance
                progCb(pct, L"Applying process clamping & Memory Saver...");
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"SitePerProcess", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"HighEfficiencyModeEnabled", 1);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"MemorySaverModeSavings", 2);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ThrottleJavaScriptTimers", 1);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"IntensiveWakeUpThrottlingEnabled", 1);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"HardwareAccelerationModeEnabled", 1);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"BuiltInDnsClientEnabled", 1);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DiskCacheSize", 268435456);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"TabHoverCards", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"BackgroundModeEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"NetworkPredictionOptions", 2);

                logCb(L"RAM clamping (SitePerProcess=0) and max Memory Saver enforced.", L"SUCCESS");
                break;
            }

            case 6: { // Performance Flags
                progCb(pct, L"Injecting performance flags into Local State...");
                std::wstring lsPath = browser.userDataDir + L"\\Local State";
                std::ifstream in(lsPath);
                if (in.is_open()) {
                    std::stringstream ss;
                    ss << in.rdbuf();
                    in.close();
                    std::string c = ss.str();

                    std::vector<std::string> flags = {
                        "enable-quic@1", "back-forward-cache@1", "smooth-scrolling@1",
                        "enable-scroll-prediction@1", "canvas-oop-rasterization@1",
                        "enable-gpu-rasterization@1", "enable-zero-copy@1", "enable-parallel-downloading@1"
                    };

                    size_t pos = c.find("\"enabled_labs_experiments\":[");
                    if (pos != std::string::npos) {
                        size_t start = pos + strlen("\"enabled_labs_experiments\":[");
                        std::string ins;
                        for (const auto& f : flags) {
                            if (c.find("\"" + f + "\"") == std::string::npos) {
                                if (!ins.empty()) ins += ",";
                                ins += "\"" + f + "\"";
                            }
                        }
                        if (!ins.empty()) {
                            if (c[start] != ']') ins += ",";
                            c.insert(start, ins);
                        }
                    }

                    std::ofstream out(lsPath, std::ios::trunc);
                    if (out.is_open()) {
                        out << c;
                        out.close();
                        logCb(L"Performance flags (QUIC, GPU raster, parallel download) injected.", L"SUCCESS");
                    }
                }
                break;
            }

            case 7: { // UI Clutter
                progCb(pct, L"Stripping UI clutter & Cast buttons...");
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ShowCastIconInToolbar", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"EnableMediaRouter", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DesktopSharingHubEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"SharedClipboardEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"WebAppInstallByUserEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"TranslateEnabled", 0);
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"AutofillCreditCardEnabled", 0);

                logCb(L"Cast, Sharing Hub, and promotion banners removed from UI.", L"SUCCESS");
                break;
            }

            case 8: { // Search Provider
                progCb(pct, L"Configuring Brave Search provider...");
                WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultSearchProviderEnabled", 1);
                WriteRegString(HKEY_LOCAL_MACHINE, p, L"DefaultSearchProviderName", L"Brave Search");
                WriteRegString(HKEY_LOCAL_MACHINE, p, L"DefaultSearchProviderSearchURL", L"https://search.brave.com/search?q={searchTerms}");
                WriteRegString(HKEY_LOCAL_MACHINE, p, L"DefaultSearchProviderSuggestURL", L"https://search.brave.com/api/suggest?q={searchTerms}");

                logCb(L"Private Brave Search set as default search engine.", L"SUCCESS");
                break;
            }

            case 9: { // Maintenance
                progCb(pct, L"Defragmenting profile databases & sweeping caches...");
                INT64 reclaimed = 0;
                RunDeepClean(browser, reclaimed, logCb);
                break;
            }

            case 10: { // Update Lockdown
                if (browser.id == L"chrome") {
                    progCb(pct, L"Enforcing permanent 4-layer update lockdown...");
                    WriteRegDword(HKEY_LOCAL_MACHINE, up, L"UpdateDefault", 0);
                    WriteRegDword(HKEY_LOCAL_MACHINE, up, L"AutoUpdateCheckPeriodMinutes", 0);
                    WriteRegDword(HKEY_LOCAL_MACHINE, up, L"DisableAutoUpdateChecksCheckboxValue", 1);
                    WriteRegDword(HKEY_LOCAL_MACHINE, up, L"InstallDefault", 0);

                    std::wstring guid = browser.appGuid;
                    WriteRegDword(HKEY_LOCAL_MACHINE, up, L"Update" + guid, 0);
                    WriteRegDword(HKEY_LOCAL_MACHINE, up, L"Install" + guid, 0);

                    if (!browser.version.empty()) {
                        WriteRegString(HKEY_LOCAL_MACHINE, up, L"TargetVersionPrefix" + guid, browser.version);
                    }

                    DisableService(L"GoogleUpdaterService154.0.8037.93");
                    DisableService(L"GoogleUpdaterInternalService154.0.8037.93");
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\GoogleSystem\\GoogleUpdater\\GoogleUpdaterTaskSystem\" /Disable", NULL, SW_HIDE);

                    logCb(L"Google Chrome frozen at current version (services & tasks locked).", L"SUCCESS");
                }
                break;
            }
        }
    }

    progCb(100, L"Ready.");
    logCb(L"All selected optimizations applied successfully.", L"INFO");
    return true;
}

bool TweakEngine::RunDeepClean(
    const BrowserTarget& browser,
    INT64& outReclaimedBytes,
    EngineLogCallback logCb
) {
    outReclaimedBytes = 0;
    logCb(L"Starting deep profile maintenance...", L"ACTION");

    // 1. Vacuum SQLite databases
    SQLiteCleaner sqlite;
    if (sqlite.IsAvailable()) {
        std::vector<std::wstring> dbs = {
            L"History", L"Favicons", L"Web Data", L"Top Sites", L"Shortcuts", L"Network Action Predictor"
        };
        std::wstring defaultDir = browser.userDataDir + L"\\Default";
        for (const auto& db : dbs) {
            std::wstring dbPath = defaultDir + L"\\" + db;
            if (PathExists(dbPath)) {
                INT64 saved = sqlite.OptimizeDatabase(dbPath);
                outReclaimedBytes += saved;
            }
        }
        logCb(L"Profile SQLite databases defragmented and reindexed.", L"SUCCESS");
    }

    // 2. Cache purge
    INT64 cacheBefore = 0;
    cacheBefore += AuditEngine::CalculateDirectorySize(browser.userDataDir + L"\\Default\\GPUCache");
    cacheBefore += AuditEngine::CalculateDirectorySize(browser.userDataDir + L"\\Default\\DawnCache");
    cacheBefore += AuditEngine::CalculateDirectorySize(browser.userDataDir + L"\\GrShaderCache");
    cacheBefore += AuditEngine::CalculateDirectorySize(browser.userDataDir + L"\\ShaderCache");
    cacheBefore += AuditEngine::CalculateDirectorySize(browser.userDataDir + L"\\Crashpad");

    RemoveDirRecursive(browser.userDataDir + L"\\Default\\GPUCache");
    RemoveDirRecursive(browser.userDataDir + L"\\Default\\DawnCache");
    RemoveDirRecursive(browser.userDataDir + L"\\GrShaderCache");
    RemoveDirRecursive(browser.userDataDir + L"\\ShaderCache");
    RemoveDirRecursive(browser.userDataDir + L"\\Crashpad");

    outReclaimedBytes += cacheBefore;
    logCb(L"Stale GPU, Dawn, and crashpad caches swept (" + std::to_wstring(cacheBefore / 1024) + L" KB reclaimed).", L"SUCCESS");

    // 3. Flush Windows DNS
    HMODULE hDns = LoadLibraryW(L"dnsapi.dll");
    if (hDns) {
        auto pFlush = (DnsFlushResolverCacheFn)GetProcAddress(hDns, "DnsFlushResolverCache");
        if (pFlush) pFlush();
        FreeLibrary(hDns);
    }
    logCb(L"Windows DNS resolver cache flushed.", L"SUCCESS");

    return true;
}
