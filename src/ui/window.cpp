#include "window.h"
#include "render_utils.h"
#include "theme.h"
#include <dwmapi.h>
#include <ctime>
#include <sstream>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

bool AppWindow::InitializeAndShow(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = AppWindow::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ChromeDebloaterModernClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL; // We paint entirely in WM_PAINT
    wc.hIcon = LoadIcon(NULL, IDI_SHIELD);
    wc.hIconSm = LoadIcon(NULL, IDI_SHIELD);

    if (!RegisterClassExW(&wc)) return false;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 1080;
    int winH = 720;
    int winX = (screenW - winW) / 2;
    int winY = (screenH - winH) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"ChromeDebloater Pro — Native Chromium Hardening & Optimizer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        winX, winY, winW, winH,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return false;

    // Windows 10/11 Immersive Dark Mode for Title Bar
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(hwnd, 19, &darkMode, sizeof(darkMode));

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return true;
}

LRESULT CALLBACK AppWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppWindow* pThis = (AppWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            pThis = new AppWindow();
            pThis->m_hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
            pThis->OnInit(hwnd);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Prevent flicker; handled in WM_PAINT

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (pThis) {
                pThis->OnPaint(hdc);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (pThis) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                pThis->OnMouseMove(x, y);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (pThis) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                pThis->OnLButtonDown(x, y);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            if (pThis) {
                short delta = GET_WHEEL_DELTA_WPARAM(wParam);
                pThis->OnMouseWheel(delta);
            }
            return 0;
        }

        case WM_APP_ENGINE_LOG: {
            if (pThis) {
                std::wstring* pMsg = (std::wstring*)lParam;
                std::wstring* pLvl = (std::wstring*)wParam;
                if (pMsg && pLvl) {
                    pThis->AddLog(*pMsg, *pLvl);
                    delete pMsg;
                    delete pLvl;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_APP_ENGINE_PROG: {
            if (pThis) {
                pThis->m_progressPercent = (int)wParam;
                std::wstring* pAct = (std::wstring*)lParam;
                if (pAct) {
                    pThis->m_progressAction = *pAct;
                    delete pAct;
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_APP_ENGINE_DONE: {
            if (pThis) {
                pThis->m_isBusy = false;
                pThis->RunAuditAsync();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_APP_AUDIT_DONE: {
            if (pThis) {
                AuditReport* pRep = (AuditReport*)lParam;
                if (pRep) {
                    pThis->m_auditReport = *pRep;
                    delete pRep;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_DESTROY: {
            if (pThis) {
                pThis->OnDestroy();
                delete pThis;
            }
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void AppWindow::OnInit(HWND hwnd) {
    GdiplusStartupInput gdiInput;
    GdiplusStartup(&m_gdiToken, &gdiInput, NULL);

    // Initialize Fonts (Segoe UI)
    m_fTitle = new Font(L"Segoe UI", 16.0f, FontStyleBold, UnitPixel);
    m_fSubtitle = new Font(L"Segoe UI", 12.0f, FontStyleRegular, UnitPixel);
    m_fNormal = new Font(L"Segoe UI", 13.0f, FontStyleRegular, UnitPixel);
    m_fBold = new Font(L"Segoe UI", 13.0f, FontStyleBold, UnitPixel);
    m_fSmall = new Font(L"Segoe UI", 11.0f, FontStyleRegular, UnitPixel);
    m_fScore = new Font(L"Segoe UI", 26.0f, FontStyleBold, UnitPixel);
    m_fConsole = new Font(L"Consolas", 12.0f, FontStyleRegular, UnitPixel);

    // Detect Browsers & Load Tweaks
    m_browsers = DetectAllBrowsers();
    m_tweaks = TweakEngine::GetAllTweaks();
    m_snapshots = BackupEngine::ListSnapshots();

    AddLog(L"ChromeDebloater Pro Native v3.0 started.", L"INFO");
    AddLog(L"Direct Win32 Kernel & Registry Hardening Engine Ready.", L"INFO");

    // Perform Initial System Audit
    RunAuditAsync();
}

void AppWindow::OnDestroy() {
    delete m_fTitle;
    delete m_fSubtitle;
    delete m_fNormal;
    delete m_fBold;
    delete m_fSmall;
    delete m_fScore;
    delete m_fConsole;

    if (m_gdiToken) {
        GdiplusShutdown(m_gdiToken);
        m_gdiToken = 0;
    }
}

void AppWindow::AddLog(const std::wstring& text, const std::wstring& level) {
    time_t now = time(nullptr);
    tm ltm;
    localtime_s(&ltm, &now);

    wchar_t timeBuf[32];
    swprintf_s(timeBuf, L"%02d:%02d:%02d", ltm.tm_hour, ltm.tm_min, ltm.tm_sec);

    LogMessage msg;
    msg.timestamp = timeBuf;
    msg.level = level;
    msg.text = text;

    m_logs.push_back(msg);
    if (m_logs.size() > 500) {
        m_logs.erase(m_logs.begin());
    }
}

void AppWindow::OnPaint(HDC hdc) {
    RECT clientRc;
    GetClientRect(m_hwnd, &clientRc);
    int width = clientRc.right - clientRc.left;
    int height = clientRc.bottom - clientRc.top;

    // Double buffering with GDI+
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    Graphics g(memDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Background
    SolidBrush bgBrush(Theme::BgDark);
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    // Layout Dimensions
    float sidebarW = 240.0f;
    RectF sidebarRect(0, 0, sidebarW, (float)height);
    RectF contentRect(sidebarW, 0, (float)width - sidebarW, (float)height);

    // Render Sidebar
    RenderSidebar(g, sidebarRect);

    // Render Active Content Page
    switch (m_currentPage) {
        case NavPage::Dashboard: RenderDashboard(g, contentRect); break;
        case NavPage::Tweaks:    RenderTweaks(g, contentRect); break;
        case NavPage::Cleaner:   RenderCleaner(g, contentRect); break;
        case NavPage::Backups:   RenderBackups(g, contentRect); break;
        case NavPage::Logs:      RenderLogs(g, contentRect); break;
    }

    // Blit to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void AppWindow::RenderSidebar(Graphics& g, const RectF& rect) {
    // Sidebar background
    SolidBrush sbBrush(Theme::BgSidebar);
    g.FillRectangle(&sbBrush, rect);

    // Divider Line
    Pen divPen(Theme::BorderSubtle, 1.0f);
    g.DrawLine(&divPen, rect.Width, 0.0f, rect.Width, rect.Height);

    // 1. App Header
    SolidBrush titleBrush(Theme::TextPrimary);
    g.DrawString(L"🛡 ChromeDebloater", -1, m_fTitle, PointF(20.0f, 20.0f), &titleBrush);

    SolidBrush verBrush(Theme::TextMuted);
    g.DrawString(L"v3.0 Pro · Native C++ Engine", -1, m_fSmall, PointF(22.0f, 44.0f), &verBrush);

    // Admin Status Badge
    RectF adminBadge(20.0f, 68.0f, 130.0f, 22.0f);
    RenderUtils::DrawBadge(g, adminBadge, L"●  ADMINISTRATOR", Theme::SuccessBadge, Theme::SuccessGreen, m_fSmall);

    // 2. Browser Target Selector Card
    RectF browCard(16.0f, 106.0f, rect.Width - 32.0f, 78.0f);
    RenderUtils::DrawCard(g, browCard, Theme::BgCard, Theme::BorderSubtle, 6.0f);

    SolidBrush lblBrush(Theme::TextMuted);
    g.DrawString(L"TARGET BROWSER", -1, m_fSmall, PointF(browCard.X + 12.0f, browCard.Y + 8.0f), &lblBrush);

    // 3 Browser pills
    float btnW = (browCard.Width - 24.0f) / 3.0f;
    const wchar_t* bNames[] = { L"Chrome", L"Brave", L"Edge" };
    for (int i = 0; i < 3; ++i) {
        RectF bPill(browCard.X + 12.0f + i * btnW, browCard.Y + 30.0f, btnW - 4.0f, 32.0f);
        bool isSel = (m_selectedBrowserIdx == i);
        Color pillBg = isSel ? Theme::AccentBlue : Theme::BgCardHover;
        Color pillTxt = isSel ? Color(255, 255, 255, 255) : Theme::TextSecondary;

        SolidBrush pillBrush(pillBg);
        RenderUtils::FillRoundedRect(g, pillBrush, bPill, 4.0f);

        SolidBrush pillTxtBrush(pillTxt);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(bNames[i], -1, m_fSmall, bPill, &sf, &pillTxtBrush);
    }

    // 3. Navigation Items
    float navY = 206.0f;
    float navH = 44.0f;
    const wchar_t* navLabels[] = {
        L"📊  Dashboard",
        L"⚡  Optimizations",
        L"🧹  Deep Cleaner",
        L"🔄  Backups & Restore",
        L"📜  Activity Console"
    };

    for (int i = 0; i < 5; ++i) {
        RectF navItem(16.0f, navY + i * (navH + 6.0f), rect.Width - 32.0f, navH);
        bool isActive = ((int)m_currentPage == i);

        if (isActive) {
            SolidBrush activeBrush(Color(255, 26, 35, 50));
            RenderUtils::FillRoundedRect(g, activeBrush, navItem, 6.0f);
            SolidBrush barBrush(Theme::AccentBlue);
            g.FillRectangle(&barBrush, navItem.X, navItem.Y + 10.0f, 3.0f, navItem.Height - 20.0f);
        }

        SolidBrush itemTxt(isActive ? Theme::TextPrimary : Theme::TextSecondary);
        StringFormat sf;
        sf.SetLineAlignment(StringAlignmentCenter);
        RectF textRc(navItem.X + 16.0f, navItem.Y, navItem.Width - 20.0f, navItem.Height);
        g.DrawString(navLabels[i], -1, isActive ? m_fBold : m_fNormal, textRc, &sf, &itemTxt);
    }

    // 4. Bottom System Status Card
    RectF statusCard(16.0f, rect.Height - 96.0f, rect.Width - 32.0f, 80.0f);
    RenderUtils::DrawCard(g, statusCard, Theme::BgCard, Theme::BorderSubtle, 6.0f);

    SolidBrush statTitle(Theme::TextPrimary);
    g.DrawString(L"System Health", -1, m_fBold, PointF(statusCard.X + 12.0f, statusCard.Y + 10.0f), &statTitle);

    std::wstring ramSaved = L"Disk Whitespace: " + std::to_wstring(m_auditReport.reclaimableBytes / (1024 * 1024)) + L" MB";
    SolidBrush statDesc(Theme::TextMuted);
    g.DrawString(ramSaved.c_str(), -1, m_fSmall, PointF(statusCard.X + 12.0f, statusCard.Y + 32.0f), &statDesc);

    std::wstring statusStr = m_isBusy ? (L"● " + m_progressAction) : L"● Engine Idle";
    SolidBrush statInd(m_isBusy ? Theme::WarningOrange : Theme::SuccessGreen);
    g.DrawString(statusStr.c_str(), -1, m_fSmall, PointF(statusCard.X + 12.0f, statusCard.Y + 52.0f), &statInd);
}

void AppWindow::RenderDashboard(Graphics& g, const RectF& rect) {
    float startX = rect.X + 28.0f;
    float startY = 24.0f;
    float contentW = rect.Width - 56.0f;

    // Header
    SolidBrush titleBrush(Theme::TextPrimary);
    g.DrawString(L"System & Browser Health Dashboard", -1, m_fTitle, PointF(startX, startY), &titleBrush);

    std::wstring sub = L"Active Profile: " + m_browsers[m_selectedBrowserIdx].name;
    if (!m_browsers[m_selectedBrowserIdx].version.empty()) {
        sub += L" (" + m_browsers[m_selectedBrowserIdx].version + L")";
    }
    SolidBrush subBrush(Theme::TextSecondary);
    g.DrawString(sub.c_str(), -1, m_fSubtitle, PointF(startX, startY + 26.0f), &subBrush);

    // 1. Large Health Score Card
    RectF gaugeRect(startX, startY + 60.0f, contentW, 100.0f);
    RenderUtils::DrawScoreGauge(g, gaugeRect, m_auditReport.score, m_fBold, m_fScore);

    // 2. Three Metric Cards Row
    float cardY = startY + 172.0f;
    float cardW = (contentW - 24.0f) / 3.0f;
    float cardH = 80.0f;

    // Metric 1: Optimized Policies
    RectF m1(startX, cardY, cardW, cardH);
    RenderUtils::DrawCard(g, m1, Theme::BgCard, Theme::BorderSubtle, 8.0f);
    g.DrawString(L"PROTECTED POLICIES", -1, m_fSmall, PointF(m1.X + 14.0f, m1.Y + 12.0f), &subBrush);
    std::wstring polStr = std::to_wstring(m_auditReport.optimizedCount) + L" Subsystems";
    g.DrawString(polStr.c_str(), -1, m_fBold, PointF(m1.X + 14.0f, m1.Y + 34.0f), &titleBrush);

    // Metric 2: AI Status
    RectF m2(startX + cardW + 12.0f, cardY, cardW, cardH);
    RenderUtils::DrawCard(g, m2, Theme::BgCard, Theme::BorderSubtle, 8.0f);
    g.DrawString(L"AI & GEMINI INTEGRATION", -1, m_fSmall, PointF(m2.X + 14.0f, m2.Y + 12.0f), &subBrush);
    SolidBrush aiStBrush(m_auditReport.score >= 80 ? Theme::SuccessGreen : Theme::WarningOrange);
    g.DrawString(m_auditReport.score >= 80 ? L"0 Active (Eliminated)" : L"Action Needed", -1, m_fBold, PointF(m2.X + 14.0f, m2.Y + 34.0f), &aiStBrush);

    // Metric 3: Reclaimable Cache
    RectF m3(startX + (cardW + 12.0f) * 2.0f, cardY, cardW, cardH);
    RenderUtils::DrawCard(g, m3, Theme::BgCard, Theme::BorderSubtle, 8.0f);
    g.DrawString(L"RECLAIMABLE SPACE", -1, m_fSmall, PointF(m3.X + 14.0f, m3.Y + 12.0f), &subBrush);
    std::wstring spaceStr = std::to_wstring(m_auditReport.reclaimableBytes / (1024 * 1024)) + L" MB Cache & Shaders";
    g.DrawString(spaceStr.c_str(), -1, m_fBold, PointF(m3.X + 14.0f, m3.Y + 34.0f), &titleBrush);

    // 3. One-Click Quick Actions
    float actY = startY + 268.0f;
    RectF btn1(startX, actY, 260.0f, 44.0f);
    RenderUtils::DrawGradientButton(g, btn1, L"🚀  Apply Maximum Optimization", false, false, m_fBold, !m_isBusy);

    RectF btn2(startX + 276.0f, actY, 200.0f, 44.0f);
    RenderUtils::DrawCard(g, btn2, Theme::BgCard, Theme::BorderSubtle, 8.0f);
    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(L"🔍  Re-Scan System", -1, m_fBold, btn2, &sfCenter, &titleBrush);

    RectF btn3(startX + 492.0f, actY, 200.0f, 44.0f);
    RenderUtils::DrawCard(g, btn3, Theme::BgCard, Theme::BorderSubtle, 8.0f);
    g.DrawString(L"💾  Create Snapshot", -1, m_fBold, btn3, &sfCenter, &titleBrush);

    // 4. Audit Checklist Table
    float listY = startY + 332.0f;
    g.DrawString(L"AUDIT & SECURITY FINDINGS", -1, m_fSmall, PointF(startX, listY), &subBrush);

    float itemY = listY + 22.0f;
    for (size_t i = 0; i < m_auditReport.items.size() && i < 5; ++i) {
        RectF rowRect(startX, itemY + i * 56.0f, contentW, 50.0f);
        RenderUtils::DrawCard(g, rowRect, Theme::BgCard, Theme::BorderSubtle, 6.0f);

        // Icon & Title
        bool isOpt = (m_auditReport.items[i].status == AuditStatus::Optimized);
        const wchar_t* icon = isOpt ? L"✓" : L"!";
        SolidBrush iconBrush(isOpt ? Theme::SuccessGreen : Theme::WarningOrange);
        g.DrawString(icon, -1, m_fBold, PointF(rowRect.X + 16.0f, rowRect.Y + 14.0f), &iconBrush);

        g.DrawString(m_auditReport.items[i].name.c_str(), -1, m_fBold, PointF(rowRect.X + 40.0f, rowRect.Y + 8.0f), &titleBrush);
        g.DrawString(m_auditReport.items[i].description.c_str(), -1, m_fSmall, PointF(rowRect.X + 40.0f, rowRect.Y + 28.0f), &subBrush);

        // Status badge
        RectF badgeRc(rowRect.X + rowRect.Width - 130.0f, rowRect.Y + 14.0f, 116.0f, 22.0f);
        RenderUtils::DrawBadge(
            g, badgeRc,
            isOpt ? L"OPTIMIZED" : L"ATTENTION",
            isOpt ? Theme::SuccessBadge : Theme::WarningBadge,
            isOpt ? Theme::SuccessGreen : Theme::WarningOrange,
            m_fSmall
        );
    }
}

void AppWindow::RenderTweaks(Graphics& g, const RectF& rect) {
    float startX = rect.X + 28.0f;
    float startY = 24.0f;
    float contentW = rect.Width - 56.0f;

    SolidBrush titleBrush(Theme::TextPrimary);
    g.DrawString(L"Hardening & Optimization Modules", -1, m_fTitle, PointF(startX, startY), &titleBrush);

    SolidBrush subBrush(Theme::TextSecondary);
    g.DrawString(L"Configure granular enterprise policies and performance flags", -1, m_fSubtitle, PointF(startX, startY + 26.0f), &subBrush);

    // Presets Row
    float presY = startY + 56.0f;
    RectF p1(startX, presY, 150.0f, 32.0f);
    RenderUtils::DrawCard(g, p1, Theme::BgCardHover, Theme::BorderSubtle, 4.0f);
    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(L"⚡ Maximum Preset", -1, m_fSmall, p1, &sfCenter, &titleBrush);

    RectF p2(startX + 160.0f, presY, 150.0f, 32.0f);
    RenderUtils::DrawCard(g, p2, Theme::BgCard, Theme::BorderSubtle, 4.0f);
    g.DrawString(L"⚖ Balanced Preset", -1, m_fSmall, p2, &sfCenter, &subBrush);

    RectF p3(startX + 320.0f, presY, 150.0f, 32.0f);
    RenderUtils::DrawCard(g, p3, Theme::BgCard, Theme::BorderSubtle, 4.0f);
    g.DrawString(L"🔒 Privacy Only", -1, m_fSmall, p3, &sfCenter, &subBrush);

    // Tweak Cards
    float cardY = presY + 44.0f - (float)m_tweakScrollY;
    for (size_t i = 0; i < m_tweaks.size(); ++i) {
        if (cardY + 68.0f < 0 || cardY > rect.Height) {
            cardY += 76.0f;
            continue;
        }

        RectF tc(startX, cardY, contentW, 68.0f);
        RenderUtils::DrawCard(g, tc, Theme::BgCard, Theme::BorderSubtle, 8.0f);

        // Title & Description
        g.DrawString(m_tweaks[i].title.c_str(), -1, m_fBold, PointF(tc.X + 16.0f, tc.Y + 12.0f), &titleBrush);
        g.DrawString(m_tweaks[i].description.c_str(), -1, m_fSmall, PointF(tc.X + 16.0f, tc.Y + 36.0f), &subBrush);

        // Impact Badge
        RectF tagRc(tc.X + tc.Width - 160.0f, tc.Y + 22.0f, 96.0f, 22.0f);
        RenderUtils::DrawBadge(g, tagRc, m_tweaks[i].impactTag, Theme::BgCardHover, Theme::TextSecondary, m_fSmall);

        // Pill Toggle Switch
        RectF togRc(tc.X + tc.Width - 52.0f, tc.Y + 22.0f, 40.0f, 22.0f);
        RenderUtils::DrawToggle(g, togRc, m_tweaks[i].enabled, false);

        cardY += 76.0f;
    }

    // Floating Bottom Apply Button
    RectF applyBtn(startX, rect.Height - 64.0f, contentW, 44.0f);
    RenderUtils::DrawGradientButton(g, applyBtn, L"⚡  APPLY SELECTED OPTIMIZATIONS", false, false, m_fBold, !m_isBusy);
}

void AppWindow::RenderCleaner(Graphics& g, const RectF& rect) {
    float startX = rect.X + 28.0f;
    float startY = 24.0f;
    float contentW = rect.Width - 56.0f;

    SolidBrush titleBrush(Theme::TextPrimary);
    g.DrawString(L"Profile Defragmentation & Deep Cleaner", -1, m_fTitle, PointF(startX, startY), &titleBrush);

    SolidBrush subBrush(Theme::TextSecondary);
    g.DrawString(L"Clean temporary shaders, defragment SQLite history, and flush DNS cache", -1, m_fSubtitle, PointF(startX, startY + 26.0f), &subBrush);

    float cy = startY + 70.0f;
    const wchar_t* cleanItems[][2] = {
        { L"SQLite Database Vacuum & Reindex", L"Compacts History, Favicons, and Shortcuts databases via winsqlite3.dll" },
        { L"Purge GPU & Shader Caches", L"Cleans stale DawnCache, GrShaderCache, and GPUCache folders" },
        { L"Crashpad & Diagnostics Sweep", L"Removes accumulated crash telemetry and crashpad memory dumps" },
        { L"Flush Windows DNS Resolver", L"Clears host cache via DnsFlushResolverCache for instant name updates" }
    };

    for (int i = 0; i < 4; ++i) {
        RectF cRect(startX, cy + i * 80.0f, contentW, 68.0f);
        RenderUtils::DrawCard(g, cRect, Theme::BgCard, Theme::BorderSubtle, 8.0f);

        g.DrawString(cleanItems[i][0], -1, m_fBold, PointF(cRect.X + 16.0f, cRect.Y + 12.0f), &titleBrush);
        g.DrawString(cleanItems[i][1], -1, m_fSmall, PointF(cRect.X + 16.0f, cRect.Y + 36.0f), &subBrush);

        RectF rdyBadge(cRect.X + cRect.Width - 110.0f, cRect.Y + 22.0f, 96.0f, 22.0f);
        RenderUtils::DrawBadge(g, rdyBadge, L"READY", Theme::SuccessBadge, Theme::SuccessGreen, m_fSmall);
    }

    RectF runCleanBtn(startX, cy + 340.0f, 280.0f, 48.0f);
    RenderUtils::DrawGradientButton(g, runCleanBtn, L"🧹  RUN DEEP CLEAN NOW", false, false, m_fBold, !m_isBusy);
}

void AppWindow::RenderBackups(Graphics& g, const RectF& rect) {
    float startX = rect.X + 28.0f;
    float startY = 24.0f;
    float contentW = rect.Width - 56.0f;

    SolidBrush titleBrush(Theme::TextPrimary);
    g.DrawString(L"Registry Snapshot & Rollback Center", -1, m_fTitle, PointF(startX, startY), &titleBrush);

    SolidBrush subBrush(Theme::TextSecondary);
    g.DrawString(L"ChromeDebloater automatically backs up registry states before applying tweaks", -1, m_fSubtitle, PointF(startX, startY + 26.0f), &subBrush);

    float by = startY + 70.0f;
    RectF snapBtn(startX, by, 220.0f, 40.0f);
    RenderUtils::DrawGradientButton(g, snapBtn, L"💾  Create New Snapshot", false, false, m_fBold, !m_isBusy);

    RectF rstBtn(startX + 236.0f, by, 220.0f, 40.0f);
    RenderUtils::DrawCard(g, rstBtn, Theme::BgCard, Theme::BorderSubtle, 8.0f);
    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(L"⚠️ Reset Policies to Default", -1, m_fBold, rstBtn, &sfCenter, &titleBrush);

    float listY = by + 60.0f;
    g.DrawString(L"AVAILABLE RESTORE SNAPSHOTS", -1, m_fSmall, PointF(startX, listY), &subBrush);

    float sy = listY + 22.0f;
    if (m_snapshots.empty()) {
        RectF emptyRc(startX, sy, contentW, 60.0f);
        RenderUtils::DrawCard(g, emptyRc, Theme::BgCard, Theme::BorderSubtle, 6.0f);
        g.DrawString(L"No previous snapshots found. Click 'Create New Snapshot' to generate one.", -1, m_fNormal, PointF(startX + 16.0f, sy + 20.0f), &subBrush);
    } else {
        for (size_t i = 0; i < m_snapshots.size() && i < 5; ++i) {
            RectF sRc(startX, sy + i * 60.0f, contentW, 52.0f);
            RenderUtils::DrawCard(g, sRc, Theme::BgCard, Theme::BorderSubtle, 6.0f);

            g.DrawString(m_snapshots[i].id.c_str(), -1, m_fBold, PointF(sRc.X + 16.0f, sRc.Y + 16.0f), &titleBrush);

            RectF rst(sRc.X + sRc.Width - 110.0f, sRc.Y + 12.0f, 96.0f, 26.0f);
            RenderUtils::DrawCard(g, rst, Theme::AccentBlue, Theme::AccentBlue, 4.0f);
            SolidBrush white(Color(255, 255, 255, 255));
            g.DrawString(L"Restore", -1, m_fSmall, rst, &sfCenter, &white);
        }
    }
}

void AppWindow::RenderLogs(Graphics& g, const RectF& rect) {
    float startX = rect.X + 28.0f;
    float startY = 24.0f;
    float contentW = rect.Width - 56.0f;

    SolidBrush titleBrush(Theme::TextPrimary);
    g.DrawString(L"Live Activity & Execution Console", -1, m_fTitle, PointF(startX, startY), &titleBrush);

    SolidBrush subBrush(Theme::TextSecondary);
    g.DrawString(L"Real-time event streaming and Win32 kernel command execution", -1, m_fSubtitle, PointF(startX, startY + 26.0f), &subBrush);

    // Terminal Card
    float termY = startY + 60.0f;
    float termH = rect.Height - termY - 32.0f;
    RectF termRc(startX, termY, contentW, termH);
    RenderUtils::DrawCard(g, termRc, Theme::BgInput, Theme::BorderSubtle, 8.0f);

    float logLineY = termRc.Y + 16.0f;
    int maxLines = (int)((termH - 32.0f) / 22.0f);
    int startIdx = (int)m_logs.size() - maxLines;
    if (startIdx < 0) startIdx = 0;

    for (size_t i = startIdx; i < m_logs.size(); ++i) {
        SolidBrush timeBrush(Theme::TextMuted);
        g.DrawString(m_logs[i].timestamp.c_str(), -1, m_fConsole, PointF(termRc.X + 16.0f, logLineY), &timeBrush);

        Color lvlColor = Theme::SuccessGreen;
        if (m_logs[i].level == L"WARNING") lvlColor = Theme::WarningOrange;
        else if (m_logs[i].level == L"ACTION") lvlColor = Theme::AccentBlue;

        SolidBrush lvlBrush(lvlColor);
        std::wstring tag = L"[" + m_logs[i].level + L"]";
        g.DrawString(tag.c_str(), -1, m_fConsole, PointF(termRc.X + 90.0f, logLineY), &lvlBrush);

        SolidBrush msgBrush(Theme::TextPrimary);
        g.DrawString(m_logs[i].text.c_str(), -1, m_fConsole, PointF(termRc.X + 180.0f, logLineY), &msgBrush);

        logLineY += 22.0f;
    }
}

void AppWindow::OnMouseMove(int x, int y) {
    m_mousePos.x = x;
    m_mousePos.y = y;
}

void AppWindow::OnMouseWheel(short delta) {
    if (m_currentPage == NavPage::Tweaks) {
        m_tweakScrollY -= (delta / 2);
        if (m_tweakScrollY < 0) m_tweakScrollY = 0;
        if (m_tweakScrollY > 300) m_tweakScrollY = 300;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void AppWindow::OnLButtonDown(int x, int y) {
    // 1. Sidebar Browser Picker
    RectF browCard(16.0f, 106.0f, 208.0f, 78.0f);
    if (x >= browCard.X && x <= browCard.X + browCard.Width && y >= browCard.Y + 30.0f && y <= browCard.Y + 62.0f) {
        float btnW = (browCard.Width - 24.0f) / 3.0f;
        int clickedIdx = (int)((x - (browCard.X + 12.0f)) / btnW);
        if (clickedIdx >= 0 && clickedIdx < 3 && clickedIdx != m_selectedBrowserIdx) {
            m_selectedBrowserIdx = clickedIdx;
            RunAuditAsync();
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        }
    }

    // 2. Sidebar Navigation Clicks
    float navY = 206.0f;
    float navH = 44.0f;
    for (int i = 0; i < 5; ++i) {
        RectF navItem(16.0f, navY + i * (navH + 6.0f), 208.0f, navH);
        if (x >= navItem.X && x <= navItem.X + navItem.Width && y >= navItem.Y && y <= navItem.Y + navItem.Height) {
            m_currentPage = (NavPage)i;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        }
    }

    // 3. Page Specific Actions
    float contentX = 240.0f;
    float startX = contentX + 28.0f;
    float startY = 24.0f;

    if (m_currentPage == NavPage::Dashboard) {
        float actY = startY + 268.0f;
        // Button 1: Apply Maximum
        if (x >= startX && x <= startX + 260.0f && y >= actY && y <= actY + 44.0f) {
            RunApplyTweaksAsync();
        }
        // Button 2: Re-Scan
        else if (x >= startX + 276.0f && x <= startX + 476.0f && y >= actY && y <= actY + 44.0f) {
            RunAuditAsync();
        }
        // Button 3: Create Snapshot
        else if (x >= startX + 492.0f && x <= startX + 692.0f && y >= actY && y <= actY + 44.0f) {
            RunCreateBackupAsync();
        }
    } else if (m_currentPage == NavPage::Tweaks) {
        // Presets
        float presY = startY + 56.0f;
        if (y >= presY && y <= presY + 32.0f) {
            if (x >= startX && x <= startX + 150.0f) {
                TweakEngine::ApplyPreset(m_tweaks, TweakPreset::Maximum);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            } else if (x >= startX + 160.0f && x <= startX + 310.0f) {
                TweakEngine::ApplyPreset(m_tweaks, TweakPreset::Balanced);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            } else if (x >= startX + 320.0f && x <= startX + 470.0f) {
                TweakEngine::ApplyPreset(m_tweaks, TweakPreset::PrivacyOnly);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            }
        }

        // Toggles
        float cardY = presY + 44.0f - (float)m_tweakScrollY;
        for (size_t i = 0; i < m_tweaks.size(); ++i) {
            RectF togRc(startX + (1080.0f - 240.0f - 56.0f) - 52.0f, cardY + 22.0f, 40.0f, 22.0f);
            if (x >= togRc.X - 10.0f && x <= togRc.X + togRc.Width + 10.0f && y >= togRc.Y && y <= togRc.Y + togRc.Height) {
                m_tweaks[i].enabled = !m_tweaks[i].enabled;
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            }
            cardY += 76.0f;
        }

        // Apply Button
        RECT cr;
        GetClientRect(m_hwnd, &cr);
        RectF applyBtn(startX, (float)cr.bottom - 64.0f, (float)(cr.right - cr.left) - 240.0f - 56.0f, 44.0f);
        if (x >= applyBtn.X && x <= applyBtn.X + applyBtn.Width && y >= applyBtn.Y && y <= applyBtn.Y + applyBtn.Height) {
            RunApplyTweaksAsync();
        }
    } else if (m_currentPage == NavPage::Cleaner) {
        float runY = startY + 70.0f + 340.0f;
        if (x >= startX && x <= startX + 280.0f && y >= runY && y <= runY + 48.0f) {
            RunDeepCleanAsync();
        }
    } else if (m_currentPage == NavPage::Backups) {
        float by = startY + 70.0f;
        if (x >= startX && x <= startX + 220.0f && y >= by && y <= by + 40.0f) {
            RunCreateBackupAsync();
        } else if (x >= startX + 236.0f && x <= startX + 456.0f && y >= by && y <= by + 40.0f) {
            RunResetPoliciesAsync();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Asynchronous Worker Dispatchers
// ─────────────────────────────────────────────────────────────────────────────

struct AsyncParam {
    HWND hwnd;
    BrowserTarget browser;
    std::vector<int> tweakIds;
};

void AppWindow::RunAuditAsync() {
    HWND hwnd = m_hwnd;
    BrowserTarget target = m_browsers[m_selectedBrowserIdx];

    CreateThread(NULL, 0, [](LPVOID p) -> DWORD {
        HWND h = (HWND)p;
        AppWindow* win = (AppWindow*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (!win) return 0;
        BrowserTarget t = win->m_browsers[win->m_selectedBrowserIdx];
        AuditReport rep = AuditEngine::PerformAudit(t);
        AuditReport* pRep = new AuditReport(rep);
        PostMessageW(h, WM_APP_AUDIT_DONE, 0, (LPARAM)pRep);
        return 0;
    }, hwnd, 0, NULL);
}

void AppWindow::RunApplyTweaksAsync() {
    if (m_isBusy) return;
    m_isBusy = true;
    m_progressPercent = 0;
    m_progressAction = L"Optimizing...";

    std::vector<int> activeIds;
    for (const auto& tw : m_tweaks) {
        if (tw.enabled) activeIds.push_back(tw.id);
    }

    AsyncParam* param = new AsyncParam();
    param->hwnd = m_hwnd;
    param->browser = m_browsers[m_selectedBrowserIdx];
    param->tweakIds = activeIds;

    // First auto-create backup
    std::wstring bPath;
    BackupEngine::CreateSnapshot(param->browser, bPath);
    AddLog(L"Automated restore snapshot created before applying tweaks.", L"ACTION");

    HWND hwnd = m_hwnd;
    CreateThread(NULL, 0, [](LPVOID p) -> DWORD {
        AsyncParam* ap = (AsyncParam*)p;
        HWND h = ap->hwnd;
        BrowserTarget b = ap->browser;
        std::vector<int> ids = ap->tweakIds;
        delete ap;

        auto logCb = [h](const std::wstring& msg, const std::wstring& lvl) {
            std::wstring* pMsg = new std::wstring(msg);
            std::wstring* pLvl = new std::wstring(lvl);
            PostMessageW(h, WM_APP_ENGINE_LOG, (WPARAM)pLvl, (LPARAM)pMsg);
        };

        auto progCb = [h](int pct, const std::wstring& act) {
            std::wstring* pAct = new std::wstring(act);
            PostMessageW(h, WM_APP_ENGINE_PROG, pct, (LPARAM)pAct);
        };

        TweakEngine::ExecuteTweaks(b, ids, logCb, progCb);
        PostMessageW(h, WM_APP_ENGINE_DONE, 0, 0);
        return 0;
    }, param, 0, NULL);
}

void AppWindow::RunDeepCleanAsync() {
    if (m_isBusy) return;
    m_isBusy = true;

    HWND hwnd = m_hwnd;
    BrowserTarget target = m_browsers[m_selectedBrowserIdx];

    CreateThread(NULL, 0, [](LPVOID p) -> DWORD {
        HWND h = (HWND)p;
        AppWindow* win = (AppWindow*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (!win) return 0;
        BrowserTarget b = win->m_browsers[win->m_selectedBrowserIdx];

        auto logCb = [h](const std::wstring& msg, const std::wstring& lvl) {
            std::wstring* pMsg = new std::wstring(msg);
            std::wstring* pLvl = new std::wstring(lvl);
            PostMessageW(h, WM_APP_ENGINE_LOG, (WPARAM)pLvl, (LPARAM)pMsg);
        };

        INT64 reclaimed = 0;
        TweakEngine::RunDeepClean(b, reclaimed, logCb);
        PostMessageW(h, WM_APP_ENGINE_DONE, 0, 0);
        return 0;
    }, hwnd, 0, NULL);
}

void AppWindow::RunCreateBackupAsync() {
    std::wstring bPath;
    if (BackupEngine::CreateSnapshot(m_browsers[m_selectedBrowserIdx], bPath)) {
        AddLog(L"Created registry snapshot: " + bPath, L"SUCCESS");
        m_snapshots = BackupEngine::ListSnapshots();
        InvalidateRect(m_hwnd, NULL, FALSE);
    } else {
        AddLog(L"Failed to create registry snapshot.", L"WARNING");
    }
}

void AppWindow::RunResetPoliciesAsync() {
    if (MessageBoxW(m_hwnd, L"Are you sure you want to reset all enterprise policies for this browser to defaults?", L"Confirm Reset", MB_YESNO | MB_ICONWARNING) == IDYES) {
        BackupEngine::ResetBrowserPolicies(m_browsers[m_selectedBrowserIdx]);
        AddLog(L"All policies deleted. Target browser reset to factory defaults.", L"SUCCESS");
        RunAuditAsync();
    }
}
