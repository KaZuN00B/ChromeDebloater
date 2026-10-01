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

void PrintConsole(const std::wstring& text) {
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    bool created = false;
    if (!hStdOut || hStdOut == INVALID_HANDLE_VALUE) {
        AttachConsole(ATTACH_PARENT_PROCESS);
        hStdOut = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        created = true;
    }
    if (hStdOut && hStdOut != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        if (!WriteConsoleW(hStdOut, text.c_str(), (DWORD)text.length(), &written, NULL)) {
            int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, NULL, 0, NULL, NULL);
            if (len > 0) {
                std::string u8(len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &u8[0], len, NULL, NULL);
                WriteFile(hStdOut, u8.c_str(), (DWORD)strlen(u8.c_str()), &written, NULL);
            }
        }
        if (created) CloseHandle(hStdOut);
    }
}

int RunCliMode(int argc, wchar_t** argv) {
    AttachConsole(ATTACH_PARENT_PROCESS);

    PrintConsole(L"\n======================================================\n");
    PrintConsole(L"  ChromeDebloater Pro Native CLI (Elevated Mode)\n");
    PrintConsole(L"======================================================\n\n");

    std::wstring targetKey = L"chrome";
    bool runAll = false;
    bool runAudit = false;
    bool runClean = false;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--all" || arg == L"-a") {
            runAll = true;
        } else if (arg == L"--audit") {
            runAudit = true;
        } else if (arg == L"--clean" || arg == L"-c") {
            runClean = true;
        } else if ((arg == L"--browser" || arg == L"-b") && (i + 1 < argc)) {
            targetKey = argv[++i];
        } else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            PrintConsole(L"Usage: ChromeDebloater.exe [options]\n\n");
            PrintConsole(L"Options:\n");
            PrintConsole(L"  --all, -a                 Apply all 10 hardening and optimization tweaks\n");
            PrintConsole(L"  --audit                   Run system audit and display hardening score\n");
            PrintConsole(L"  --clean, -c               Perform deep profile SQLite vacuum and cache sweep\n");
            PrintConsole(L"  --browser, -b <name>      Target browser: chrome (default), brave, or edge\n");
            PrintConsole(L"  --help, -h                Display this help screen\n\n");
            PrintConsole(L"Launch without parameters to open the modern dark graphical interface.\n\n");
            return 0;
        }
    }

    auto browsers = DetectAllBrowsers();
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

    PrintConsole(L"[*] Target Browser: " + pTarget->name + L"\n");

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
        TweakEngine::RunDeepClean(*pTarget, reclaimed, [](const std::wstring& msg, const std::wstring& lvl) {
            PrintConsole(L"  [" + lvl + L"] " + msg + L"\n");
        });
        PrintConsole(L"\n[✓] Deep clean complete. Reclaimed " + std::to_wstring(reclaimed / 1024) + L" KB whitespace.\n\n");
        return 0;
    }

    if (runAll) {
        PrintConsole(L"[*] Creating automated safety restore point...\n");
        std::wstring bPath;
        BackupEngine::CreateSnapshot(*pTarget, bPath);
        PrintConsole(L"[✓] Snapshot saved to: " + bPath + L"\n");

        PrintConsole(L"[*] Enforcing 10 hardening and debloat modules...\n");
        auto allTweaks = TweakEngine::GetAllTweaks();
        std::vector<int> ids;
        for (const auto& tw : allTweaks) ids.push_back(tw.id);

        bool ok = TweakEngine::ExecuteTweaks(
            *pTarget, ids,
            [](const std::wstring& msg, const std::wstring& lvl) {
                PrintConsole(L"  [" + lvl + L"] " + msg + L"\n");
            },
            [](int pct, const std::wstring& act) {}
        );

        PrintConsole(L"\n[✓] Optimization sweep completed with status: " + std::wstring(ok ? L"SUCCESS" : L"WARNINGS") + L"\n\n");
        return ok ? 0 : 1;
    }

    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 1. Check for CLI arguments first
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--all" || arg == L"-a" || arg == L"--audit" || arg == L"--clean" || arg == L"-c" || arg == L"--help" || arg == L"-h" || arg == L"/?") {
                // If modifying system, ensure elevation
                if ((arg == L"--all" || arg == L"-a" || arg == L"--clean" || arg == L"-c") && !IsProcessElevated()) {
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

    // 2. Ensure process is elevated as Administrator for GUI mode
    if (!IsProcessElevated()) {
        if (RelaunchAsAdmin(pCmdLine)) {
            return 0;
        } else {
            MessageBoxW(
                NULL,
                L"ChromeDebloater Pro requires Administrator privileges to configure Windows enterprise policies.\nPlease run as administrator.",
                L"Elevation Required",
                MB_ICONERROR | MB_OK
            );
            return 1;
        }
    }

    // 3. Launch Modern Dark GUI
    if (!AppWindow::InitializeAndShow(hInstance, nCmdShow)) {
        MessageBoxW(NULL, L"Failed to initialize modern user interface.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    // 4. Message Loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
