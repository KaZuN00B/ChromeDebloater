#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "browser.h"
#include "engine.h"

#define WM_APP_LOG      (WM_APP + 1)
#define WM_APP_PROGRESS (WM_APP + 2)
#define WM_APP_DONE     (WM_APP + 3)

class MainWindow {
public:
    static bool RegisterAndCreate(HINSTANCE hInstance, int nCmdShow);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd = nullptr;
    HWND m_hBrowserCombo = nullptr;
    HWND m_hBtnSelectAll = nullptr;
    HWND m_hBtnClearAll = nullptr;
    HWND m_hBtnRun = nullptr;
    HWND m_hLogEdit = nullptr;
    HWND m_hProgressBar = nullptr;
    HWND m_hStatusLabel = nullptr;
    std::vector<HWND> m_checkboxes;

    std::vector<BrowserTarget> m_browsers;
    std::vector<TweakOption> m_tweaks;
    int m_selectedBrowserIdx = 0;
    bool m_isRunning = false;

    // GDI Brushes & Fonts
    HBRUSH m_hBrBackground = nullptr;
    HBRUSH m_hBrPanel = nullptr;
    HBRUSH m_hBrLog = nullptr;
    HBRUSH m_hBrBadge = nullptr;
    HFONT m_hFontTitle = nullptr;
    HFONT m_hFontSubtitle = nullptr;
    HFONT m_hFontNormal = nullptr;
    HFONT m_hFontBold = nullptr;
    HFONT m_hFontLog = nullptr;

    void CreateUI(HWND hwnd);
    void PopulateBrowsers();
    void PopulateTweaks();
    void SelectAllTweaks(bool check);
    void AppendLog(const std::wstring& msg, bool isSuccess = true);
    void SetProgress(int percent);
    void StartExecution();
    void OnExecutionComplete(bool success);

    static DWORD WINAPI WorkerThread(LPVOID param);
};
