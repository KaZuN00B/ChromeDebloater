#include "ui.h"
#include <dwmapi.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <iostream>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")

#define ID_BROWSER_COMBO    101
#define ID_BTN_SELECT_ALL   102
#define ID_BTN_CLEAR_ALL    103
#define ID_BTN_RUN          104
#define ID_LOG_EDIT         105
#define ID_PROGRESS_BAR     106
#define ID_CHECKBOX_BASE    200

struct WorkerParam {
    HWND hwnd;
    BrowserTarget browser;
    std::vector<int> selectedTweaks;
};

bool MainWindow::RegisterAndCreate(HINSTANCE hInstance, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ChromeDebloaterMainWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(15, 17, 23)); // #0F1117
    wc.hIcon = LoadIcon(NULL, IDI_SHIELD);
    wc.hIconSm = LoadIcon(NULL, IDI_SHIELD);

    if (!RegisterClassExW(&wc)) return false;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 980;
    int winH = 700;
    int winX = (screenW - winW) / 2;
    int winY = (screenH - winH) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"ChromeDebloater v2.0 - Native Chromium Hardener & Optimizer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        winX, winY, winW, winH,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return false;

    // Apply Windows 10/11 Immersive Dark Mode to title bar
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(hwnd, 19 /* DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 */, &darkMode, sizeof(darkMode));

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return true;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = (MainWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            pThis = new MainWindow();
            pThis->m_hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
            pThis->CreateUI(hwnd);
            return 0;
        }

        case WM_COMMAND: {
            if (!pThis) break;
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == ID_BROWSER_COMBO && wmEvent == CBN_SELCHANGE) {
                pThis->m_selectedBrowserIdx = (int)SendMessageW(pThis->m_hBrowserCombo, CB_GETCURSEL, 0, 0);
            } else if (wmId == ID_BTN_SELECT_ALL) {
                pThis->SelectAllTweaks(true);
            } else if (wmId == ID_BTN_CLEAR_ALL) {
                pThis->SelectAllTweaks(false);
            } else if (wmId == ID_BTN_RUN) {
                pThis->StartExecution();
            }
            return 0;
        }

        case WM_APP_LOG: {
            if (!pThis) break;
            std::wstring* pMsg = (std::wstring*)lParam;
            bool ok = (wParam != 0);
            if (pMsg) {
                pThis->AppendLog(*pMsg, ok);
                delete pMsg;
            }
            return 0;
        }

        case WM_APP_PROGRESS: {
            if (!pThis) break;
            pThis->SetProgress((int)wParam);
            return 0;
        }

        case WM_APP_DONE: {
            if (!pThis) break;
            bool success = (wParam != 0);
            pThis->OnExecutionComplete(success);
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            HWND hwndStatic = (HWND)lParam;
            SetTextColor(hdcStatic, RGB(226, 232, 240));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)pThis->m_hBrBackground;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, RGB(74, 222, 128)); // Emerald green log text
            SetBkColor(hdcEdit, RGB(10, 13, 20));     // Deep black/navy
            return (LRESULT)pThis->m_hBrLog;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Draw header background panel
            RECT headerRc = { 0, 0, 980, 75 };
            HBRUSH hBrHeader = CreateSolidBrush(RGB(10, 13, 20));
            FillRect(hdc, &headerRc, hBrHeader);
            DeleteObject(hBrHeader);

            // Draw subtle divider line below header
            HPEN hPenLine = CreatePen(PS_SOLID, 1, RGB(30, 41, 59));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPenLine);
            MoveToEx(hdc, 0, 75, NULL);
            LineTo(hdc, 980, 75);

            // Draw divider line between left and right panels
            MoveToEx(hdc, 490, 75, NULL);
            LineTo(hdc, 490, 700);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPenLine);

            // Header Title
            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, pThis->m_hFontTitle);
            SetTextColor(hdc, RGB(241, 245, 249));
            TextOutW(hdc, 24, 14, L"🛡 ChromeDebloater", 18);

            // Header Subtitle
            SelectObject(hdc, pThis->m_hFontSubtitle);
            SetTextColor(hdc, RGB(148, 163, 184));
            TextOutW(hdc, 26, 46, L"High-performance native browser hardening, privacy, and debloating engine", 73);

            // Admin Badge
            RECT badgeRc = { 830, 22, 946, 52 };
            HBRUSH hBrBadge = CreateSolidBrush(RGB(20, 83, 45));
            FillRect(hdc, &badgeRc, hBrBadge);
            DeleteObject(hBrBadge);

            FrameRect(hdc, &badgeRc, (HBRUSH)GetStockObject(BLACK_BRUSH));
            SelectObject(hdc, pThis->m_hFontBold);
            SetTextColor(hdc, RGB(74, 222, 128));
            DrawTextW(hdc, L"⚡ ADMIN", -1, &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Section Headers
            SelectObject(hdc, pThis->m_hFontBold);
            SetTextColor(hdc, RGB(96, 165, 250)); // Accent blue
            TextOutW(hdc, 24, 90, L"TARGET BROWSER", 14);
            TextOutW(hdc, 24, 155, L"HARDENING & OPTIMIZATION TWEAKS", 32);
            TextOutW(hdc, 510, 90, L"LIVE ACTIVITY & EXECUTION LOG", 29);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY: {
            if (pThis) {
                delete pThis;
            }
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void MainWindow::CreateUI(HWND hwnd) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

    // GDI Brushes
    m_hBrBackground = CreateSolidBrush(RGB(15, 17, 23));
    m_hBrPanel = CreateSolidBrush(RGB(22, 27, 38));
    m_hBrLog = CreateSolidBrush(RGB(10, 13, 20));
    m_hBrBadge = CreateSolidBrush(RGB(20, 83, 45));

    // Fonts
    m_hFontTitle = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontSubtitle = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontNormal = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontBold = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontLog = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    // 1. Target Browser ComboBox
    m_hBrowserCombo = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
        24, 114, 440, 240,
        hwnd, (HMENU)ID_BROWSER_COMBO, hInst, NULL
    );
    SendMessageW(m_hBrowserCombo, WM_SETFONT, (WPARAM)m_hFontNormal, TRUE);
    SetWindowTheme(m_hBrowserCombo, L"DarkMode_Explorer", NULL);

    // 2. "Select All" and "Clear All" buttons
    m_hBtnSelectAll = CreateWindowExW(
        0, L"BUTTON", L"Select All",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        310, 150, 75, 24,
        hwnd, (HMENU)ID_BTN_SELECT_ALL, hInst, NULL
    );
    SendMessageW(m_hBtnSelectAll, WM_SETFONT, (WPARAM)m_hFontNormal, TRUE);
    SetWindowTheme(m_hBtnSelectAll, L"DarkMode_Explorer", NULL);

    m_hBtnClearAll = CreateWindowExW(
        0, L"BUTTON", L"Clear",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        392, 150, 72, 24,
        hwnd, (HMENU)ID_BTN_CLEAR_ALL, hInst, NULL
    );
    SendMessageW(m_hBtnClearAll, WM_SETFONT, (WPARAM)m_hFontNormal, TRUE);
    SetWindowTheme(m_hBtnClearAll, L"DarkMode_Explorer", NULL);

    // 3. Tweak Checkboxes
    PopulateBrowsers();
    PopulateTweaks();

    // 4. "⚡ RUN OPTIMIZATIONS" Button
    m_hBtnRun = CreateWindowExW(
        0, L"BUTTON", L"⚡  RUN OPTIMIZATIONS",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        24, 595, 440, 48,
        hwnd, (HMENU)ID_BTN_RUN, hInst, NULL
    );
    SendMessageW(m_hBtnRun, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
    SetWindowTheme(m_hBtnRun, L"DarkMode_Explorer", NULL);

    // 5. Output Log Edit Control
    m_hLogEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        510, 114, 436, 470,
        hwnd, (HMENU)ID_LOG_EDIT, hInst, NULL
    );
    SendMessageW(m_hLogEdit, WM_SETFONT, (WPARAM)m_hFontLog, TRUE);
    SetWindowTheme(m_hLogEdit, L"DarkMode_Explorer", NULL);

    // 6. Progress Bar
    m_hProgressBar = CreateWindowExW(
        0, PROGRESS_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        510, 595, 436, 16,
        hwnd, (HMENU)ID_PROGRESS_BAR, hInst, NULL
    );
    SendMessageW(m_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(m_hProgressBar, PBM_SETPOS, 0, 0);

    // 7. Status Label
    m_hStatusLabel = CreateWindowExW(
        0, L"STATIC", L"Ready. Choose your target browser and tweaks, then click Run.",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        510, 620, 436, 24,
        hwnd, NULL, hInst, NULL
    );
    SendMessageW(m_hStatusLabel, WM_SETFONT, (WPARAM)m_hFontSubtitle, TRUE);

    AppendLog(L"============================================================", true);
    AppendLog(L"  ChromeDebloater Native v2.0 Initialized", true);
    AppendLog(L"  Mode: Elevated Administrator", true);
    AppendLog(L"  Direct Win32 Kernel & Registry Hardening Engine Ready", true);
    AppendLog(L"============================================================\n", true);
}

void MainWindow::PopulateBrowsers() {
    m_browsers = DetectBrowsers();
    SendMessageW(m_hBrowserCombo, CB_RESETCONTENT, 0, 0);

    int selectIdx = 0;
    for (size_t i = 0; i < m_browsers.size(); ++i) {
        std::wstring text = m_browsers[i].name;
        if (m_browsers[i].isInstalled) {
            text += L"  [Installed ✓]";
        } else {
            text += L"  [Not Detected]";
        }
        SendMessageW(m_hBrowserCombo, CB_ADDSTRING, 0, (LPARAM)text.c_str());

        // Default to Google Chrome if installed
        if (m_browsers[i].id == L"chrome" && m_browsers[i].isInstalled) {
            selectIdx = (int)i;
        }
    }
    SendMessageW(m_hBrowserCombo, CB_SETCURSEL, selectIdx, 0);
    m_selectedBrowserIdx = selectIdx;
}

void MainWindow::PopulateTweaks() {
    m_tweaks = HardeningEngine::GetAvailableTweaks();
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(m_hwnd, GWLP_HINSTANCE);

    int startY = 185;
    int spacingY = 38;

    for (size_t i = 0; i < m_tweaks.size(); ++i) {
        int y = startY + (int)i * spacingY;
        std::wstring label = L"[" + m_tweaks[i].category + L"] " + m_tweaks[i].label;

        HWND hCheck = CreateWindowExW(
            0, L"BUTTON", label.c_str(),
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            24, y, 440, 26,
            m_hwnd, (HMENU)(INT_PTR)(ID_CHECKBOX_BASE + i), hInst, NULL
        );
        SendMessageW(hCheck, WM_SETFONT, (WPARAM)m_hFontNormal, TRUE);
        SendMessageW(hCheck, BM_SETCHECK, BST_CHECKED, 0);
        SetWindowTheme(hCheck, L"DarkMode_Explorer", NULL);

        m_checkboxes.push_back(hCheck);
    }
}

void MainWindow::SelectAllTweaks(bool check) {
    WPARAM state = check ? BST_CHECKED : BST_UNCHECKED;
    for (HWND hCheck : m_checkboxes) {
        SendMessageW(hCheck, BM_SETCHECK, state, 0);
    }
}

void MainWindow::AppendLog(const std::wstring& msg, bool isSuccess) {
    int len = GetWindowTextLengthW(m_hLogEdit);
    SendMessageW(m_hLogEdit, EM_SETSEL, len, len);
    std::wstring line = msg + L"\r\n";
    SendMessageW(m_hLogEdit, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
}

void MainWindow::SetProgress(int percent) {
    SendMessageW(m_hProgressBar, PBM_SETPOS, percent, 0);
}

void MainWindow::StartExecution() {
    if (m_isRunning) return;

    if (m_selectedBrowserIdx < 0 || m_selectedBrowserIdx >= (int)m_browsers.size()) {
        AppendLog(L"[!] Invalid browser selection.", false);
        return;
    }

    std::vector<int> selectedTweaks;
    for (size_t i = 0; i < m_checkboxes.size(); ++i) {
        if (SendMessageW(m_checkboxes[i], BM_GETCHECK, 0, 0) == BST_CHECKED) {
            selectedTweaks.push_back(m_tweaks[i].id);
        }
    }

    if (selectedTweaks.empty()) {
        AppendLog(L"[!] Please select at least one tweak to apply.", false);
        return;
    }

    m_isRunning = true;
    EnableWindow(m_hBtnRun, FALSE);
    EnableWindow(m_hBtnSelectAll, FALSE);
    EnableWindow(m_hBtnClearAll, FALSE);
    EnableWindow(m_hBrowserCombo, FALSE);
    SetProgress(0);
    SetWindowTextW(m_hStatusLabel, L"Applying optimizations in background thread...");

    WorkerParam* pParam = new WorkerParam();
    pParam->hwnd = m_hwnd;
    pParam->browser = m_browsers[m_selectedBrowserIdx];
    pParam->selectedTweaks = selectedTweaks;

    CreateThread(NULL, 0, WorkerThread, pParam, 0, NULL);
}

void MainWindow::OnExecutionComplete(bool success) {
    m_isRunning = false;
    EnableWindow(m_hBtnRun, TRUE);
    EnableWindow(m_hBtnSelectAll, TRUE);
    EnableWindow(m_hBtnClearAll, TRUE);
    EnableWindow(m_hBrowserCombo, TRUE);
    SetProgress(100);

    if (success) {
        SetWindowTextW(m_hStatusLabel, L"Done! All optimizations applied successfully.");
    } else {
        SetWindowTextW(m_hStatusLabel, L"Completed with one or more warnings.");
    }
}

DWORD WINAPI MainWindow::WorkerThread(LPVOID param) {
    WorkerParam* p = (WorkerParam*)param;
    HWND hwnd = p->hwnd;
    BrowserTarget browser = p->browser;
    std::vector<int> tweaks = p->selectedTweaks;
    delete p;

    auto logCb = [hwnd](const std::wstring& msg, bool ok) {
        std::wstring* pMsg = new std::wstring(msg);
        PostMessageW(hwnd, WM_APP_LOG, ok ? 1 : 0, (LPARAM)pMsg);
    };

    auto progCb = [hwnd](int pct) {
        PostMessageW(hwnd, WM_APP_PROGRESS, pct, 0);
    };

    bool ok = HardeningEngine::RunTweaks(browser, tweaks, logCb, progCb);
    PostMessageW(hwnd, WM_APP_DONE, ok ? 1 : 0, 0);
    return 0;
}
