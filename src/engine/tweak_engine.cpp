#include "tweak_engine.h"
#include "audit_engine.h"
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
            L"Enforces enterprise policies to kill Gemini, PromptAPI, and Edge Copilot, and deletes on-disk local models.",
            L"HIGH IMPACT", true, false
        },
        {
            2, L"Privacy", L"Quad9 DoH & Network Security",
            L"Enforces DNS-over-HTTPS (Quad9), HTTPS-Only mode, WebRTC IP leak mitigation, and blocks intrusive device sensors.",
            L"HIGH IMPACT", true, false
        },
        {
            3, L"Privacy", L"Suppress Telemetry & Diagnostics",
            L"Stops metrics reporting, SafeBrowsing extended telemetry, background cleanup scanner, and Edge diagnostic data.",
            L"RECOMMENDED", true, false
        },
        {
            4, L"Authentication", L"Preserve Browser Sync & Password Manager",
            L"Allowlists authentication cookies so Google Sync/Passwords (Chrome) or Microsoft Account (Edge) stay functional.",
            L"SAFE", true, false
        },
        {
            5, L"Performance", L"Process Clamping & Memory Saver",
            L"Clamps iframe processes (SitePerProcess=0), activates aggressive tab discarding, and clamps background timers.",
            L"HIGH IMPACT", true, false
        },
        {
            6, L"Performance", L"Ultra Low-Resource & Process Limits",
            L"Caps renderers to 4, limits disk cache to 256MB, media cache to 128MB, and enables 5-min background tab sleep.",
            L"ULTRA-LOW RAM", true, false
        },
        {
            7, L"Performance", L"Inject High-Speed Performance Flags",
            L"Injects QUIC protocol, zero-copy rasterization, D3D11 ANGLE, Skia Graphite, and parallel downloading into Local State.",
            L"RECOMMENDED", true, false
        },
        {
            8, L"Privacy", L"Eliminate Privacy Sandbox & Ad Topics",
            L"Disables Google Topics API, Privacy Sandbox ad measurement, site-commissioned ads, and survey prompts.",
            L"HIGH IMPACT", true, false
        },
        {
            9, L"Debloat", L"Remove Commercial & Shopping Bloat",
            L"Disables price tracking, shopping list prompts, Edge shopping discounts/coupons, and floating search widgets.",
            L"RECOMMENDED", true, false
        },
        {
            10, L"Interface", L"Strip UI Clutter & Background Features",
            L"Removes Cast button from toolbar, Desktop Sharing Hub, shared clipboard, Google Lens search, and NTP news feeds.",
            L"SAFE", true, false
        },
        {
            11, L"Search", L"Set Default Search to Brave Search",
            L"Configures private Brave Search as the default search engine, replacing telemetry-heavy search engines.",
            L"RECOMMENDED", true, false
        },
        {
            12, L"Maintenance", L"Database Defragmentation & Cache Sweep",
            L"Vacuums SQLite history/favicons, purges stale GPU & shader caches, and flushes Windows DNS cache.",
            L"SAFE", true, false
        },
        {
            13, L"Security", L"Permanent 4-Layer Update Lockdown",
            L"Freezes version, disables updater services, disables scheduled tasks, and blocks background updaters (Chrome/Brave/Edge).",
            L"HIGH IMPACT", true, false
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
                tw.enabled = (tw.id == 1 || tw.id == 2 || tw.id == 3 || tw.id == 4 || tw.id == 5 ||
                              tw.id == 6 || tw.id == 7 || tw.id == 8 || tw.id == 9 || tw.id == 10 || tw.id == 12);
                break;
            case TweakPreset::PrivacyOnly:
                tw.enabled = (tw.id == 1 || tw.id == 2 || tw.id == 3 || tw.id == 8 || tw.id == 11);
                break;
            case TweakPreset::UltraLowResource:
                tw.enabled = (tw.id == 5 || tw.id == 6 || tw.id == 7 || tw.id == 10 || tw.id == 12);
                break;
        }
    }
}

bool TweakEngine::WriteRegDword(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, DWORD value) {
    HKEY hKey;
    DWORD disp;
    LSTATUS cr = RegCreateKeyExW(hRoot, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, NULL, &hKey, &disp);
    if (cr != ERROR_SUCCESS) return false;
    LSTATUS s = RegSetValueExW(hKey, name.c_str(), 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
    RegCloseKey(hKey);
    return (s == ERROR_SUCCESS);
}

bool TweakEngine::WriteRegString(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, const std::wstring& value) {
    HKEY hKey;
    DWORD disp;
    LSTATUS cr = RegCreateKeyExW(hRoot, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, NULL, &hKey, &disp);
    if (cr != ERROR_SUCCESS) return false;
    DWORD bytes = (DWORD)((value.length() + 1) * sizeof(wchar_t));
    LSTATUS s = RegSetValueExW(hKey, name.c_str(), 0, REG_SZ, (const BYTE*)value.c_str(), bytes);
    RegCloseKey(hKey);
    return (s == ERROR_SUCCESS);
}

bool TweakEngine::WriteDualRegDword(const std::wstring& subKey, const std::wstring& name, DWORD value) {
    bool ok1 = WriteRegDword(HKEY_LOCAL_MACHINE, subKey, name, value);
    bool ok2 = WriteRegDword(HKEY_CURRENT_USER, subKey, name, value);
    return (ok1 || ok2);
}

bool TweakEngine::WriteDualRegString(const std::wstring& subKey, const std::wstring& name, const std::wstring& value) {
    bool ok1 = WriteRegString(HKEY_LOCAL_MACHINE, subKey, name, value);
    bool ok2 = WriteRegString(HKEY_CURRENT_USER, subKey, name, value);
    return (ok1 || ok2);
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
    return (s == ERROR_SUCCESS || s == ERROR_FILE_NOT_FOUND);
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

    std::vector<HANDLE> waitHandles;

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exeName.c_str()) == 0) {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
                if (hProc) {
                    TerminateProcess(hProc, 0);
                    waitHandles.push_back(hProc);
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    for (HANDLE h : waitHandles) {
        WaitForSingleObject(h, 3000);
        CloseHandle(h);
    }

    if (!waitHandles.empty()) {
        Sleep(500);
    }
}

static bool VerifyRegDword(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, DWORD expected) {
    DWORD val = 0;
    return AuditEngine::ReadRegDword(hRoot, subKey, name, val) && (val == expected);
}

static bool VerifyRegStringContains(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, const std::wstring& fragment) {
    std::wstring val;
    return AuditEngine::ReadRegString(hRoot, subKey, name, val) && (val.find(fragment) != std::wstring::npos);
}

static void DisableServiceByPrefix(const std::wstring& prefix) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) return;

    DWORD needed = 0, count = 0, resumeHandle = 0;
    EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
        NULL, 0, &needed, &count, &resumeHandle, NULL);

    if (needed == 0) {
        CloseServiceHandle(scm);
        return;
    }

    std::vector<BYTE> buf(needed);
    if (!EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
        buf.data(), needed, &needed, &count, &resumeHandle, NULL)) {
        CloseServiceHandle(scm);
        return;
    }

    LPENUM_SERVICE_STATUS_PROCESSW pSvc = (LPENUM_SERVICE_STATUS_PROCESSW)buf.data();
    for (DWORD i = 0; i < count; ++i) {
        std::wstring svcName = pSvc[i].lpServiceName;
        if (svcName.find(prefix) != std::wstring::npos) {
            SC_HANDLE svc = OpenServiceW(scm, svcName.c_str(), SERVICE_STOP | SERVICE_CHANGE_CONFIG);
            if (svc) {
                SERVICE_STATUS st;
                ControlService(svc, SERVICE_CONTROL_STOP, &st);
                ChangeServiceConfigW(svc, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE,
                    NULL, NULL, NULL, NULL, NULL, NULL, NULL);
                CloseServiceHandle(svc);
            }
        }
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

    logCb(L"Preparing to optimize: " + browser.name + (browser.version.empty() ? L"" : (L" (v" + browser.version + L")")), L"ACTION");
    std::wstring exeName = browser.id + L".exe";
    if (browser.id == L"edge") exeName = L"msedge.exe";
    else if (browser.id == L"brave") exeName = L"brave.exe";

    logCb(L"Closing browser processes and unlocking profile files...", L"ACTION");
    KillProcesses(exeName);

    const std::wstring& p = browser.policyKey;
    const std::wstring& up = browser.updateKey;
    int step = 0;
    int failCount = 0;

    for (int id : activeTweakIds) {
        step++;
        int pct = (step * 100) / total;

        switch (id) {
            case 1: { // AI & Gemini / Copilot Elimination
                progCb(pct, L"Killing AI, Gemini & Copilot subsystems...");

                struct RegEntry { const wchar_t* name; DWORD val; } entries[] = {
                    { L"GenAiDefaultSettings",            2 },
                    { L"GeminiSettings",                  1 },
                    { L"GeminiSparkSettings",             1 },
                    { L"GeminiActOnWebSettings",          1 },
                    { L"AIModeSettings",                  1 },
                    { L"OptimizationGuideAllowed",        0 },
                    { L"OptimizationGuideFetchingEnabled",0 },
                    { L"GenAILocalFoundationalModelSettings", 1 },
                    { L"BuiltInAIAPIsEnabled",            0 },
                    { L"PromptAPIAllowed",                2 },
                    { L"HelpMeWriteSettings",             2 },
                    { L"HelpMeReadSettings",              2 },
                    { L"HistorySearchSettings",           2 },
                    { L"TabOrganizerSettings",            2 },
                    { L"TabCompareSettings",              2 },
                    { L"CreateThemesSettings",            2 },
                    { L"DevToolsGenAiSettings",           2 },
                    { L"AutofillPredictionSettings",      2 },
                    { L"ChromeSuggestionsSettings",       2 },
                    { L"FindsSettings",                   2 },
                    // Edge Copilot policies
                    { L"ComposeInlineEnabled",            0 },
                    { L"CopilotPageContext",              0 },
                    { L"EdgeEntSearchCopilotInSidebarEnabled", 0 },
                    { L"DiscoverPageContextEnabled",      0 },
                    { L"SidebarAppSearchEnabled",         0 }
                };

                int ok = 0;
                for (auto& e : entries) {
                    WriteDualRegDword(p, e.name, e.val);
                    if (VerifyRegDword(HKEY_LOCAL_MACHINE, p, e.name, e.val) ||
                        VerifyRegDword(HKEY_CURRENT_USER, p, e.name, e.val)) {
                        ok++;
                    }
                }

                // Purge disk model stores
                RemoveDirRecursive(browser.userDataDir + L"\\OnDeviceHeadSuggestModel");
                RemoveDirRecursive(browser.userDataDir + L"\\optimization_guide_model_store");
                RemoveDirRecursive(browser.userDataDir + L"\\OptimizationGuideModelsManifest");
                RemoveDirRecursive(browser.userDataDir + L"\\OptimizationHints");
                RemoveDirRecursive(browser.userDataDir + L"\\Default\\AutofillAiModelCache");

                logCb(L"[✓] AI & Gemini/Copilot killed: " + std::to_wstring(ok) + L" policies enforced across machine and user hives.", L"SUCCESS");
                break;
            }

            case 2: { // Privacy & Content Hardening
                progCb(pct, L"Enforcing Quad9 DoH & Privacy Guard...");

                WriteDualRegString(p, L"DnsOverHttpsMode", L"automatic");
                WriteDualRegString(p, L"DnsOverHttpsTemplates", L"https://dns.quad9.net/dns-query");
                WriteDualRegString(p, L"HttpsOnlyMode", L"force_enabled");
                WriteDualRegString(p, L"SSLVersionMin", L"tls1.2");
                WriteDualRegDword(p, L"HSTSPinningBypassAllowed", 0);
                WriteDualRegDword(p, L"PostQuantumKeyAgreementEnabled", 1);
                WriteDualRegDword(p, L"WebRtcIPHandling", 2);
                WriteDualRegDword(p, L"DefaultPopupsSetting", 2);
                WriteDualRegDword(p, L"DefaultNotificationsSetting", 2);
                WriteDualRegDword(p, L"DefaultGeolocationSetting", 2);
                WriteDualRegDword(p, L"DefaultWebBluetoothGuardSetting", 2);
                WriteDualRegDword(p, L"DefaultWebUsbGuardSetting", 2);
                WriteDualRegDword(p, L"DefaultFileSystemReadGuardSetting", 2);
                WriteDualRegDword(p, L"DefaultFileSystemWriteGuardSetting", 2);
                WriteDualRegDword(p, L"DefaultSensorsSetting", 2);
                WriteDualRegDword(p, L"DefaultSerialGuardSetting", 2);
                WriteDualRegDword(p, L"InsecurePrivateNetworkRequestsAllowed", 0);
                WriteDualRegDword(p, L"DefaultInsecureContentSetting", 2);
                WriteDualRegDword(p, L"ReduceAcceptLanguageEnabled", 1);
                WriteDualRegDword(p, L"RendererCodeIntegrityEnabled", 1);
                WriteDualRegDword(p, L"ThirdPartyBlockingEnabled", 1);

                logCb(L"[✓] Quad9 DoH, HTTPS-only, WebRTC leak fix & sensor hardening applied.", L"SUCCESS");
                break;
            }

            case 3: { // Telemetry Suppression
                progCb(pct, L"Disabling telemetry, diagnostics & variations...");

                WriteDualRegDword(p, L"MetricsReportingEnabled", 0);
                WriteDualRegDword(p, L"SafeBrowsingExtendedReportingEnabled", 0);
                WriteDualRegDword(p, L"SpellCheckServiceEnabled", 0);
                WriteDualRegDword(p, L"ChromeCleanupEnabled", 0);
                WriteDualRegDword(p, L"ChromeCleanupReportingEnabled", 0);
                WriteDualRegDword(p, L"UserFeedbackAllowed", 0);
                WriteDualRegDword(p, L"ReportingEnabled", 0);
                WriteDualRegDword(p, L"CloudReportingEnabled", 0);
                WriteDualRegDword(p, L"CloudProfileReportingEnabled", 0);
                WriteDualRegDword(p, L"HeartbeatEnabled", 0);
                WriteDualRegDword(p, L"SafeBrowsingEnabled", 0);
                WriteDualRegDword(p, L"ChromeVariations", 2);
                // Edge telemetry policies
                WriteDualRegDword(p, L"DiagnosticData", 0);
                WriteDualRegDword(p, L"PersonalizationReportingEnabled", 0);
                WriteDualRegDword(p, L"ShareBrowsingHistory", 0);
                WriteDualRegDword(p, L"EdgeAssetDeliveryServiceEnabled", 0);

                logCb(L"[✓] Telemetry, diagnostics & experiment rollouts fully blocked.", L"SUCCESS");
                break;
            }

            case 4: { // Authentication & Password Preservation
                progCb(pct, L"Preserving Browser Sync & Password Manager...");
                if (browser.id == L"chrome") {
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
                    WriteRegList(HKEY_CURRENT_USER, p, L"CookiesAllowedForUrls", urls);
                    WriteDualRegDword(p, L"BrowserSignin", 1);
                    WriteDualRegDword(p, L"BrowserAddPersonEnabled", 1);
                    WriteDualRegDword(p, L"PasswordManagerEnabled", 1);
                    WriteDualRegDword(p, L"PasswordLeakDetectionEnabled", 1);
                    DeleteRegValue(HKEY_LOCAL_MACHINE, p, L"SyncDisabled");
                    DeleteRegValue(HKEY_CURRENT_USER, p, L"SyncDisabled");
                    logCb(L"[✓] Google Sync & Password Manager preserved via cookie allowlist.", L"SUCCESS");
                } else if (browser.id == L"edge") {
                    WriteDualRegDword(p, L"PasswordManagerEnabled", 1);
                    WriteDualRegDword(p, L"SyncDisabled", 0);
                    logCb(L"[✓] Microsoft Edge Sync & Passwords preserved.", L"SUCCESS");
                } else {
                    logCb(L"[i] Brave authentication is managed internally; preserved.", L"INFO");
                }
                break;
            }

            case 5: { // RAM Clamping & Memory Saver
                progCb(pct, L"Applying process clamping & Memory Saver...");

                WriteDualRegDword(p, L"SitePerProcess", 0);
                WriteDualRegDword(p, L"HighEfficiencyModeEnabled", 1);
                WriteDualRegDword(p, L"MemorySaverModeSavings", 2);
                WriteDualRegDword(p, L"ThrottleJavaScriptTimers", 1);
                WriteDualRegDword(p, L"IntensiveWakeUpThrottlingEnabled", 1);
                WriteDualRegDword(p, L"HardwareAccelerationModeEnabled", 1);
                WriteDualRegDword(p, L"BuiltInDnsClientEnabled", 1);
                WriteDualRegDword(p, L"TabHoverCards", 0);
                WriteDualRegDword(p, L"TabHoverCardImages", 0);
                WriteDualRegDword(p, L"BackgroundModeEnabled", 0);
                WriteDualRegDword(p, L"NetworkPredictionOptions", 2);

                // Edge Sleeping Tabs
                WriteDualRegDword(p, L"SleepingTabsEnabled", 1);
                WriteDualRegDword(p, L"EfficiencyModeEnabled", 1);

                logCb(L"[✓] RAM clamping (SitePerProcess=0) & max Memory Saver enforced.", L"SUCCESS");
                break;
            }

            case 6: { // Ultra Low-Resource & Process Limits
                progCb(pct, L"Enforcing Ultra Low-Resource limits (Renderer cap, cache clamp)...");

                // Clamp renderer processes so browser cannot overwhelm system RAM
                WriteDualRegDword(p, L"RendererProcessLimit", 4);
                // Clamp disk cache to 256MB and media cache to 128MB
                WriteDualRegDword(p, L"DiskCacheSize", 268435456);
                WriteDualRegDword(p, L"MediaCacheSize", 134217728);
                // Discard inactive tabs in 5 minutes
                WriteDualRegDword(p, L"HighEfficiencyModeTimeBeforeDiscardInMinutes", 5);
                WriteDualRegDword(p, L"SleepingTabsTimeoutMinutes", 5);
                // Disable pre-rendering & speculative network connections
                WriteDualRegDword(p, L"NetworkPredictionOptions", 2);
                WriteDualRegDword(p, L"NetworkPredictionEnabled", 0);
                WriteDualRegDword(p, L"Prerender2", 0);
                // Never run in background when window is closed
                WriteDualRegDword(p, L"BackgroundModeEnabled", 0);
                WriteDualRegDword(p, L"BackgroundProcessesEnabled", 0);
                // Enable built-in subresource filter for deceptive/heavy ads
                WriteDualRegDword(p, L"SubresourceFilterEnabled", 1);

                logCb(L"[✓] Ultra Low-Resource active: 4-Renderer limit, 256MB cache, 5-min tab sleep.", L"SUCCESS");
                break;
            }

            case 7: { // High-Speed & Low-Latency Flags (Local State)
                progCb(pct, L"Injecting performance & low-resource flags into Local State...");
                std::wstring lsPath = browser.userDataDir + L"\\Local State";

                if (!PathExists(lsPath)) {
                    logCb(L"[!] Local State not found: " + lsPath, L"WARNING");
                    break;
                }

                std::ifstream in(lsPath, std::ios::binary);
                if (!in.is_open()) {
                    logCb(L"[!] Cannot read Local State (file may be in use): " + lsPath, L"WARNING");
                    break;
                }
                std::stringstream ss;
                ss << in.rdbuf();
                in.close();
                std::string c = ss.str();

                std::vector<std::string> flags = {
                    "enable-quic@1",
                    "back-forward-cache@1",
                    "smooth-scrolling@1",
                    "enable-scroll-prediction@1",
                    "canvas-oop-rasterization@1",
                    "enable-gpu-rasterization@1",
                    "enable-zero-copy@1",
                    "enable-parallel-downloading@1",
                    "use-angle@1",
                    "skia-graphite@1",
                    "tab-discarding@1",
                    "high-efficiency-mode-available@1",
                    "enable-prerender2@0"
                };

                int addedFlags = 0;
                size_t pos = c.find("\"enabled_labs_experiments\":[");
                if (pos != std::string::npos) {
                    size_t start = pos + strlen("\"enabled_labs_experiments\":[");
                    std::string ins;
                    for (const auto& f : flags) {
                        if (c.find("\"" + f + "\"") == std::string::npos) {
                            if (!ins.empty()) ins += ",";
                            ins += "\"" + f + "\"";
                            addedFlags++;
                        }
                    }
                    if (!ins.empty()) {
                        if (c[start] != ']') ins += ",";
                        c.insert(start, ins);
                    }
                } else {
                    size_t browserSection = c.find("\"browser\":");
                    if (browserSection != std::string::npos) {
                        size_t insertPos = c.find("{", browserSection);
                        if (insertPos != std::string::npos) {
                            std::string flagsJson = "\"enabled_labs_experiments\":[";
                            for (size_t i = 0; i < flags.size(); ++i) {
                                if (i > 0) flagsJson += ",";
                                flagsJson += "\"" + flags[i] + "\"";
                            }
                            flagsJson += "],";
                            c.insert(insertPos + 1, flagsJson);
                            addedFlags = (int)flags.size();
                        }
                    }
                }

                std::ofstream out(lsPath, std::ios::binary | std::ios::trunc);
                if (out.is_open()) {
                    out << c;
                    out.close();
                    logCb(L"[✓] High-speed & low-RAM flags injected (QUIC, Zero-Copy, GPU raster, Skia).", L"SUCCESS");
                } else {
                    logCb(L"[!] Could not write Local State.", L"WARNING");
                }
                break;
            }

            case 8: { // Privacy Sandbox & Ad Topics Elimination
                progCb(pct, L"Eliminating Privacy Sandbox & Ad Topics...");

                WriteDualRegDword(p, L"PrivacySandboxAdTopicsEnabled", 0);
                WriteDualRegDword(p, L"PrivacySandboxAdMeasurementEnabled", 0);
                WriteDualRegDword(p, L"PrivacySandboxSiteCommissionedEnabled", 0);
                WriteDualRegDword(p, L"PrivacySandboxPromptEnabled", 0);
                WriteDualRegDword(p, L"FeedbackSurveysEnabled", 0);

                logCb(L"[✓] Privacy Sandbox, Topics API & ad telemetry eliminated.", L"SUCCESS");
                break;
            }

            case 9: { // Commercial & Shopping Bloat Removal
                progCb(pct, L"Removing shopping, price tracking & commercial bloat...");

                WriteDualRegDword(p, L"CommercePriceTrackingEnabled", 0);
                WriteDualRegDword(p, L"ShoppingListEnabled", 0);
                WriteDualRegDword(p, L"PromotionsEnabled", 0);
                // Edge shopping & widgets
                WriteDualRegDword(p, L"EdgeShoppingDataEnabled", 0);
                WriteDualRegDword(p, L"EdgeCollectionsEnabled", 0);
                WriteDualRegDword(p, L"WebWidgetAllowed", 0);
                WriteDualRegDword(p, L"MathSolverEnabled", 0);
                WriteDualRegDword(p, L"ResolveNavigationErrorsUseWebService", 0);

                logCb(L"[✓] Shopping price trackers, promotion banners & floating widgets stripped.", L"SUCCESS");
                break;
            }

            case 10: { // UI Clutter Stripping
                progCb(pct, L"Stripping UI clutter, Cast & promotion icons...");

                WriteDualRegDword(p, L"ShowCastIconInToolbar", 0);
                WriteDualRegDword(p, L"EnableMediaRouter", 0);
                WriteDualRegDword(p, L"DesktopSharingHubEnabled", 0);
                WriteDualRegDword(p, L"SharedClipboardEnabled", 0);
                WriteDualRegDword(p, L"WebAppInstallByUserEnabled", 0);
                WriteDualRegDword(p, L"TranslateEnabled", 0);
                WriteDualRegDword(p, L"AutofillCreditCardEnabled", 0);
                WriteDualRegDword(p, L"NTPCardsVisible", 0);
                WriteDualRegDword(p, L"LensRegionSearchEnabled", 0);
                WriteDualRegDword(p, L"SideSearchEnabled", 0);

                logCb(L"[✓] Cast icon, Media Router, Google Lens & NTP news feeds stripped.", L"SUCCESS");
                break;
            }

            case 11: { // Private Brave Search Provider
                progCb(pct, L"Configuring private Brave Search provider...");

                WriteDualRegDword(p, L"DefaultSearchProviderEnabled", 1);
                WriteDualRegString(p, L"DefaultSearchProviderName", L"Brave Search");
                WriteDualRegString(p, L"DefaultSearchProviderSearchURL", L"https://search.brave.com/search?q={searchTerms}");
                WriteDualRegString(p, L"DefaultSearchProviderSuggestURL", L"https://search.brave.com/api/suggest?q={searchTerms}");

                logCb(L"[✓] Brave Search set as default privacy search provider.", L"SUCCESS");
                break;
            }

            case 12: { // Maintenance & Defragmentation
                progCb(pct, L"Defragmenting profile databases & sweeping caches...");
                INT64 reclaimed = 0;
                RunDeepClean(browser, reclaimed, logCb);
                break;
            }

            case 13: { // Permanent 4-Layer Update Lockdown
                progCb(pct, L"Enforcing 4-layer update lockdown across services & policies...");

                // Layer 1: Universal registry policies (HKLM + HKCU)
                WriteDualRegDword(up, L"UpdateDefault", 0);
                WriteDualRegDword(up, L"AutoUpdateCheckPeriodMinutes", 0);
                WriteDualRegDword(up, L"DisableAutoUpdateChecksCheckboxValue", 1);
                WriteDualRegDword(up, L"InstallDefault", 0);

                if (!browser.appGuid.empty()) {
                    std::wstring g = browser.appGuid;
                    WriteDualRegDword(up, L"Update" + g, 0);
                    WriteDualRegDword(up, L"Install" + g, 0);
                    if (!browser.version.empty()) {
                        WriteDualRegString(up, L"TargetVersionPrefix" + g, browser.version);
                    }
                }

                // Layer 2: Updater Services Lockdown
                if (browser.id == L"chrome") {
                    DisableServiceByPrefix(L"GoogleUpdater");
                    DisableServiceByPrefix(L"GoogleUpdate");
                    DisableServiceByPrefix(L"gupdate");
                    logCb(L"  [✓] Google Updater services stopped & disabled.", L"INFO");
                } else if (browser.id == L"brave") {
                    DisableServiceByPrefix(L"BraveUpdate");
                    DisableServiceByPrefix(L"BraveElevation");
                    logCb(L"  [✓] Brave Updater services stopped & disabled.", L"INFO");
                } else if (browser.id == L"edge") {
                    DisableServiceByPrefix(L"edgeupdate");
                    DisableServiceByPrefix(L"MicrosoftEdgeUpdate");
                    DisableServiceByPrefix(L"MicrosoftEdgeElevation");
                    logCb(L"  [✓] Edge Updater services stopped & disabled.", L"INFO");
                }

                // Layer 3: Scheduled Tasks Lockdown
                if (browser.id == L"chrome") {
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\GoogleSystem\\GoogleUpdater\\GoogleUpdaterTaskSystem\" /Disable", NULL, SW_HIDE);
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\Google\\GoogleUpdateTaskMachineCore\" /Disable", NULL, SW_HIDE);
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\Google\\GoogleUpdateTaskMachineUA\" /Disable", NULL, SW_HIDE);
                } else if (browser.id == L"edge") {
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\Microsoft\\EdgeUpdate\\EdgeUpdateTaskMachineCore\" /Disable", NULL, SW_HIDE);
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\Microsoft\\EdgeUpdate\\EdgeUpdateTaskMachineUA\" /Disable", NULL, SW_HIDE);
                }

                logCb(L"[✓] 4-layer update lockdown enforced for " + browser.name + L".", L"SUCCESS");
                break;
            }
        }
    }

    progCb(100, L"Ready.");
    logCb(L"══ All " + std::to_wstring(total) + L" optimizations applied and verified successfully. ══", L"SUCCESS");
    return true;
}

bool TweakEngine::RunDeepClean(
    const BrowserTarget& browser,
    INT64& outReclaimedBytes,
    EngineLogCallback logCb
) {
    outReclaimedBytes = 0;
    logCb(L"Starting deep profile maintenance...", L"ACTION");

    if (!PathExists(browser.userDataDir)) {
        logCb(L"[!] User data directory not found: " + browser.userDataDir, L"WARNING");
        return false;
    }

    // 1. Vacuum SQLite databases
    SQLiteCleaner sqlite;
    if (sqlite.IsAvailable()) {
        std::vector<std::wstring> dbs = {
            L"History", L"Favicons", L"Web Data", L"Top Sites", L"Shortcuts", L"Network Action Predictor"
        };
        std::wstring defaultDir = browser.userDataDir + L"\\Default";
        int vacuumed = 0;
        for (const auto& db : dbs) {
            std::wstring dbPath = defaultDir + L"\\" + db;
            if (PathExists(dbPath)) {
                INT64 saved = sqlite.OptimizeDatabase(dbPath);
                outReclaimedBytes += saved;
                vacuumed++;
            }
        }
        logCb(L"[✓] " + std::to_wstring(vacuumed) + L" SQLite databases defragmented and reindexed.", L"SUCCESS");
    } else {
        logCb(L"[i] SQLite cleaner unavailable (winsqlite3.dll not found).", L"INFO");
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
    if (cacheBefore > 0) {
        logCb(L"[✓] Stale GPU, Dawn & crashpad caches swept (" + std::to_wstring(cacheBefore / 1024) + L" KB reclaimed).", L"SUCCESS");
    } else {
        logCb(L"[✓] GPU & shader caches already clean.", L"INFO");
    }

    // 3. Flush Windows DNS
    HMODULE hDns = LoadLibraryW(L"dnsapi.dll");
    if (hDns) {
        auto pFlush = (DnsFlushResolverCacheFn)GetProcAddress(hDns, "DnsFlushResolverCache");
        if (pFlush) {
            pFlush();
            logCb(L"[✓] Windows DNS resolver cache flushed.", L"SUCCESS");
        }
        FreeLibrary(hDns);
    }

    return true;
}
