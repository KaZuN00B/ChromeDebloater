#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "browser_target.h"
#include "audit_engine.h"
#include "network_shield.h"

enum class TweakPreset {
    Maximum,         // All 15 optimizations & security modules
    Balanced,        // Everyday fast, privacy & low resource
    PrivacyOnly,     // Anti-telemetry, Network Shield & DoH focus
    UltraLowResource,// Max RAM savings, renderer limits, cache clamps & tab sleep
    GamingMode       // Unlocked FPS, zero VSync, GPU raster, low RAM clamp
};

struct TweakItem {
    int id;
    std::wstring category;
    std::wstring title;
    std::wstring description;
    std::wstring impactTag; // "HIGH IMPACT", "RECOMMENDED", "SAFE", "ULTRA-LOW RAM", "SHIELD"
    bool enabled;
    bool isChromeOnly;
};

typedef std::function<void(const std::wstring& msg, const std::wstring& level)> EngineLogCallback;
typedef std::function<void(int percent, const std::wstring& currentAction)> EngineProgressCallback;

class TweakEngine {
public:
    static std::vector<TweakItem> GetAllTweaks();
    static void ApplyPreset(std::vector<TweakItem>& tweaks, TweakPreset preset);

    static bool ExecuteTweaks(
        const BrowserTarget& browser,
        const std::vector<int>& activeTweakIds,
        EngineLogCallback logCb,
        EngineProgressCallback progCb
    );

    static bool RunDeepClean(
        const BrowserTarget& browser,
        INT64& outReclaimedBytes,
        EngineLogCallback logCb
    );

    // Helpers
    static bool WriteDualRegDword(const std::wstring& subKey, const std::wstring& name, DWORD value);
    static bool WriteDualRegString(const std::wstring& subKey, const std::wstring& name, const std::wstring& value);
    static bool WriteRegDword(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, DWORD value);
    static bool WriteRegString(HKEY hRoot, const std::wstring& subKey, const std::wstring& name, const std::wstring& value);
    static bool WriteRegList(HKEY hRoot, const std::wstring& subKey, const std::wstring& listName, const std::vector<std::wstring>& items);
    static bool DeleteRegValue(HKEY hRoot, const std::wstring& subKey, const std::wstring& name);
    static bool RemoveDirRecursive(const std::wstring& path);
    static bool PermanentlySilenceDefaultAndPinPrompts(const BrowserTarget& browser, EngineLogCallback logCb);
    static void KillProcesses(const std::wstring& exeName);
};
