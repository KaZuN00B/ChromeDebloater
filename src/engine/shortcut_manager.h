#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "browser_target.h"
#include "tweak_engine.h"

class ShortcutManager {
public:
    // Scans Desktop, Start Menu, and Taskbar shortcuts for the browser and injects privacy flags
    static bool HardenShortcuts(const BrowserTarget& browser, EngineLogCallback logCb);

    // Strips injected privacy flags from all shortcuts
    static bool RestoreShortcuts(const BrowserTarget& browser, EngineLogCallback logCb);

    // Launches an ephemeral sanitized browser profile in %TEMP% with zero telemetry
    static bool LaunchSanitizedProfile(const BrowserTarget& browser, EngineLogCallback logCb);

    // Installs dedicated Hardened Gaming Mode desktop shortcut (.lnk) and batch launcher (.bat)
    static bool InstallGamingMode(const BrowserTarget& browser, EngineLogCallback logCb);

    // Returns the standard hardened argument string
    static std::wstring GetHardenedArguments();

    // Returns the ultra gaming mode argument string (unlocked FPS, zero VSync, GPU raster, low RAM clamp)
    static std::wstring GetGamingArguments();

private:
    static std::vector<std::wstring> FindBrowserShortcuts(const BrowserTarget& browser);
};
