#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include <vector>
#include "ui/window.h"
#include "engine/browser_target.h"
#include "engine/audit_engine.h"
#include "engine/tweak_engine.h"
#include "engine/backup_engine.h"
#include "engine/update_checker.h"
#include "engine/network_shield.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

bool IsProcessElevated() {
    BOOL isElevated = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return isElevated != FALSE;
}

bool RelaunchAsAdmin(PWSTR pCmdLine) {
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, MAX_PATH)) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.lpParameters = pCmdLine;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;

        if (ShellExecuteExW(&sei)) {
            return true;
        }
    }
    return false;
}

static HANDLE s_hConsoleOut = NULL;

void PrintConsole(const std::wstring& text) {
    if (!s_hConsoleOut || s_hConsoleOut == INVALID_HANDLE_VALUE) {
        s_hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!s_hConsoleOut || s_hConsoleOut == INVALID_HANDLE_VALUE) {
            AttachConsole(ATTACH_PARENT_PROCESS);
            s_hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
        }
        if (!s_hConsoleOut || s_hConsoleOut == INVALID_HANDLE_VALUE) {
            s_hConsoleOut = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        }
    }
    if (s_hConsoleOut && s_hConsoleOut != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        if (!WriteConsoleW(s_hConsoleOut, text.c_str(), (DWORD)text.length(), &written, NULL)) {
            int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.length(), NULL, 0, NULL, NULL);
            if (len > 0) {
                std::string u8(len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.length(), &u8[0], len, NULL, NULL);
                WriteFile(s_hConsoleOut, u8.data(), (DWORD)len, &written, NULL);
            }
        }
    }
}

int RunCliMode(int argc, wchar_t** argv) {
    HANDLE hExisting = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!hExisting || hExisting == INVALID_HANDLE_VALUE) {
        AttachConsole(ATTACH_PARENT_PROCESS);
    }

    PrintConsole(L"\n======================================================\n");
    PrintConsole(L"  ChromeDebloater Pro Native CLI (Elevated Mode)\n");
    PrintConsole(L"======================================================\n\n");

    std::wstring targetKey = L"chrome";
    bool runAll = false;
    bool runLowResource = false;
    bool runAudit = false;
    bool runClean = false;
    bool checkUpdates = false;
    bool lockUpdates = false;
    bool unlockUpdates = false;
    bool enableShield = false;
    bool disableShield = false;
    bool enableHosts = false;
    bool disableHosts = false;
    bool enableFw = false;
    bool disableFw = false;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--all" || arg == L"-a") {
            runAll = true;
        } else if (arg == L"--low-resource" || arg == L"-l" || arg == L"--low") {
            runLowResource = true;
        } else if (arg == L"--audit") {
            runAudit = true;
        } else if (arg == L"--clean" || arg == L"-c") {
            runClean = true;
        } else if (arg == L"--check-updates" || arg == L"-u" || arg == L"--updates") {
            checkUpdates = true;
        } else if (arg == L"--lock-updates") {
            lockUpdates = true;
        } else if (arg == L"--unlock-updates") {
            unlockUpdates = true;
        } else if (arg == L"--shield" || arg == L"-s") {
            enableShield = true;
        } else if (arg == L"--unshield") {
            disableShield = true;
        } else if (arg == L"--hosts") {
            enableHosts = true;
        } else if (arg == L"--unhosts") {
            disableHosts = true;
        } else if (arg == L"--firewall") {
            enableFw = true;
        } else if (arg == L"--unfirewall") {
            disableFw = true;
        } else if ((arg == L"--browser" || arg == L"-b") && (i + 1 < argc)) {
            targetKey = argv[++i];
        } else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            PrintConsole(L"Usage: ChromeDebloater.exe [options]\n\n");
            PrintConsole(L"Options:\n");
            PrintConsole(L"  --all, -a                 Apply all 15 hardening and debloat modules\n");
            PrintConsole(L"  --shield, -s              Activate Network Shield (Windows Firewall + Hosts file block)\n");
            PrintConsole(L"  --unshield                Deactivate Network Shield (removes rules and restores hosts)\n");
            PrintConsole(L"  --hosts                   Block Google telemetry & tracking domains via hosts file\n");
            PrintConsole(L"  --unhosts                 Remove Google telemetry entries from hosts file\n");
            PrintConsole(L"  --firewall                Add Windows Defender Firewall outbound rules for updaters\n");
            PrintConsole(L"  --unfirewall              Delete ChromeDebloater Windows Firewall rules\n");
            PrintConsole(L"  --low-resource, -l        Apply Ultra Low-Resource profile (RAM clamp & renderer limit)\n");
            PrintConsole(L"  --audit                   Run system audit and display hardening score\n");
            PrintConsole(L"  --clean, -c               Perform deep profile SQLite vacuum and cache sweep\n");
            PrintConsole(L"  --check-updates, -u       Check upstream release channels for Chrome, Brave, and Edge\n");
            PrintConsole(L"  --lock-updates            Freeze target browser version & block updater services\n");
            PrintConsole(L"  --unlock-updates          Restore target browser update policies to defaults\n");
            PrintConsole(L"  --browser, -b <name>      Target browser: chrome (default), brave, or edge\n");
            PrintConsole(L"  --help, -h                Display this help screen\n\n");
            PrintConsole(L"Launch without parameters to open the modern graphical interface.\n\n");
            return 0;
        }
    }

    auto browsers = DetectAllBrowsers();

    if (checkUpdates) {
        PrintConsole(L"[*] Querying upstream release channels for Chrome, Brave, and Edge...\n\n");
        for (const auto& b : browsers) {
            PrintConsole(L"[*] Checking " + b.name + L"...\n");
            auto info = UpdateChecker::CheckBrowser(b);
            PrintConsole(L"  Browser:   " + info.browserName + L"\n");
            PrintConsole(L"  Installed: " + (info.isInstalled ? (L"v" + info.installedVersion) : L"Not Detected") + L"\n");
            PrintConsole(L"  Upstream:  " + (info.latestVersion.empty() ? L"Unreachable / Offline" : (L"v" + info.latestVersion)) + L"\n");
            PrintConsole(L"  Lockdown:  " + std::wstring(info.isUpdateLocked ? L"Active (Updates Frozen)" : L"Inactive (Updates Allowed)") + L"\n");
            PrintConsole(L"  Status:    " + info.statusText + L"\n");
            PrintConsole(L"  ----------------------------------------------------\n");
        }
        PrintConsole(L"\n[✓] Upstream release verification complete.\n\n");
        return 0;
    }

    BrowserTarget* pTarget = nullptr;
    for (auto& b : browsers) {
        if (b.id == targetKey) {
            pTarget = &b;
            break;
        }
    }

    if (!pTarget) {
        PrintConsole(L"[X] Unknown browser: " + targetKey + L"\n");
        return 1;
    }

    PrintConsole(L"[*] Target Browser: " + pTarget->name + (pTarget->version.empty() ? L"" : (L" (v" + pTarget->version + L")")) + L"\n");

    auto logPrinter = [](const std::wstring& msg, const std::wstring& lvl) {
        PrintConsole(L"  [" + lvl + L"] " + msg + L"\n");
    };

    if (enableShield) {
        PrintConsole(L"[*] Activating full Windows Network Shield (Firewall + Hosts)...\n");
        NetworkShield::EnableAll(*pTarget, logPrinter);
        PrintConsole(L"[✓] Network Shield engaged successfully.\n\n");
        return 0;
    }

    if (disableShield) {
        PrintConsole(L"[*] Deactivating Windows Network Shield...\n");
        NetworkShield::DisableAll(logPrinter);
        PrintConsole(L"[✓] Network Shield deactivated.\n\n");
        return 0;
    }

    if (enableHosts) {
        PrintConsole(L"[*] Blocking Google telemetry domains in Windows hosts file...\n");
        NetworkShield::EnableHostsBlock(logPrinter);
        return 0;
    }

    if (disableHosts) {
        PrintConsole(L"[*] Restoring Windows hosts file...\n");
        NetworkShield::DisableHostsBlock(logPrinter);
        return 0;
    }

    if (enableFw) {
        PrintConsole(L"[*] Configuring Windows Defender Firewall rules...\n");
        NetworkShield::EnableFirewallBlock(*pTarget, logPrinter);
        return 0;
    }

    if (disableFw) {
        PrintConsole(L"[*] Deleting ChromeDebloater Windows Firewall rules...\n");
        NetworkShield::DisableFirewallBlock(logPrinter);
        return 0;
    }

    if (lockUpdates) {
        PrintConsole(L"[*] Enforcing 4-layer update lockdown for " + pTarget->name + L"...\n");
        std::vector<int> ids = { 13 };
        TweakEngine::ExecuteTweaks(*pTarget, ids, logPrinter, [](int, const std::wstring&) {});
        PrintConsole(L"[✓] " + pTarget->name + L" frozen at current installed version.\n\n");
        return 0;
    }

    if (unlockUpdates) {
        PrintConsole(L"[*] Restoring update policies for " + pTarget->name + L"...\n");
        TweakEngine::DeleteRegValue(HKEY_LOCAL_MACHINE, pTarget->updateKey, L"UpdateDefault");
        TweakEngine::DeleteRegValue(HKEY_CURRENT_USER, pTarget->updateKey, L"UpdateDefault");
        TweakEngine::DeleteRegValue(HKEY_LOCAL_MACHINE, pTarget->updateKey, L"AutoUpdateCheckPeriodMinutes");
        TweakEngine::DeleteRegValue(HKEY_CURRENT_USER, pTarget->updateKey, L"AutoUpdateCheckPeriodMinutes");
        PrintConsole(L"[✓] " + pTarget->name + L" updater policies restored.\n\n");
        return 0;
    }

    if (runAudit) {
        PrintConsole(L"[*] Running system audit scan...\n");
        auto rep = AuditEngine::PerformAudit(*pTarget);
        PrintConsole(L"\n------------------------------------------------------\n");
        PrintConsole(L"  HARDENING & OPTIMIZATION SCORE: " + std::to_wstring(rep.score) + L"%\n");
        PrintConsole(L"  Protected Subsystems: " + std::to_wstring(rep.optimizedCount) + L"\n");
        PrintConsole(L"  Bloated Subsystems:   " + std::to_wstring(rep.bloatedCount) + L"\n");
        PrintConsole(L"  Reclaimable Disk:     " + std::to_wstring(rep.reclaimableBytes / (1024 * 1024)) + L" MB\n");
        PrintConsole(L"------------------------------------------------------\n\n");
        return 0;
    }

    if (runClean) {
        PrintConsole(L"[*] Executing deep profile cleaner...\n");
        INT64 reclaimed = 0;
        TweakEngine::RunDeepClean(*pTarget, reclaimed, logPrinter);
        PrintConsole(L"\n[✓] Deep clean complete. Reclaimed " + std::to_wstring(reclaimed / 1024) + L" KB whitespace.\n\n");
        return 0;
    }

    if (runLowResource) {
        PrintConsole(L"[*] Applying Ultra Low-Resource Profile...\n");
        auto allTweaks = TweakEngine::GetAllTweaks();
        TweakEngine::ApplyPreset(allTweaks, TweakPreset::UltraLowResource);
        std::vector<int> ids;
        for (const auto& tw : allTweaks) {
            if (tw.enabled) ids.push_back(tw.id);
        }

        bool ok = TweakEngine::ExecuteTweaks(*pTarget, ids, logPrinter, [](int, const std::wstring&) {});
        PrintConsole(L"\n[✓] Ultra Low-Resource optimization complete with status: " + std::wstring(ok ? L"SUCCESS" : L"WARNINGS") + L"\n\n");
        return ok ? 0 : 1;
    }

    if (runAll) {
        PrintConsole(L"[*] Creating automated safety restore point...\n");
        std::wstring bPath;
        BackupEngine::CreateSnapshot(*pTarget, bPath);
        PrintConsole(L"[✓] Snapshot saved to: " + bPath + L"\n");

        PrintConsole(L"[*] Enforcing all 15 hardening, debloat, and shield modules...\n");
        auto allTweaks = TweakEngine::GetAllTweaks();
        std::vector<int> ids;
        for (const auto& tw : allTweaks) ids.push_back(tw.id);

        bool ok = TweakEngine::ExecuteTweaks(*pTarget, ids, logPrinter, [](int, const std::wstring&) {});

        PrintConsole(L"\n[✓] Optimization sweep completed with status: " + std::wstring(ok ? L"SUCCESS" : L"WARNINGS") + L"\n\n");
        return ok ? 0 : 1;
    }

    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--all" || arg == L"-a" || arg == L"--low-resource" || arg == L"-l" || arg == L"--low" ||
                arg == L"--audit" || arg == L"--clean" || arg == L"-c" || arg == L"--check-updates" || arg == L"-u" ||
                arg == L"--updates" || arg == L"--lock-updates" || arg == L"--unlock-updates" ||
                arg == L"--shield" || arg == L"-s" || arg == L"--unshield" || arg == L"--hosts" || arg == L"--unhosts" ||
                arg == L"--firewall" || arg == L"--unfirewall" ||
                arg == L"--help" || arg == L"-h" || arg == L"/?") {
                
                if ((arg == L"--all" || arg == L"-a" || arg == L"--low-resource" || arg == L"-l" ||
                     arg == L"--clean" || arg == L"-c" || arg == L"--lock-updates" || arg == L"--unlock-updates" ||
                     arg == L"--shield" || arg == L"-s" || arg == L"--unshield" || arg == L"--hosts" || arg == L"--unhosts" ||
                     arg == L"--firewall" || arg == L"--unfirewall") && !IsProcessElevated()) {
                    if (RelaunchAsAdmin(pCmdLine)) {
                        LocalFree(argv);
                        return 0;
                    }
                }
                int res = RunCliMode(argc, argv);
                LocalFree(argv);
                return res;
            }
        }
    }
    if (argv) LocalFree(argv);

    if (!IsProcessElevated()) {
        if (RelaunchAsAdmin(pCmdLine)) {
            return 0;
        } else {
            MessageBoxW(
                NULL,
                L"ChromeDebloater Pro requires Administrator privileges to configure Windows enterprise policies and firewall rules.\nPlease run as administrator.",
                L"Elevation Required",
                MB_ICONERROR | MB_OK
            );
            return 1;
        }
    }

    if (!AppWindow::InitializeAndShow(hInstance, nCmdShow)) {
        MessageBoxW(NULL, L"Failed to initialize modern user interface.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
