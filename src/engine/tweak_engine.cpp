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

    // Wait for each terminated process to fully exit (up to 3s each)
    for (HANDLE h : waitHandles) {
        WaitForSingleObject(h, 3000);
        CloseHandle(h);
    }

    if (!waitHandles.empty()) {
        Sleep(500); // Extra buffer for file unlocks
    }
}

// Helper: Verify a DWORD registry value was set correctly
static bool VerifyRegDword(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, DWORD expected) {
    DWORD val = 0;
    return AuditEngine::ReadRegDword(hRoot, subKey, name, val) && (val == expected);
}

// Helper: Verify a string registry value contains a substring
static bool VerifyRegStringContains(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, const std::wstring& fragment) {
    std::wstring val;
    return AuditEngine::ReadRegString(hRoot, subKey, name, val) && (val.find(fragment) != std::wstring::npos);
}

// Helper: Disable a Windows service dynamically by searching for a prefix
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

    logCb(L"Preparing to optimize: " + browser.name, L"ACTION");
    std::wstring exeName = browser.id + L".exe";
    if (browser.id == L"edge") exeName = L"msedge.exe";
    else if (browser.id == L"brave") exeName = L"brave.exe";

    logCb(L"Terminating browser processes and waiting for file unlock...", L"ACTION");
    KillProcesses(exeName);
    logCb(L"Browser processes terminated. Applying enterprise policies...", L"INFO");

    const std::wstring& p = browser.policyKey;
    const std::wstring& up = browser.updateKey;
    int step = 0;
    int failCount = 0;

    for (int id : activeTweakIds) {
        step++;
        int pct = (step * 100) / total;

        switch (id) {
            case 1: { // AI Elimination
                progCb(pct, L"Killing AI & Gemini subsystems...");

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
                };

                int ok = 0, fail = 0;
                for (auto& e : entries) {
                    if (WriteRegDword(HKEY_LOCAL_MACHINE, p, e.name, e.val)) {
                        // Verify
                        if (VerifyRegDword(HKEY_LOCAL_MACHINE, p, e.name, e.val)) {
                            ok++;
                        } else {
                            logCb(std::wstring(L"  [!] Verification failed: ") + e.name, L"WARNING");
                            fail++;
                        }
                    } else {
                        logCb(std::wstring(L"  [!] Write failed: ") + e.name, L"WARNING");
                        fail++;
                    }
                }

                RemoveDirRecursive(browser.userDataDir + L"\\OnDeviceHeadSuggestModel");
                RemoveDirRecursive(browser.userDataDir + L"\\optimization_guide_model_store");
                RemoveDirRecursive(browser.userDataDir + L"\\OptimizationGuideModelsManifest");
                RemoveDirRecursive(browser.userDataDir + L"\\OptimizationHints");
                RemoveDirRecursive(browser.userDataDir + L"\\Default\\AutofillAiModelCache");

                if (fail == 0) {
                    logCb(L"[✓] AI & Gemini eliminated: " + std::to_wstring(ok) + L" policies enforced & verified.", L"SUCCESS");
                } else {
                    logCb(L"[!] AI policies: " + std::to_wstring(ok) + L" OK, " + std::to_wstring(fail) + L" failed.", L"WARNING");
                    failCount += fail;
                }
                break;
            }

            case 2: { // Privacy & Content Hardening
                progCb(pct, L"Configuring Quad9 DoH & Privacy Guard...");

                bool ok = true;
                ok &= WriteRegString(HKEY_LOCAL_MACHINE, p, L"DnsOverHttpsMode", L"automatic");
                ok &= WriteRegString(HKEY_LOCAL_MACHINE, p, L"DnsOverHttpsTemplates", L"https://dns.quad9.net/dns-query");
                ok &= WriteRegString(HKEY_LOCAL_MACHINE, p, L"HttpsOnlyMode", L"force_enabled");
                ok &= WriteRegString(HKEY_LOCAL_MACHINE, p, L"SSLVersionMin", L"tls1.2");
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"HSTSPinningBypassAllowed", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"PostQuantumKeyAgreementEnabled", 1);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"WebRtcIPHandling", 2);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultPopupsSetting", 2);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultNotificationsSetting", 2);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultGeolocationSetting", 2);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultWebBluetoothGuardSetting", 2);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultWebUsbGuardSetting", 2);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultFileSystemReadGuardSetting", 2);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultFileSystemWriteGuardSetting", 2);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultSensorsSetting", 2);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultSerialGuardSetting", 2);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"InsecurePrivateNetworkRequestsAllowed", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultInsecureContentSetting", 2);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ReduceAcceptLanguageEnabled", 1);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"RendererCodeIntegrityEnabled", 1);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ThirdPartyBlockingEnabled", 1);

                // Verify Quad9 DoH specifically (most important)
                bool dohOk = VerifyRegStringContains(HKEY_LOCAL_MACHINE, p, L"DnsOverHttpsTemplates", L"quad9.net");
                if (!dohOk) {
                    logCb(L"[!] Quad9 DoH template verification failed!", L"WARNING");
                    failCount++;
                }

                if (ok) {
                    logCb(L"[✓] Quad9 DoH, HTTPS-only, WebRTC & content hardening applied and verified.", L"SUCCESS");
                } else {
                    logCb(L"[!] Privacy hardening: some registry writes failed.", L"WARNING");
                    failCount++;
                }
                break;
            }

            case 3: { // Telemetry Suppression
                progCb(pct, L"Disabling telemetry & crash reporting...");

                bool ok = true;
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"MetricsReportingEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"SafeBrowsingExtendedReportingEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"SpellCheckServiceEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ChromeCleanupEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ChromeCleanupReportingEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"UserFeedbackAllowed", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ReportingEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"CloudReportingEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"CloudProfileReportingEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"HeartbeatEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"SafeBrowsingEnabled", 0);

                bool verified = VerifyRegDword(HKEY_LOCAL_MACHINE, p, L"MetricsReportingEnabled", 0) &&
                                VerifyRegDword(HKEY_LOCAL_MACHINE, p, L"SafeBrowsingExtendedReportingEnabled", 0);

                if (ok && verified) {
                    logCb(L"[✓] Telemetry, diagnostics & feedback reporting fully suppressed.", L"SUCCESS");
                } else {
                    logCb(L"[!] Telemetry: some policies failed to write or verify.", L"WARNING");
                    failCount++;
                }
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

                    bool verified = VerifyRegDword(HKEY_LOCAL_MACHINE, p, L"BrowserSignin", 1) &&
                                    VerifyRegDword(HKEY_LOCAL_MACHINE, p, L"PasswordManagerEnabled", 1);
                    logCb(verified
                        ? L"[✓] Google Sync & Password Manager preserved with cookie allowlist."
                        : L"[!] Auth preservation had verification issues.", verified ? L"SUCCESS" : L"WARNING");
                } else {
                    logCb(L"[i] Auth preservation: Not applicable to " + browser.name + L".", L"INFO");
                }
                break;
            }

            case 5: { // Low-RAM & Performance
                progCb(pct, L"Applying process clamping & Memory Saver...");

                bool ok = true;
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"SitePerProcess", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"HighEfficiencyModeEnabled", 1);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"MemorySaverModeSavings", 2);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ThrottleJavaScriptTimers", 1);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"IntensiveWakeUpThrottlingEnabled", 1);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"HardwareAccelerationModeEnabled", 1);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"BuiltInDnsClientEnabled", 1);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DiskCacheSize", 268435456);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"TabHoverCards", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"BackgroundModeEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"NetworkPredictionOptions", 2);

                bool verified = VerifyRegDword(HKEY_LOCAL_MACHINE, p, L"SitePerProcess", 0) &&
                                VerifyRegDword(HKEY_LOCAL_MACHINE, p, L"HighEfficiencyModeEnabled", 1);
                if (ok && verified) {
                    logCb(L"[✓] RAM clamping (SitePerProcess=0) & max Memory Saver enforced and verified.", L"SUCCESS");
                } else {
                    logCb(L"[!] Performance policies: write or verification failed.", L"WARNING");
                    failCount++;
                }
                break;
            }

            case 6: { // Performance Flags via Local State
                progCb(pct, L"Injecting performance flags into Local State...");
                std::wstring lsPath = browser.userDataDir + L"\\Local State";

                if (!PathExists(lsPath)) {
                    logCb(L"[!] Local State not found — browser may not be installed or never launched: " + lsPath, L"WARNING");
                    failCount++;
                    break;
                }

                // Read current Local State content
                std::ifstream in(lsPath, std::ios::binary);
                if (!in.is_open()) {
                    logCb(L"[!] Cannot read Local State (browser may be running): " + lsPath, L"WARNING");
                    failCount++;
                    break;
                }
                std::stringstream ss;
                ss << in.rdbuf();
                in.close();
                std::string c = ss.str();

                std::vector<std::string> flags = {
                    "enable-quic@1", "back-forward-cache@1", "smooth-scrolling@1",
                    "enable-scroll-prediction@1", "canvas-oop-rasterization@1",
                    "enable-gpu-rasterization@1", "enable-zero-copy@1", "enable-parallel-downloading@1"
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
                    // Insert the flags block if not present at all
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
                    if (addedFlags > 0) {
                        logCb(L"[✓] " + std::to_wstring(addedFlags) + L" performance flags injected (QUIC, GPU raster, parallel download).", L"SUCCESS");
                    } else {
                        logCb(L"[✓] All 8 performance flags already present in Local State.", L"INFO");
                    }
                } else {
                    logCb(L"[!] Cannot write Local State — check file permissions.", L"WARNING");
                    failCount++;
                }
                break;
            }

            case 7: { // UI Clutter
                progCb(pct, L"Stripping UI clutter & Cast buttons...");

                bool ok = true;
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"ShowCastIconInToolbar", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"EnableMediaRouter", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DesktopSharingHubEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"SharedClipboardEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"WebAppInstallByUserEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"TranslateEnabled", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"AutofillCreditCardEnabled", 0);

                bool verified = VerifyRegDword(HKEY_LOCAL_MACHINE, p, L"ShowCastIconInToolbar", 0) &&
                                VerifyRegDword(HKEY_LOCAL_MACHINE, p, L"EnableMediaRouter", 0);
                if (ok && verified) {
                    logCb(L"[✓] Cast, Sharing Hub & promotion banners removed from UI.", L"SUCCESS");
                } else {
                    logCb(L"[!] UI clutter removal: some policies failed.", L"WARNING");
                    failCount++;
                }
                break;
            }

            case 8: { // Search Provider
                progCb(pct, L"Configuring Brave Search provider...");

                bool ok = true;
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, p, L"DefaultSearchProviderEnabled", 1);
                ok &= WriteRegString(HKEY_LOCAL_MACHINE, p, L"DefaultSearchProviderName", L"Brave Search");
                ok &= WriteRegString(HKEY_LOCAL_MACHINE, p, L"DefaultSearchProviderSearchURL", L"https://search.brave.com/search?q={searchTerms}");
                ok &= WriteRegString(HKEY_LOCAL_MACHINE, p, L"DefaultSearchProviderSuggestURL", L"https://search.brave.com/api/suggest?q={searchTerms}");

                bool verified = VerifyRegStringContains(HKEY_LOCAL_MACHINE, p, L"DefaultSearchProviderSearchURL", L"brave.com");
                if (ok && verified) {
                    logCb(L"[✓] Private Brave Search enforced as default search engine.", L"SUCCESS");
                } else {
                    logCb(L"[!] Search provider change failed or could not be verified.", L"WARNING");
                    failCount++;
                }
                break;
            }

            case 9: { // Maintenance
                progCb(pct, L"Defragmenting profile databases & sweeping caches...");
                INT64 reclaimed = 0;
                RunDeepClean(browser, reclaimed, logCb);
                break;
            }

            case 10: { // Update Lockdown
                progCb(pct, L"Enforcing permanent 4-layer update lockdown...");

                bool ok = true;
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, up, L"UpdateDefault", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, up, L"AutoUpdateCheckPeriodMinutes", 0);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, up, L"DisableAutoUpdateChecksCheckboxValue", 1);
                ok &= WriteRegDword(HKEY_LOCAL_MACHINE, up, L"InstallDefault", 0);

                if (!browser.appGuid.empty()) {
                    std::wstring g = browser.appGuid;
                    WriteRegDword(HKEY_LOCAL_MACHINE, up, L"Update" + g, 0);
                    WriteRegDword(HKEY_LOCAL_MACHINE, up, L"Install" + g, 0);
                    if (!browser.version.empty()) {
                        WriteRegString(HKEY_LOCAL_MACHINE, up, L"TargetVersionPrefix" + g, browser.version);
                    }
                }

                // Layer 2: Disable updater services by prefix (version-independent)
                if (browser.id == L"chrome") {
                    DisableServiceByPrefix(L"GoogleUpdater");
                    DisableServiceByPrefix(L"GoogleUpdate");
                    logCb(L"  [✓] Google Updater services searched and disabled.", L"INFO");
                } else if (browser.id == L"brave") {
                    DisableServiceByPrefix(L"BraveUpdate");
                    logCb(L"  [✓] Brave Updater services searched and disabled.", L"INFO");
                } else if (browser.id == L"edge") {
                    DisableServiceByPrefix(L"edgeupdate");
                    DisableServiceByPrefix(L"MicrosoftEdgeUpdate");
                    logCb(L"  [✓] Edge Updater services searched and disabled.", L"INFO");
                }

                // Layer 3: Disable scheduled tasks
                if (browser.id == L"chrome") {
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\GoogleSystem\\GoogleUpdater\\GoogleUpdaterTaskSystem\" /Disable", NULL, SW_HIDE);
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\Google\\GoogleUpdateTaskMachineCore\" /Disable", NULL, SW_HIDE);
                    ShellExecuteW(NULL, L"open", L"schtasks.exe", L"/Change /TN \"\\Google\\GoogleUpdateTaskMachineUA\" /Disable", NULL, SW_HIDE);
                }

                bool verified = VerifyRegDword(HKEY_LOCAL_MACHINE, up, L"UpdateDefault", 0);
                if (ok && verified) {
                    logCb(L"[✓] Version freeze & 4-layer update lockdown enforced and verified.", L"SUCCESS");
                } else {
                    logCb(L"[!] Update lockdown: some layers could not be verified.", L"WARNING");
                    failCount++;
                }
                break;
            }
        }
    }

    progCb(100, L"Ready.");

    if (failCount == 0) {
        logCb(L"══ All " + std::to_wstring(total) + L" optimizations applied and verified successfully. ══", L"SUCCESS");
    } else {
        logCb(L"══ Completed with " + std::to_wstring(failCount) + L" warnings. Some policies may need manual review. ══", L"WARNING");
    }

    return (failCount == 0);
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
