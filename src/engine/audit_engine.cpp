#include "audit_engine.h"
#include "network_shield.h"
#include <iostream>

bool AuditEngine::ReadRegDword(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, DWORD& outVal) {
    HKEY hKey;
    if (RegOpenKeyExW(hRoot, subKey.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = REG_DWORD;
    DWORD size = sizeof(DWORD);
    LSTATUS status = RegQueryValueExW(hKey, name.c_str(), 0, &type, (LPBYTE)&outVal, &size);
    RegCloseKey(hKey);
    return (status == ERROR_SUCCESS && type == REG_DWORD);
}

bool AuditEngine::ReadRegString(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, std::wstring& outVal) {
    HKEY hKey;
    if (RegOpenKeyExW(hRoot, subKey.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    wchar_t buf[512] = { 0 };
    DWORD size = sizeof(buf);
    DWORD type = REG_SZ;
    LSTATUS status = RegQueryValueExW(hKey, name.c_str(), 0, &type, (LPBYTE)buf, &size);
    RegCloseKey(hKey);
    if (status == ERROR_SUCCESS) {
        outVal = buf;
        return true;
    }
    return false;
}

INT64 AuditEngine::CalculateDirectorySize(const std::wstring& dirPath) {
    if (!PathExists(dirPath)) return 0;

    INT64 total = 0;
    std::wstring search = dirPath + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        std::wstring fullPath = dirPath + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            total += CalculateDirectorySize(fullPath);
        } else {
            LARGE_INTEGER sz;
            sz.HighPart = fd.nFileSizeHigh;
            sz.LowPart = fd.nFileSizeLow;
            total += sz.QuadPart;
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return total;
}

static bool CheckDword(HKEY root, const std::wstring& subKey, const std::wstring& name, DWORD expected) {
    DWORD val = 0;
    return AuditEngine::ReadRegDword(root, subKey, name, val) && (val == expected);
}

static bool CheckDualDword(const std::wstring& subKey, const std::wstring& name, DWORD expected) {
    return CheckDword(HKEY_LOCAL_MACHINE, subKey, name, expected) ||
           CheckDword(HKEY_CURRENT_USER, subKey, name, expected);
}

AuditReport AuditEngine::PerformAudit(const BrowserTarget& browser) {
    AuditReport report;
    const std::wstring& p = browser.policyKey;
    const std::wstring& up = browser.updateKey;

    // ─────────────────────────────────────────────────────────────────────────
    // 1. AI & Gemini / Copilot / Leo Subsystems
    // ─────────────────────────────────────────────────────────────────────────
    bool aiDisabled = CheckDualDword(p, L"GenAiDefaultSettings", 2) || CheckDualDword(p, L"OptimizationGuideAllowed", 0);
    if (browser.id == L"edge") {
        aiDisabled = aiDisabled || CheckDualDword(p, L"HubsSidebarEnabled", 0) || CheckDualDword(p, L"ComposeInlineEnabled", 0);
    } else if (browser.id == L"brave") {
        aiDisabled = aiDisabled || CheckDualDword(p, L"BraveAIChatEnabled", 0);
    }
    
    INT64 aiModelSize = 0;
    aiModelSize += CalculateDirectorySize(browser.userDataDir + L"\\OnDeviceHeadSuggestModel");
    aiModelSize += CalculateDirectorySize(browser.userDataDir + L"\\optimization_guide_model_store");
    aiModelSize += CalculateDirectorySize(browser.userDataDir + L"\\OptimizationGuideModelsManifest");
    aiModelSize += CalculateDirectorySize(browser.userDataDir + L"\\Default\\AutofillAiModelCache");

    AuditItem itemAi;
    itemAi.id = 1;
    itemAi.category = L"AI & Models";
    itemAi.name = L"AI, Gemini, Copilot & Leo";
    if (aiDisabled && aiModelSize == 0) {
        itemAi.status = AuditStatus::Optimized;
        itemAi.description = L"AI features blocked by enterprise policy. 0 on-disk models found.";
        itemAi.recommendation = L"Protected.";
        report.optimizedCount++;
    } else {
        itemAi.status = AuditStatus::Bloated;
        itemAi.description = L"AI features active or foundational model files present on disk.";
        itemAi.recommendation = L"Apply AI elimination policy & model purge.";
        report.bloatedCount++;
    }
    report.items.push_back(itemAi);
    report.reclaimableBytes += aiModelSize;

    // ─────────────────────────────────────────────────────────────────────────
    // 2. Telemetry, UKM, Crash Reports & Diagnostics
    // ─────────────────────────────────────────────────────────────────────────
    bool metricsOff = CheckDualDword(p, L"MetricsReportingEnabled", 0);
    bool ukmOff = CheckDualDword(p, L"UrlKeyedAnonymizedDataCollectionEnabled", 0);
    bool sbOff = CheckDualDword(p, L"SafeBrowsingExtendedReportingEnabled", 0);
    if (browser.id == L"edge") {
        metricsOff = metricsOff && CheckDualDword(p, L"DiagnosticData", 0);
    } else if (browser.id == L"brave") {
        metricsOff = metricsOff && CheckDualDword(p, L"BraveP3AEnabled", 0);
    }

    AuditItem itemTel;
    itemTel.id = 2;
    itemTel.category = L"Privacy";
    itemTel.name = L"Telemetry, UKM & Diagnostic Reporting";
    if (metricsOff && (ukmOff || sbOff)) {
        itemTel.status = AuditStatus::Optimized;
        itemTel.description = L"Telemetry, UKM logging, background diagnostics and variations blocked.";
        itemTel.recommendation = L"Protected.";
        report.optimizedCount++;
    } else {
        itemTel.status = AuditStatus::Bloated;
        itemTel.description = L"Diagnostic telemetry and crash reports actively transmitting.";
        itemTel.recommendation = L"Suppress telemetry & background diagnostics.";
        report.bloatedCount++;
    }
    report.items.push_back(itemTel);

    // ─────────────────────────────────────────────────────────────────────────
    // 3. DNS-over-HTTPS (Quad9 Encrypted DNS) & ECH
    // ─────────────────────────────────────────────────────────────────────────
    std::wstring dohMode, dohTpl;
    ReadRegString(HKEY_LOCAL_MACHINE, p, L"DnsOverHttpsMode", dohMode);
    ReadRegString(HKEY_LOCAL_MACHINE, p, L"DnsOverHttpsTemplates", dohTpl);
    if (dohTpl.empty()) {
        ReadRegString(HKEY_CURRENT_USER, p, L"DnsOverHttpsTemplates", dohTpl);
    }
    bool dohOk = (!dohTpl.empty() && dohTpl.find(L"quad9.net") != std::wstring::npos);
    bool echOn = CheckDualDword(p, L"EncryptedClientHelloEnabled", 1) || CheckDualDword(p, L"PostQuantumKeyAgreementEnabled", 1);

    AuditItem itemDoh;
    itemDoh.id = 3;
    itemDoh.category = L"Privacy";
    itemDoh.name = L"Encrypted DNS (Quad9 DoH)";
    if (dohOk) {
        itemDoh.status = AuditStatus::Optimized;
        itemDoh.description = L"Encrypted Quad9 DoH active (malware blocking + no logs).";
        itemDoh.recommendation = L"Protected.";
        report.optimizedCount++;
    } else {
        itemDoh.status = AuditStatus::Bloated;
        itemDoh.description = L"Unencrypted plaintext ISP DNS or default resolver active.";
        itemDoh.recommendation = L"Enforce Quad9 secure DoH.";
        report.bloatedCount++;
    }
    report.items.push_back(itemDoh);

    // ─────────────────────────────────────────────────────────────────────────
    // 4. Memory Saver & Process Clamping
    // ─────────────────────────────────────────────────────────────────────────
    bool sppOff = CheckDualDword(p, L"SitePerProcess", 0);
    bool memSavOn = CheckDualDword(p, L"HighEfficiencyModeEnabled", 1) || CheckDualDword(p, L"EfficiencyModeEnabled", 1);

    AuditItem itemPerf;
    itemPerf.id = 4;
    itemPerf.category = L"Performance";
    itemPerf.name = L"Memory Saver & Process Clamping";
    if (sppOff && memSavOn) {
        itemPerf.status = AuditStatus::Optimized;
        itemPerf.description = L"Process clamping (SitePerProcess=0) and Memory Saver active.";
        itemPerf.recommendation = L"Optimized.";
        report.optimizedCount++;
    } else {
        itemPerf.status = AuditStatus::Bloated;
        itemPerf.description = L"High RAM consumption: iframe multiplier enabled without clamping.";
        itemPerf.recommendation = L"Clamp subframe processes & enable Memory Saver.";
        report.bloatedCount++;
    }
    report.items.push_back(itemPerf);

    // ─────────────────────────────────────────────────────────────────────────
    // 5. Ultra Low-Resource Limits & Startup Boost
    // ─────────────────────────────────────────────────────────────────────────
    bool procLimit = CheckDualDword(p, L"RendererProcessLimit", 4);
    bool cacheLimit = CheckDualDword(p, L"DiskCacheSize", 268435456);
    bool prefetchOff = CheckDualDword(p, L"NetworkPredictionOptions", 2);
    bool boostOff = true;
    if (browser.id == L"edge") {
        boostOff = CheckDualDword(p, L"StartupBoostEnabled", 0);
    }

    AuditItem itemUltra;
    itemUltra.id = 5;
    itemUltra.category = L"Performance";
    itemUltra.name = L"Ultra Low-Resource Limits & Startup Boost";
    if (procLimit && cacheLimit && prefetchOff && boostOff) {
        itemUltra.status = AuditStatus::Optimized;
        itemUltra.description = L"Renderer cap (4), 256MB cache ceiling, prefetch off & background boost killed.";
        itemUltra.recommendation = L"Max low-resource configuration active.";
        report.optimizedCount++;
    } else {
        itemUltra.status = AuditStatus::Attention;
        itemUltra.description = L"Uncapped background processes, unbounded disk cache or active startup boost.";
        itemUltra.recommendation = L"Apply Ultra Low-Resource policy suite.";
        report.bloatedCount++;
    }
    report.items.push_back(itemUltra);

    // ─────────────────────────────────────────────────────────────────────────
    // 6. Privacy Sandbox & Ad Topics
    // ─────────────────────────────────────────────────────────────────────────
    bool sandboxOff = CheckDualDword(p, L"PrivacySandboxAdTopicsEnabled", 0);

    AuditItem itemSandbox;
    itemSandbox.id = 6;
    itemSandbox.category = L"Privacy";
    itemSandbox.name = L"Privacy Sandbox & Ad Topics";
    if (sandboxOff) {
        itemSandbox.status = AuditStatus::Optimized;
        itemSandbox.description = L"Privacy Sandbox, Topics API & ad telemetry eliminated.";
        itemSandbox.recommendation = L"Protected.";
        report.optimizedCount++;
    } else {
        itemSandbox.status = AuditStatus::Bloated;
        itemSandbox.description = L"Browser browsing habits shared with ad-targeting APIs.";
        itemSandbox.recommendation = L"Eliminate Privacy Sandbox & Topics API.";
        report.bloatedCount++;
    }
    report.items.push_back(itemSandbox);

    // ─────────────────────────────────────────────────────────────────────────
    // 7. Commercial, Shopping & Crypto Bloat
    // ─────────────────────────────────────────────────────────────────────────
    bool shopOff = false;
    if (browser.id == L"edge") {
        shopOff = CheckDualDword(p, L"EdgeShoppingDataEnabled", 0) || CheckDualDword(p, L"EdgeWalletEnabled", 0);
    } else if (browser.id == L"brave") {
        shopOff = CheckDualDword(p, L"BraveRewardsDisabled", 1) || CheckDualDword(p, L"BraveWalletDisabled", 1);
    } else {
        shopOff = CheckDualDword(p, L"CommercePriceTrackingEnabled", 0) || CheckDualDword(p, L"AutofillPaymentMethodsEnabled", 0);
    }

    AuditItem itemShop;
    itemShop.id = 7;
    itemShop.category = L"Debloat";
    itemShop.name = L"Shopping & Commercial Bloat";
    if (shopOff) {
        itemShop.status = AuditStatus::Optimized;
        itemShop.description = L"Shopping price tracking, coupon prompts, wallets & crypto widgets disabled.";
        itemShop.recommendation = L"Clean.";
        report.optimizedCount++;
    } else {
        itemShop.status = AuditStatus::Bloated;
        itemShop.description = L"E-commerce price scanners, shopping coupons or crypto extensions active.";
        itemShop.recommendation = L"Disable shopping & price tracking bloat.";
        report.bloatedCount++;
    }
    report.items.push_back(itemShop);

    // ─────────────────────────────────────────────────────────────────────────
    // 8. UI Clutter (Cast, Sharing Hub, Lens)
    // ─────────────────────────────────────────────────────────────────────────
    bool clutterOff = CheckDualDword(p, L"ShowCastIconInToolbar", 0) && CheckDualDword(p, L"EnableMediaRouter", 0);
    if (browser.id == L"edge") {
        clutterOff = clutterOff || CheckDualDword(p, L"NewTabPageContentEnabled", 0);
    }

    AuditItem itemClutter;
    itemClutter.id = 8;
    itemClutter.category = L"Interface";
    itemClutter.name = L"UI Clutter & Background Features";
    if (clutterOff) {
        itemClutter.status = AuditStatus::Optimized;
        itemClutter.description = L"Cast icon, Media Router, Google Lens and NTP cards stripped.";
        itemClutter.recommendation = L"Clean UI.";
        report.optimizedCount++;
    } else {
        itemClutter.status = AuditStatus::Bloated;
        itemClutter.description = L"Toolbar cluttered with Cast, Sharing Hub and promotional icons.";
        itemClutter.recommendation = L"Strip toolbar clutter and background services.";
        report.bloatedCount++;
    }
    report.items.push_back(itemClutter);

    // ─────────────────────────────────────────────────────────────────────────
    // 9. Shader & GPU Caches Whitespace
    // ─────────────────────────────────────────────────────────────────────────
    INT64 cacheBytes = 0;
    std::vector<std::wstring> auditDirs = {
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
    for (const auto& d : auditDirs) {
        cacheBytes += CalculateDirectorySize(d);
    }
    report.reclaimableBytes += cacheBytes;

    AuditItem itemCache;
    itemCache.id = 9;
    itemCache.category = L"Maintenance";
    itemCache.name = L"Shader & GPU Cache Bloat";
    if (cacheBytes < (5 * 1024 * 1024)) {
        itemCache.status = AuditStatus::Optimized;
        itemCache.description = L"Shader and temporary caches are compact (" + std::to_wstring(cacheBytes / 1024) + L" KB).";
        itemCache.recommendation = L"No action needed.";
        report.optimizedCount++;
    } else {
        itemCache.status = AuditStatus::Bloated;
        itemCache.description = L"Accumulated cache and crash dump whitespace (" + std::to_wstring(cacheBytes / (1024 * 1024)) + L" MB).";
        itemCache.recommendation = L"Run Profile Defragmenter & Deep Cleaner.";
        report.bloatedCount++;
    }
    report.items.push_back(itemCache);

    // ─────────────────────────────────────────────────────────────────────────
    // 10. Version Freeze & Update Lockdown (All browsers)
    // ─────────────────────────────────────────────────────────────────────────
    bool upLocked = CheckDualDword(up, L"UpdateDefault", 0);

    AuditItem itemUp;
    itemUp.id = 10;
    itemUp.category = L"Security";
    itemUp.name = L"Version Freeze & Update Lockdown";
    if (upLocked) {
        itemUp.status = AuditStatus::Optimized;
        itemUp.description = L"Automatic updates frozen. Browser locked at installed version.";
        itemUp.recommendation = L"Locked.";
        report.optimizedCount++;
    } else {
        itemUp.status = AuditStatus::Bloated;
        itemUp.description = L"Automatic background updaters active; version can change unprompted.";
        itemUp.recommendation = L"Enforce 4-layer permanent update lockdown.";
        report.bloatedCount++;
    }
    report.items.push_back(itemUp);

    // ─────────────────────────────────────────────────────────────────────────
    // 11. Windows Network Shield (Firewall & Hosts Sinkhole)
    // ─────────────────────────────────────────────────────────────────────────
    bool shieldActive = NetworkShield::IsHostsBlockActive() || NetworkShield::IsFirewallBlockActive();

    AuditItem itemShield;
    itemShield.id = 11;
    itemShield.category = L"Network Shield";
    itemShield.name = L"Firewall & Hosts Network Shield";
    if (shieldActive) {
        itemShield.status = AuditStatus::Optimized;
        itemShield.description = L"Outbound telemetry blocked at kernel firewall & hosts level.";
        itemShield.recommendation = L"Protected.";
        report.optimizedCount++;
    } else {
        itemShield.status = AuditStatus::Bloated;
        itemShield.description = L"Telemetry and tracking domains can reach Google/Microsoft servers.";
        itemShield.recommendation = L"Engage Windows Firewall & Hosts Network Shield.";
        report.bloatedCount++;
    }
    report.items.push_back(itemShield);

    // ─────────────────────────────────────────────────────────────────────────
    // 12. Zen UI & Quiet Notification Declutter
    // ─────────────────────────────────────────────────────────────────────────
    bool zenActive = CheckDualDword(p, L"TabHoverCardImages", 0) &&
                     CheckDualDword(p, L"LensOverlaySettings", 1) &&
                     CheckDualDword(p, L"QuietNotificationPromptsEnabled", 1) &&
                     CheckDword(HKEY_LOCAL_MACHINE, p, L"DefaultBrowserSettingEnabled", 0);

    AuditItem itemZen;
    itemZen.id = 12;
    itemZen.category = L"Interface";
    itemZen.name = L"Zen UI & Quiet Notifications";
    if (zenActive) {
        itemZen.status = AuditStatus::Optimized;
        itemZen.description = L"Tab hover preview popups killed, Google Lens stripped & default/pin nags permanently silenced.";
        itemZen.recommendation = L"Clean interface.";
        report.optimizedCount++;
    } else {
        itemZen.status = AuditStatus::Bloated;
        itemZen.description = L"Default browser nag, taskbar pin prompts, or intrusive hover thumbnails enabled.";
        itemZen.recommendation = L"Apply Zen UI & anti-nag declutter module.";
        report.bloatedCount++;
    }
    report.items.push_back(itemZen);

    // Score calculation
    int totalAssessable = report.optimizedCount + report.bloatedCount;
    if (totalAssessable > 0) {
        report.score = (report.optimizedCount * 100) / totalAssessable;
    } else {
        report.score = 100;
    }

    return report;
}
