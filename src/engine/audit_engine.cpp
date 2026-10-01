#include "audit_engine.h"
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

AuditReport AuditEngine::PerformAudit(const BrowserTarget& browser) {
    AuditReport report;
    const std::wstring& p = browser.policyKey;
    const std::wstring& up = browser.updateKey;

    // 1. AI & Gemini Subsystems
    DWORD aiVal = 0;
    bool aiDisabled = ReadRegDword(HKEY_LOCAL_MACHINE, p, L"GenAiDefaultSettings", aiVal) && (aiVal == 2);
    DWORD optGuide = 1;
    bool optDisabled = ReadRegDword(HKEY_LOCAL_MACHINE, p, L"OptimizationGuideAllowed", optGuide) && (optGuide == 0);
    
    INT64 aiModelSize = 0;
    aiModelSize += CalculateDirectorySize(browser.userDataDir + L"\\OnDeviceHeadSuggestModel");
    aiModelSize += CalculateDirectorySize(browser.userDataDir + L"\\optimization_guide_model_store");
    aiModelSize += CalculateDirectorySize(browser.userDataDir + L"\\OptimizationGuideModelsManifest");
    aiModelSize += CalculateDirectorySize(browser.userDataDir + L"\\Default\\AutofillAiModelCache");

    AuditItem itemAi;
    itemAi.id = 1;
    itemAi.category = L"AI & Models";
    itemAi.name = L"AI & Gemini Subsystems";
    if (aiDisabled && optDisabled && aiModelSize == 0) {
        itemAi.status = AuditStatus::Optimized;
        itemAi.description = L"All AI & Gemini policies killed. 0 local models present.";
        itemAi.recommendation = L"No action needed.";
        report.optimizedCount++;
    } else {
        itemAi.status = AuditStatus::Bloated;
        itemAi.description = L"AI features are active or on-disk local models found.";
        itemAi.recommendation = L"Apply AI elimination policy and purge model stores.";
        report.bloatedCount++;
    }
    report.items.push_back(itemAi);
    report.reclaimableBytes += aiModelSize;

    // 2. Telemetry & Metrics
    DWORD metrics = 1;
    bool metricsOff = ReadRegDword(HKEY_LOCAL_MACHINE, p, L"MetricsReportingEnabled", metrics) && (metrics == 0);
    DWORD sbExt = 1;
    bool sbOff = ReadRegDword(HKEY_LOCAL_MACHINE, p, L"SafeBrowsingExtendedReportingEnabled", sbExt) && (sbExt == 0);

    AuditItem itemTel;
    itemTel.id = 2;
    itemTel.category = L"Privacy";
    itemTel.name = L"Telemetry & Diagnostics";
    if (metricsOff && sbOff) {
        itemTel.status = AuditStatus::Optimized;
        itemTel.description = L"Google telemetry and diagnostic logging disabled.";
        itemTel.recommendation = L"No action needed.";
        report.optimizedCount++;
    } else {
        itemTel.status = AuditStatus::Bloated;
        itemTel.description = L"Telemetry and diagnostic reporting active.";
        itemTel.recommendation = L"Suppress telemetry and metrics reporting.";
        report.bloatedCount++;
    }
    report.items.push_back(itemTel);

    // 3. DNS-over-HTTPS (Quad9)
    std::wstring dohMode, dohTpl;
    ReadRegString(HKEY_LOCAL_MACHINE, p, L"DnsOverHttpsMode", dohMode);
    ReadRegString(HKEY_LOCAL_MACHINE, p, L"DnsOverHttpsTemplates", dohTpl);
    bool dohOk = (!dohMode.empty() && dohTpl.find(L"quad9.net") != std::wstring::npos);

    AuditItem itemDoh;
    itemDoh.id = 3;
    itemDoh.category = L"Privacy";
    itemDoh.name = L"Encrypted DNS (Quad9 DoH)";
    if (dohOk) {
        itemDoh.status = AuditStatus::Optimized;
        itemDoh.description = L"Encrypted Quad9 DoH active.";
        itemDoh.recommendation = L"No action needed.";
        report.optimizedCount++;
    } else {
        itemDoh.status = AuditStatus::Bloated;
        itemDoh.description = L"Standard unencrypted DNS or browser default.";
        itemDoh.recommendation = L"Enforce Quad9 secure DoH.";
        report.bloatedCount++;
    }
    report.items.push_back(itemDoh);

    // 4. Memory Saver & Process Multiplier
    DWORD spp = 1;
    bool sppOff = ReadRegDword(HKEY_LOCAL_MACHINE, p, L"SitePerProcess", spp) && (spp == 0);
    DWORD memSav = 0;
    bool memSavOn = ReadRegDword(HKEY_LOCAL_MACHINE, p, L"HighEfficiencyModeEnabled", memSav) && (memSav == 1);

    AuditItem itemPerf;
    itemPerf.id = 4;
    itemPerf.category = L"Performance";
    itemPerf.name = L"Memory Saver & Process Isolation";
    if (sppOff && memSavOn) {
        itemPerf.status = AuditStatus::Optimized;
        itemPerf.description = L"Process consolidation active (SitePerProcess=0) and max Memory Saver enabled.";
        itemPerf.recommendation = L"No action needed.";
        report.optimizedCount++;
    } else {
        itemPerf.status = AuditStatus::Bloated;
        itemPerf.description = L"High RAM consumption from aggressive iframe sub-processes.";
        itemPerf.recommendation = L"Clamp iframe processes and enable aggressive tab discard.";
        report.bloatedCount++;
    }
    report.items.push_back(itemPerf);

    // 5. Google Authentication (Sync & Passwords)
    AuditItem itemAuth;
    itemAuth.id = 5;
    itemAuth.category = L"Authentication";
    itemAuth.name = L"Google Sync & Password Manager";
    if (browser.id == L"chrome") {
        DWORD signin = 0, pwd = 0;
        bool signinOk = ReadRegDword(HKEY_LOCAL_MACHINE, p, L"BrowserSignin", signin) && (signin == 1);
        bool pwdOk = ReadRegDword(HKEY_LOCAL_MACHINE, p, L"PasswordManagerEnabled", pwd) && (pwd == 1);

        if (signinOk && pwdOk) {
            itemAuth.status = AuditStatus::Optimized;
            itemAuth.description = L"Google Sync and Password Manager enabled with cookie allowlist.";
            itemAuth.recommendation = L"Preserved.";
            report.optimizedCount++;
        } else {
            itemAuth.status = AuditStatus::Attention;
            itemAuth.description = L"Sync or Passwords might be restricted.";
            itemAuth.recommendation = L"Preserve auth cookie allowlist.";
            report.bloatedCount++;
        }
    } else {
        itemAuth.status = AuditStatus::NotApplicable;
        itemAuth.description = L"Not applicable to this browser.";
        itemAuth.recommendation = L"-";
    }
    report.items.push_back(itemAuth);

    // 6. Shader & GPU Caches
    INT64 cacheBytes = 0;
    cacheBytes += CalculateDirectorySize(browser.userDataDir + L"\\Default\\GPUCache");
    cacheBytes += CalculateDirectorySize(browser.userDataDir + L"\\Default\\DawnCache");
    cacheBytes += CalculateDirectorySize(browser.userDataDir + L"\\GrShaderCache");
    cacheBytes += CalculateDirectorySize(browser.userDataDir + L"\\ShaderCache");
    cacheBytes += CalculateDirectorySize(browser.userDataDir + L"\\Crashpad");
    report.reclaimableBytes += cacheBytes;

    AuditItem itemCache;
    itemCache.id = 6;
    itemCache.category = L"Maintenance";
    itemCache.name = L"Shader & GPU Cache Bloat";
    if (cacheBytes < (5 * 1024 * 1024)) { // Less than 5 MB
        itemCache.status = AuditStatus::Optimized;
        itemCache.description = L"Shader and GPU caches are clean (" + std::to_wstring(cacheBytes / 1024) + L" KB).";
        itemCache.recommendation = L"No action needed.";
        report.optimizedCount++;
    } else {
        itemCache.status = AuditStatus::Bloated;
        itemCache.description = L"Stale shader and crashpad files (" + std::to_wstring(cacheBytes / (1024 * 1024)) + L" MB).";
        itemCache.recommendation = L"Purge stale shader and temporary caches.";
        report.bloatedCount++;
    }
    report.items.push_back(itemCache);

    // 7. Update Lockdown (Chrome only)
    AuditItem itemUp;
    itemUp.id = 7;
    itemUp.category = L"Security";
    itemUp.name = L"Version Freeze & Update Lockdown";
    if (browser.id == L"chrome") {
        DWORD upDef = 1;
        bool upLocked = ReadRegDword(HKEY_LOCAL_MACHINE, up, L"UpdateDefault", upDef) && (upDef == 0);
        if (upLocked) {
            itemUp.status = AuditStatus::Optimized;
            itemUp.description = L"Auto-updates locked. Frozen at installed version.";
            itemUp.recommendation = L"No action needed.";
            report.optimizedCount++;
        } else {
            itemUp.status = AuditStatus::Bloated;
            itemUp.description = L"Auto-updates active; browser will modify version without consent.";
            itemUp.recommendation = L"Enforce 4-layer permanent update lockdown.";
            report.bloatedCount++;
        }
    } else {
        itemUp.status = AuditStatus::NotApplicable;
        itemUp.description = L"Update lockdown applies to Google Chrome.";
        itemUp.recommendation = L"-";
    }
    report.items.push_back(itemUp);

    // Calculate overall Score
    int totalAssessable = report.optimizedCount + report.bloatedCount;
    if (totalAssessable > 0) {
        report.score = (report.optimizedCount * 100) / totalAssessable;
    } else {
        report.score = 100;
    }

    return report;
}
