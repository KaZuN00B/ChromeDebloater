#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "browser_target.h"

struct BackupSnapshot {
    std::wstring id;
    std::wstring timestamp;
    std::wstring browserName;
    std::wstring filePath;
};

class BackupEngine {
public:
    static std::wstring GetBackupDir();
    static bool CreateSnapshot(const BrowserTarget& browser, std::wstring& outPath);
    static std::vector<BackupSnapshot> ListSnapshots();
    static bool RestoreSnapshot(const std::wstring& filePath);
    static bool ResetBrowserPolicies(const BrowserTarget& browser);
};
