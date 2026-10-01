#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include <vector>
#include "ui.h"
#include "engine.h"
#include "browser.h"

// Linker directives for MSVC
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
    if (hStdOut && hStdOut != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteConsoleW(hStdOut, text.c_str(), (DWORD)text.length(), &written, NULL);
    }
}

int RunCliMode(int argc, wchar_t** argv) {
    AttachConsole(ATTACH_PARENT_PROCESS);

    PrintConsole(L"\n======================================================\n");
    PrintConsole(L"  ChromeDebloater Native CLI (Administrator Mode)\n");
    PrintConsole(L"======================================================\n\n");

    std::wstring targetKey = L"chrome";
    bool runAll = false;
    std::vector<int> selectedTweaks;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--all" || arg == L"-a" || arg == L"/all") {
            runAll = true;
        } else if ((arg == L"--browser" || arg == L"-b") && (i + 1 < argc)) {
            targetKey = argv[++i];
        } else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            PrintConsole(L"Usage: ChromeDebloater.exe [options]\n\n");
            PrintConsole(L"Options:\n");
            PrintConsole(L"  --all, -a                 Apply all hardening and optimization tweaks\n");
            PrintConsole(L"  --browser, -b <name>      Target browser: chrome (default), brave, or edge\n");
            PrintConsole(L"  --silent, -s              Run headlessly without GUI\n");
            PrintConsole(L"  --help, -h                Display this help screen\n\n");
            PrintConsole(L"If no options are specified, the full native dark GUI will launch.\n\n");
            return 0;
        }
    }

    auto browsers = DetectBrowsers();
    BrowserTarget* pTarget = nullptr;
    for (auto& b : browsers) {
        if (b.id == targetKey) {
            pTarget = &b;
            break;
        }
    }

    if (!pTarget) {
        PrintConsole(L"[X] Browser not recognized: " + targetKey + L"\n");
        return 1;
    }

    PrintConsole(L"[*] Selected Browser: " + pTarget->name + L"\n");
    auto tweaks = HardeningEngine::GetAvailableTweaks();
    for (const auto& tw : tweaks) {
        selectedTweaks.push_back(tw.id);
    }

    auto logCb = [](const std::wstring& msg, bool ok) {
        PrintConsole(L"  " + msg + L"\n");
    };

    auto progCb = [](int pct) {
        // progress callback for CLI
    };

    bool ok = HardeningEngine::RunTweaks(*pTarget, selectedTweaks, logCb, progCb);
    PrintConsole(L"\n[*] Completed with status: " + std::wstring(ok ? L"SUCCESS" : L"WARNINGS") + L"\n\n");
    return ok ? 0 : 1;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 1. Ensure process is elevated as Administrator
    if (!IsProcessElevated()) {
        if (RelaunchAsAdmin(pCmdLine)) {
            return 0; // Successfully spawned elevated child process
        } else {
            MessageBoxW(
                NULL,
                L"ChromeDebloater requires Administrator rights to apply system and enterprise policies.\nPlease run as administrator.",
                L"Administrator Access Required",
                MB_ICONERROR | MB_OK
            );
            return 1;
        }
    }

    // 2. Check for CLI arguments
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--all" || arg == L"-a" || arg == L"--silent" || arg == L"-s" || arg == L"--help" || arg == L"-h" || arg == L"/?") {
                int res = RunCliMode(argc, argv);
                LocalFree(argv);
                return res;
            }
        }
    }
    if (argv) LocalFree(argv);

    // 3. Initialize Common Controls and GUI
    if (!MainWindow::RegisterAndCreate(hInstance, nCmdShow)) {
        MessageBoxW(NULL, L"Failed to initialize ChromeDebloater user interface.", L"Error", MB_ICONERROR | MB_OK);
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
