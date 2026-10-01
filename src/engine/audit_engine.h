#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "browser_target.h"

enum class AuditStatus {
    Optimized,      // Green check
    Bloated,        // Red warning (needs optimization)
    Attention,      // Yellow alert
    NotApplicable   // Gray
};

struct AuditItem {
    int id;
    std::wstring name;
    std::wstring category;
    AuditStatus status;
    std::wstring description;
    std::wstring recommendation;
};

struct AuditReport {
    int score = 0;              // 0 to 100%
    int optimizedCount = 0;
    int bloatedCount = 0;
    INT64 reclaimableBytes = 0;
    std::vector<AuditItem> items;
};

class AuditEngine {
public:
    static AuditReport PerformAudit(const BrowserTarget& browser);

    static bool ReadRegDword(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, DWORD& outVal);
    static bool ReadRegString(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, std::wstring& outVal);
    static INT64 CalculateDirectorySize(const std::wstring& dirPath);
};
