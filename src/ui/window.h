#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include "navigation.h"
#include "../engine/browser_target.h"
#include "../engine/audit_engine.h"
#include "../engine/tweak_engine.h"
#include "../engine/backup_engine.h"
#include "../engine/update_checker.h"
#include "../engine/network_shield.h"

#define WM_APP_ENGINE_LOG      (WM_APP + 10)
#define WM_APP_ENGINE_PROG     (WM_APP + 11)
#define WM_APP_ENGINE_DONE     (WM_APP + 12)
#define WM_APP_AUDIT_DONE      (WM_APP + 13)
#define WM_APP_UPDATES_DONE    (WM_APP + 14)
#define WM_APP_SHIELD_DONE     (WM_APP + 15)

struct LogMessage {
    std::wstring timestamp;
    std::wstring level; // "INFO", "SUCCESS", "ACTION", "WARNING"
    std::wstring text;
};

class AppWindow {
public:
    static bool InitializeAndShow(HINSTANCE hInstance, int nCmdShow);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd = nullptr;
    NavPage m_currentPage = NavPage::Dashboard;

    // GDI+ Resources
    ULONG_PTR m_gdiToken = 0;
    Gdiplus::Font* m_fTitle = nullptr;
    Gdiplus::Font* m_fSubtitle = nullptr;
    Gdiplus::Font* m_fNormal = nullptr;
    Gdiplus::Font* m_fBold = nullptr;
    Gdiplus::Font* m_fSmall = nullptr;
    Gdiplus::Font* m_fScore = nullptr;
    Gdiplus::Font* m_fConsole = nullptr;

    // State
    std::vector<BrowserTarget> m_browsers;
    int m_selectedBrowserIdx = 0;
    std::vector<TweakItem> m_tweaks;
    AuditReport m_auditReport;
    std::vector<BackupSnapshot> m_snapshots;
    std::vector<BrowserUpdateInfo> m_updateInfos;
    NetworkShieldStatus m_shieldStatus;
    std::vector<LogMessage> m_logs;
    int m_progressPercent = 0;
    std::wstring m_progressAction = L"Ready";
    bool m_isBusy = false;
    bool m_isCheckingUpdates = false;

    // Interaction & Layout
    POINT m_mousePos = { 0, 0 };
    int m_tweakScrollY = 0;

    void OnInit(HWND hwnd);
    void OnDestroy();
    void OnPaint(HDC hdc);
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnMouseWheel(short delta);

    // Render subroutines
    void RenderSidebar(Gdiplus::Graphics& g, const Gdiplus::RectF& rect);
    void RenderDashboard(Gdiplus::Graphics& g, const Gdiplus::RectF& rect);
    void RenderTweaks(Gdiplus::Graphics& g, const Gdiplus::RectF& rect);
    void RenderShield(Gdiplus::Graphics& g, const Gdiplus::RectF& rect);
    void RenderCleaner(Gdiplus::Graphics& g, const Gdiplus::RectF& rect);
    void RenderUpdates(Gdiplus::Graphics& g, const Gdiplus::RectF& rect);
    void RenderBackups(Gdiplus::Graphics& g, const Gdiplus::RectF& rect);
    void RenderLogs(Gdiplus::Graphics& g, const Gdiplus::RectF& rect);

    // Actions
    void RunAuditAsync();
    void RunCheckUpdatesAsync();
    void RunCheckShieldAsync();
    void RunToggleHostsBlockAsync(bool enable);
    void RunToggleFirewallBlockAsync(bool enable);
    void RunToggleAllShieldAsync(bool enable);
    void RunToggleUpdateLockAsync(int browserIdx, bool lock);
    void RunApplyTweaksAsync();
    void RunDeepCleanAsync();
    void RunCreateBackupAsync();
    void RunRestoreBackupAsync(const std::wstring& path);
    void RunResetPoliciesAsync();
    void AddLog(const std::wstring& text, const std::wstring& level);
};
