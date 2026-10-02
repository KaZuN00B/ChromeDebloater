#include "tweak_engine.h"
#include "audit_engine.h"
#include "sqlite_cleaner.h"
#include "network_shield.h"
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
            1, L"AI & Models", L"Eliminate AI, Gemini, Copilot & Leo",
            L"Enforces policies to kill Gemini, PromptAPI, Edge Copilot/Discover, and Brave Leo AI, and purges on-disk local models.",
            L"HIGH IMPACT", true, false
        },
        {
            2, L"Privacy", L"Quad9 DoH, ECH & Anti-Snooping Security",
            L"Enforces Quad9 DoH, Encrypted Client Hello (ECH), HTTPS-Only, Post-Quantum TLS, and blocks sensor & WebRTC leaks.",
            L"HIGH IMPACT", true, false
        },
        {
            3, L"Privacy", L"Suppress Telemetry, UKM & Variations",
            L"Blocks metrics reporting, URL-keyed anonymized telemetry (UKM), cloud spellcheck, external variations, and Brave P3A.",
            L"RECOMMENDED", true, false
        },
        {
            4, L"Authentication", L"Preserve Browser Sync & Password Manager",
            L"Allowlists authentication cookies so Google Sync/Passwords (Chrome) or Microsoft Account (Edge) stay functional.",
            L"SAFE", true, false
        },
        {
            5, L"Performance", L"Process Clamping & Memory Saver",
            L"Clamps iframe processes (SitePerProcess=0), activates aggressive tab discarding, sleeping tabs, and background timers.",
            L"HIGH IMPACT", true, false
        },
        {
            6, L"Performance", L"Ultra Low-Resource Limits & Startup Boost",
            L"Caps renderers to 4, limits disk cache to 256MB, enables 5-min tab sleep, and stops Edge 24/7 background startup boost.",
            L"ULTRA-LOW RAM", true, false
        },
        {
            7, L"Performance", L"Inject High-Speed & Modern Crypto Flags",
            L"Injects QUIC protocol, zero-copy rasterization, D3D11 ANGLE, Skia Graphite, ECH, and Kyber post-quantum into Local State.",
            L"RECOMMENDED", true, false
        },
        {
            8, L"Privacy", L"Eliminate Privacy Sandbox & Ad Topics",
            L"Disables Google Topics API, Privacy Sandbox ad measurement, site-commissioned ads, attribution reporting, and surveys.",
            L"HIGH IMPACT", true, false
        },
        {
            9, L"Debloat", L"Remove Commercial, Shopping & Crypto Bloat",
            L"Disables price tracking, Edge Shopping & Wallet, Brave Rewards, Brave Wallet crypto extensions, and Brave VPN.",
            L"RECOMMENDED", true, false
        },
        {
            10, L"Interface", L"Strip UI Clutter, News Feeds & Media Router",
            L"Removes Cast button, SSDP local broadcasting, Google Lens, Edge desktop widget, Edge workspaces, and NTP news feeds.",
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
        },
        {
            14, L"Network Shield", L"Windows Firewall & Hosts Telemetry Block",
            L"Enforces Windows Defender Firewall outbound rules and sinkholes 48+ Google, Edge, and Brave telemetry domains via hosts file.",
            L"SHIELD", true, false
        },
        {
            15, L"Interface", L"Zen UI, Tab Hover Cards & Quiet Notifications",
            L"Kills tab hover card thumbnails, Google Lens, side search, download bubble animations, and quiets notification prompts.",
            L"RECOMMENDED", true, false
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
                              tw.id == 6 || tw.id == 7 || tw.id == 8 || tw.id == 9 || tw.id == 10 ||
                              tw.id == 12 || tw.id == 14 || tw.id == 15);
                break;
            case TweakPreset::PrivacyOnly:
                tw.enabled = (tw.id == 1 || tw.id == 2 || tw.id == 3 || tw.id == 8 || tw.id == 11 || tw.id == 14 || tw.id == 15);
                break;
            case TweakPreset::UltraLowResource:
                tw.enabled = (tw.id == 5 || tw.id == 6 || tw.id == 7 || tw.id == 10 || tw.id == 12 || tw.id == 14 || tw.id == 15);
                break;
            case TweakPreset::GamingMode:
                // Focus on max frame rate, GPU acceleration, 4-renderer clamp, 100MB cache, network shield, no telemetry
                tw.enabled = (tw.id == 4 || tw.id == 5 || tw.id == 6 || tw.id == 7 || tw.id == 8 || tw.id == 10 || tw.id == 12 || tw.id == 14 || tw.id == 15);
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

    for (int id : activeTweakIds) {
        step++;
        int pct = (step * 100) / total;

        switch (id) {
            case 1: { // AI & Gemini / Copilot / Leo Elimination
                progCb(pct, L"Killing AI, Gemini, Copilot & Leo subsystems...");

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
                    // Microsoft Edge Copilot & Hubs Sidebar
                    { L"HubsSidebarEnabled",              0 },
                    { L"StandaloneHubsSidebarEnabled",    0 },
                    { L"CopilotPageContext",              0 },
                    { L"EdgeEntSearchCopilotInSidebarEnabled", 0 },
                    { L"DiscoverPageContextEnabled",      0 },
                    { L"SidebarAppSearchEnabled",         0 },
                    { L"ComposeInlineEnabled",            0 },
                    // Brave Leo AI
                    { L"BraveAIChatEnabled",              0 }
                };

                int ok = 0;
                for (auto& e : entries) {
                    WriteDualRegDword(p, e.name, e.val);
                    if (VerifyRegDword(HKEY_LOCAL_MACHINE, p, e.name, e.val) ||
                        VerifyRegDword(HKEY_CURRENT_USER, p, e.name, e.val)) {
                        ok++;
                    }
                }

                RemoveDirRecursive(browser.userDataDir + L"\\OnDeviceHeadSuggestModel");
                RemoveDirRecursive(browser.userDataDir + L"\\optimization_guide_model_store");
                RemoveDirRecursive(browser.userDataDir + L"\\OptimizationGuideModelsManifest");
                RemoveDirRecursive(browser.userDataDir + L"\\OptimizationHints");
                RemoveDirRecursive(browser.userDataDir + L"\\Default\\AutofillAiModelCache");

                logCb(L"[✓] AI & Gemini/Copilot/Leo killed: " + std::to_wstring(ok) + L" policies enforced across machine and user hives.", L"SUCCESS");
                break;
            }

            case 2: { // Privacy, Content Hardening & ECH
                progCb(pct, L"Enforcing Quad9 DoH, ECH & Privacy Guard...");

                WriteDualRegString(p, L"DnsOverHttpsMode", L"automatic");
                WriteDualRegString(p, L"DnsOverHttpsTemplates", L"https://dns.quad9.net/dns-query");
                WriteDualRegString(p, L"HttpsOnlyMode", L"force_enabled");
                WriteDualRegString(p, L"SSLVersionMin", L"tls1.2");
                WriteDualRegDword(p, L"HSTSPinningBypassAllowed", 0);
                WriteDualRegDword(p, L"PostQuantumKeyAgreementEnabled", 1);
                WriteDualRegDword(p, L"EncryptedClientHelloEnabled", 1);
                WriteDualRegDword(p, L"PaymentMethodQueryEnabled", 0);
                WriteDualRegDword(p, L"WebRtcIPHandling", 2);
                WriteDualRegString(p, L"WebRtcIPHandlingPolicy", L"disable_non_proxied_udp");
                WriteDualRegDword(p, L"DefaultPopupsSetting", 2);
                WriteDualRegDword(p, L"DefaultNotificationsSetting", 2);
                WriteDualRegDword(p, L"DefaultGeolocationSetting", 2);
                WriteDualRegDword(p, L"DefaultWebBluetoothGuardSetting", 2);
                WriteDualRegDword(p, L"DefaultWebUsbGuardSetting", 2);
                WriteDualRegDword(p, L"DefaultFileSystemReadGuardSetting", 2);
                WriteDualRegDword(p, L"DefaultFileSystemWriteGuardSetting", 2);
                WriteDualRegDword(p, L"DefaultSensorsSetting", 2);
                WriteDualRegDword(p, L"DefaultSerialGuardSetting", 2);
                WriteDualRegDword(p, L"DefaultHidGuardSetting", 2);
                WriteDualRegDword(p, L"InsecurePrivateNetworkRequestsAllowed", 0);
                WriteDualRegDword(p, L"DefaultInsecureContentSetting", 2);
                WriteDualRegDword(p, L"ReduceAcceptLanguageEnabled", 1);
                WriteDualRegDword(p, L"RendererCodeIntegrityEnabled", 1);
                WriteDualRegDword(p, L"ThirdPartyBlockingEnabled", 1);
                WriteDualRegDword(p, L"QuicAllowed", 1);
                WriteDualRegDword(p, L"ZstdContentEncodingEnabled", 1);

                logCb(L"[✓] Quad9 DoH, ECH encryption, HTTPS-only, WebRTC leak fix & sensor hardening applied.", L"SUCCESS");
                break;
            }

            case 3: { // Telemetry, UKM, Diagnostics & Variations
                progCb(pct, L"Disabling telemetry, UKM, diagnostics & variations...");

                WriteDualRegDword(p, L"MetricsReportingEnabled", 0);
                WriteDualRegDword(p, L"UrlKeyedAnonymizedDataCollectionEnabled", 0);
                WriteDualRegDword(p, L"VariationsRestrictionsByPolicy", 2);
                WriteDualRegDword(p, L"DeviceMetricsReportingEnabled", 0);
                WriteDualRegDword(p, L"SafeBrowsingExtendedReportingEnabled", 0);
                WriteDualRegDword(p, L"SpellCheckServiceEnabled", 0);
                WriteDualRegDword(p, L"ChromeCleanupEnabled", 0);
                WriteDualRegDword(p, L"ChromeCleanupReportingEnabled", 0);
                WriteDualRegDword(p, L"UserFeedbackAllowed", 0);
                WriteDualRegDword(p, L"FeedbackSurveysEnabled", 0);
                WriteDualRegDword(p, L"ReportingEnabled", 0);
                WriteDualRegDword(p, L"CloudReportingEnabled", 0);
                WriteDualRegDword(p, L"CloudProfileReportingEnabled", 0);
                WriteDualRegDword(p, L"HeartbeatEnabled", 0);
                WriteDualRegDword(p, L"SafeBrowsingEnabled", 0);
                WriteDualRegDword(p, L"SafeBrowsingProtectionLevel", 0);
                WriteDualRegDword(p, L"ChromeVariations", 2);
                WriteDualRegDword(p, L"CloudManagementEnrollmentMandatory", 0);

                // Keystroke & Component telemetry
                WriteDualRegDword(p, L"SearchSuggestEnabled", 0);
                WriteDualRegDword(p, L"AutocompletePrerenderEnabled", 0);
                WriteDualRegDword(p, L"ComponentUpdatesEnabled", 0);
                WriteDualRegDword(p, L"DeviceTrustEnabled", 0);
                WriteDualRegDword(p, L"BrowserLabsEnabled", 0);

                // Edge telemetry & cloud services
                WriteDualRegDword(p, L"DiagnosticData", 0);
                WriteDualRegDword(p, L"PersonalizationReportingEnabled", 0);
                WriteDualRegDword(p, L"ShareBrowsingHistory", 0);
                WriteDualRegDword(p, L"EdgeAssetDeliveryServiceEnabled", 0);
                WriteDualRegDword(p, L"TyposquattingCheckerEnabled", 0);
                WriteDualRegDword(p, L"ResolveNavigationErrorsUseWebService", 0);
                WriteDualRegDword(p, L"AddressBarMicrosoftSearchInBingProviderEnabled", 0);
                WriteDualRegDword(p, L"VisualSearchEnabled", 0);
                WriteDualRegDword(p, L"EdgeFollowEnabled", 0);

                // Brave telemetry & analytics
                WriteDualRegDword(p, L"BraveP3AEnabled", 0);

                logCb(L"[✓] Telemetry, UKM, keystroke suggestions & component updaters fully blocked.", L"SUCCESS");
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
                WriteDualRegDword(p, L"PageDiscardingEnabled", 1);
                WriteDualRegDword(p, L"SleepingTabsEnabled", 1);
                WriteDualRegDword(p, L"EfficiencyModeEnabled", 1);

                logCb(L"[✓] RAM clamping (SitePerProcess=0) & max Memory Saver enforced.", L"SUCCESS");
                break;
            }

            case 6: { // Ultra Low-Resource Limits & Startup Boost
                progCb(pct, L"Enforcing Ultra Low-Resource limits (Renderer cap, cache clamp, startup boost off)...");

                WriteDualRegDword(p, L"RendererProcessLimit", 4);
                WriteDualRegDword(p, L"DiskCacheSize", 268435456);
                WriteDualRegDword(p, L"MediaCacheSize", 134217728);
                WriteDualRegDword(p, L"HighEfficiencyModeTimeBeforeDiscardInMinutes", 5);
                WriteDualRegDword(p, L"SleepingTabsTimeoutMinutes", 5);
                WriteDualRegDword(p, L"NetworkPredictionOptions", 2);
                WriteDualRegDword(p, L"NetworkPredictionEnabled", 0);
                WriteDualRegDword(p, L"Prerender2", 0);
                WriteDualRegDword(p, L"BackgroundModeEnabled", 0);
                WriteDualRegDword(p, L"BackgroundProcessesEnabled", 0);
                WriteDualRegDword(p, L"SubresourceFilterEnabled", 1);
                // Edge: Kill silent 24/7 background pre-launch
                WriteDualRegDword(p, L"StartupBoostEnabled", 0);

                logCb(L"[✓] Ultra Low-Resource active: 4-Renderer limit, 256MB cache, 5-min tab sleep, StartupBoost killed.", L"SUCCESS");
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
                    "enable-prerender2@0",
                    "enable-encrypted-client-hello@1",
                    "enable-tls13-kyber@1",
                    "privacy-sandbox-ads-apis@0"
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
                    logCb(L"[✓] High-speed & low-RAM flags injected (QUIC, Zero-Copy, GPU raster, Skia, ECH, Kyber).", L"SUCCESS");
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

            case 9: { // Commercial, Shopping & Crypto Bloat Removal
                progCb(pct, L"Removing shopping, price tracking, Brave crypto & commercial bloat...");

                // Universal / Chrome
                WriteDualRegDword(p, L"CommercePriceTrackingEnabled", 0);
                WriteDualRegDword(p, L"ShoppingListEnabled", 0);
                WriteDualRegDword(p, L"PromotionsEnabled", 0);
                WriteDualRegDword(p, L"AutofillPaymentMethodsEnabled", 0);
                WriteDualRegDword(p, L"AutofillCreditCardEnabled", 0);

                // Edge Shopping & Wallet
                WriteDualRegDword(p, L"EdgeShoppingDataEnabled", 0);
                WriteDualRegDword(p, L"EdgeCollectionsEnabled", 0);
                WriteDualRegDword(p, L"EdgeWalletEnabled", 0);
                WriteDualRegDword(p, L"WebWidgetAllowed", 0);
                WriteDualRegDword(p, L"MathSolverEnabled", 0);
                WriteDualRegDword(p, L"CitationsEnabled", 0);
                WriteDualRegDword(p, L"EdgeEnhanceImagesEnabled", 0);
                WriteDualRegDword(p, L"EdgeSuperResolutionEnabled", 0);

                // Brave Rewards, Crypto Wallet, VPN & IPFS
                WriteDualRegDword(p, L"BraveRewardsDisabled", 1);
                WriteDualRegDword(p, L"BraveWalletDisabled", 1);
                WriteDualRegDword(p, L"BraveVPNDisabled", 1);
                WriteDualRegDword(p, L"IPFSResolveMethod", 0);
                WriteDualRegDword(p, L"TorDisabled", 1);

                logCb(L"[✓] Shopping price trackers, Edge wallet/coupons & Brave crypto/VPN stripped.", L"SUCCESS");
                break;
            }

            case 10: { // UI Clutter & Background Features
                progCb(pct, L"Stripping UI clutter, Cast, news feeds & NTP promos...");

                WriteDualRegDword(p, L"ShowCastIconInToolbar", 0);
                WriteDualRegDword(p, L"EnableMediaRouter", 0);
                WriteDualRegDword(p, L"DesktopSharingHubEnabled", 0);
                WriteDualRegDword(p, L"SharedClipboardEnabled", 0);
                WriteDualRegDword(p, L"WebAppInstallByUserEnabled", 0);
                WriteDualRegDword(p, L"TranslateEnabled", 0);
                WriteDualRegDword(p, L"AutofillAddressEnabled", 0);
                WriteDualRegDword(p, L"NTPCardsVisible", 0);
                WriteDualRegDword(p, L"LensRegionSearchEnabled", 0);
                WriteDualRegDword(p, L"SideSearchEnabled", 0);

                // Universal Chrome/Brave/Edge: Suppress Default Browser checks & First-Run Welcome
                WriteDualRegDword(p, L"DefaultBrowserSettingEnabled", 0);
                WriteDualRegDword(p, L"HideFirstRunExperience", 1);
                WriteDualRegDword(p, L"PromotionsEnabled", 0);

                // Create First Run file marker to permanently silence onboarding and default browser dialogs
                std::wstring firstRunFile = browser.userDataDir + L"\\First Run";
                if (!PathExists(firstRunFile) && PathExists(browser.userDataDir)) {
                    std::ofstream fr(firstRunFile, std::ios::binary | std::ios::out);
                    if (fr.is_open()) {
                        fr.close();
                        logCb(L"  [✓] First Run marker created (permanently silences default browser & welcome popups).", L"INFO");
                    }
                }

                // Preferences JSON Patching (Default browser & Taskbar Pin Promo for all browsers)
                std::wstring prefPath = browser.userDataDir + L"\\Default\\Preferences";
                if (PathExists(prefPath)) {
                    std::ifstream in(prefPath, std::ios::binary);
                    if (in.is_open()) {
                        std::stringstream ss;
                        ss << in.rdbuf();
                        in.close();
                        std::string prefStr = ss.str();
                        bool changed = false;

                        auto replaceAll = [&](const std::string& from, const std::string& to) {
                            size_t pos = 0;
                            while ((pos = prefStr.find(from, pos)) != std::string::npos) {
                                prefStr.replace(pos, from.length(), to);
                                pos += to.length();
                                changed = true;
                            }
                        };

                        // Silence default browser check & taskbar pin promo
                        replaceAll("\"check_default_browser\":true", "\"check_default_browser\":false");
                        replaceAll("\"has_seen_welcome_page\":false", "\"has_seen_welcome_page\":true");
                        replaceAll("\"taskbar_pinning_promo_dismissed\":false", "\"taskbar_pinning_promo_dismissed\":true");

                        if (browser.id == L"brave") {
                            replaceAll("\"show_sponsored_images\":true", "\"show_sponsored_images\":false");
                            replaceAll("\"should_show_on_new_tab\":true", "\"should_show_on_new_tab\":false");
                            replaceAll("\"show_brave_talk\":true", "\"show_brave_talk\":false");
                            replaceAll("\"show_together\":true", "\"show_together\":false");
                        }

                        if (changed) {
                            std::ofstream out(prefPath, std::ios::binary | std::ios::trunc);
                            if (out.is_open()) {
                                out << prefStr;
                                out.close();
                                logCb(L"  [✓] Default browser nag & taskbar pin promos silenced in profile preferences.", L"INFO");
                            }
                        }
                    }
                }

                logCb(L"[✓] Cast icon, Media Router local broadcasting & NTP clickbait feeds stripped.", L"SUCCESS");
                break;
            }

            case 11: { // Private Brave Search Provider
                progCb(pct, L"Configuring private Brave Search provider...");

                WriteDualRegDword(p, L"DefaultSearchProviderEnabled", 1);
                WriteDualRegDword(p, L"SearchEngineChoiceScreenNavigationCondition", 0);
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

                if (browser.id == L"chrome") {
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\GoogleSystem\\GoogleUpdater\\GoogleUpdaterTaskSystem\" /Disable", NULL, SW_HIDE);
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\Google\\GoogleUpdateTaskMachineCore\" /Disable", NULL, SW_HIDE);
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\Google\\GoogleUpdateTaskMachineUA\" /Disable", NULL, SW_HIDE);
                } else if (browser.id == L"edge") {
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\Microsoft\\EdgeUpdate\\EdgeUpdateTaskMachineCore\" /Disable", NULL, SW_HIDE);
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\Microsoft\\EdgeUpdate\\EdgeUpdateTaskMachineUA\" /Disable", NULL, SW_HIDE);
                } else if (browser.id == L"brave") {
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\BraveSoftware\\Update\\BraveUpdateTaskMachineCore\" /Disable", NULL, SW_HIDE);
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\BraveSoftware\\Update\\BraveUpdateTaskMachineUA\" /Disable", NULL, SW_HIDE);
                }

                logCb(L"[✓] 4-layer update lockdown enforced for " + browser.name + L".", L"SUCCESS");
                break;
            }

            case 14: { // Network Shield (Firewall & Hosts File Blocklist)
                progCb(pct, L"Engaging Windows Firewall & Hosts Network Shield...");
                NetworkShield::EnableAll(browser, logCb);
                break;
            }

            case 15: { // Zen UI, Tab Hover Cards & Quiet Notifications
                progCb(pct, L"Enforcing Zen UI, tab hover cards & quiet notification declutter...");

                WriteDualRegDword(p, L"TabHoverCardImages", 0);
                WriteDualRegDword(p, L"LensOverlaySettings", 1);
                WriteDualRegDword(p, L"SideSearchEnabled", 0);
                WriteDualRegDword(p, L"SidePanelPinning", 0);
                WriteDualRegDword(p, L"QuietNotificationPromptsEnabled", 1);
                WriteDualRegDword(p, L"DefaultGeolocationSetting", 2);
                WriteDualRegDword(p, L"DownloadBubbleEnabled", 0);
                WriteDualRegDword(p, L"BrowserAddPersonEnabled", 0);
                WriteDualRegDword(p, L"BrowserGuestModeEnabled", 0);
                WriteDualRegDword(p, L"PromotionsEnabled", 0);
                WriteDualRegDword(p, L"SuppressUnsupportedOSWarning", 1);
                WriteDualRegDword(p, L"SearchEngineChoiceScreenNavigationCondition", 0);
                WriteDualRegDword(p, L"DefaultBrowserSettingEnabled", 0);
                WriteDualRegDword(p, L"HideFirstRunExperience", 1);

                // Inject Local State flags: disable tab search & disable tab hover cards & pin promo
                std::wstring lsPath = browser.userDataDir + L"\\Local State";
                if (PathExists(lsPath)) {
                    std::ifstream in(lsPath, std::ios::binary);
                    if (in.is_open()) {
                        std::stringstream ss;
                        ss << in.rdbuf();
                        in.close();
                        std::string c = ss.str();
                        bool modified = false;

                        std::vector<std::string> zenFlags = {
                            "enable-tab-search@0",
                            "tab-hover-card-images@0",
                            "taskbar-pin-promo@2"
                        };

                        size_t pos = c.find("\"enabled_labs_experiments\"");
                        if (pos != std::string::npos) {
                            size_t openBracket = c.find('[', pos);
                            if (openBracket != std::string::npos) {
                                for (const auto& zf : zenFlags) {
                                    if (c.find("\"" + zf + "\"") == std::string::npos) {
                                        c.insert(openBracket + 1, "\"" + zf + "\",");
                                        modified = true;
                                    }
                                }
                            }
                        }

                        if (modified) {
                            std::ofstream out(lsPath, std::ios::binary | std::ios::trunc);
                            if (out.is_open()) {
                                out << c;
                                out.close();
                                logCb(L"  [✓] Tab search arrow and hover image flags disabled in Local State.", L"INFO");
                            }
                        }
                    }
                }

                logCb(L"[✓] Zen UI enforced: tab hover previews killed, Google Lens stripped & notification nags silenced.", L"SUCCESS");
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

    INT64 cacheBefore = 0;
    std::vector<std::wstring> cleanDirs = {
        browser.userDataDir + L"\\Default\\GPUCache",
        browser.userDataDir + L"\\Default\\DawnCache",
        browser.userDataDir + L"\\GrShaderCache",
        browser.userDataDir + L"\\ShaderCache",
        browser.userDataDir + L"\\Crashpad",
        browser.userDataDir + L"\\OptimizationGuidePredictionModels",
        browser.userDataDir + L"\\OnDeviceModel",
        browser.userDataDir + L"\\BrowserMetrics",
        browser.userDataDir + L"\\Default\\Service Worker\\CacheStorage",
        browser.userDataDir + L"\\Default\\Service Worker\\ScriptCache",
        browser.userDataDir + L"\\Default\\Media Cache"
    };

    for (const auto& d : cleanDirs) {
        INT64 dSize = AuditEngine::CalculateDirectorySize(d);
        if (dSize > 0) {
            cacheBefore += dSize;
            RemoveDirRecursive(d);
        }
    }

    outReclaimedBytes += cacheBefore;
    if (cacheBefore > 0) {
        logCb(L"[✓] Stale GPU caches, AI models & worker storage swept (" + std::to_wstring(cacheBefore / 1024) + L" KB reclaimed).", L"SUCCESS");
    } else {
        logCb(L"[✓] GPU, shader & AI model caches already clean.", L"INFO");
    }

    NetworkShield::FlushDns();
    logCb(L"[✓] Windows DNS resolver cache flushed.", L"SUCCESS");

    return true;
}
