#include <windows.h>
#include <shellapi.h>
#include "ui.h"

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

bool RelaunchAsAdmin() {
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, MAX_PATH)) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;

        if (ShellExecuteExW(&sei)) {
            return true;
        }
    }
    return false;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 1. Ensure process is elevated as Administrator
    if (!IsProcessElevated()) {
        if (RelaunchAsAdmin()) {
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

    // 2. Initialize Common Controls and GUI
    if (!MainWindow::RegisterAndCreate(hInstance, nCmdShow)) {
        MessageBoxW(NULL, L"Failed to initialize ChromeDebloater user interface.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    // 3. Message Loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
