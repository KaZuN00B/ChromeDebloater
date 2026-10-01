#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "browser.h"

struct TweakOption {
    int id;
    std::wstring category;
    std::wstring label;
    std::wstring description;
    bool checked;
    bool isChromeOnly;
};

typedef std::function<void(const std::wstring& msg, bool isSuccess)> LogCallback;
typedef std::function<void(int percent)> ProgressCallback;

class HardeningEngine {
public:
    static std::vector<TweakOption> GetAvailableTweaks();

    static bool RunTweaks(
        const BrowserTarget& browser,
        const std::vector<int>& selectedTweakIds,
        LogCallback logCb,
        ProgressCallback progressCb
    );

    // Helpers
    static bool SetRegDword(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, DWORD value);
    static bool SetRegString(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, const std::wstring& value);
    static bool SetRegMultiString(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, const std::vector<std::wstring>& values);
    static bool DeleteRegValue(HKEY hRoot, const std::wstring& subKey, const std::wstring& name);
    static bool DeletePathRecursive(const std::wstring& path);
    static void KillBrowserProcesses(const std::wstring& exeName);
    static std::wstring GetBrowserVersion(const std::wstring& exePath);
};
