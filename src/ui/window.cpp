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
    wc.hbrBackground = NULL;
    wc.hIcon = LoadIcon(NULL, IDI_SHIELD);
    wc.hIconSm = LoadIcon(NULL, IDI_SHIELD);

    if (!RegisterClassExW(&wc)) return false;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 1260;
    int winH = 820;
    int winX = (screenW - winW) / 2;
    int winY = (screenH - winH) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"ChromeDebloater Pro",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        winX, winY, winW, winH,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return false;

    // Dark Mode Title Bar
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
            return 1;

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
                pThis->OnMouseMove(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (pThis) {
                pThis->OnLButtonDown(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            if (pThis) {
                pThis->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
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
                pThis->RunCheckUpdatesAsync();
                pThis->RunCheckShieldAsync();
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

        case WM_APP_UPDATES_DONE: {
            if (pThis) {
                std::vector<BrowserUpdateInfo>* pInfos = (std::vector<BrowserUpdateInfo>*)lParam;
                if (pInfos) {
                    pThis->m_updateInfos = *pInfos;
                    delete pInfos;
                    pThis->m_isCheckingUpdates = false;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_APP_SHIELD_DONE: {
            if (pThis) {
                NetworkShieldStatus* pSt = (NetworkShieldStatus*)lParam;
                if (pSt) {
                    pThis->m_shieldStatus = *pSt;
                    delete pSt;
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

    m_fMainTitle = new Font(L"Segoe UI", 26.0f, FontStyleBold, UnitPixel);
    m_fTitle = new Font(L"Segoe UI", 16.0f, FontStyleBold, UnitPixel);
    m_fSubtitle = new Font(L"Segoe UI", 12.0f, FontStyleRegular, UnitPixel);
    m_fNormal = new Font(L"Segoe UI", 13.0f, FontStyleRegular, UnitPixel);
    m_fBold = new Font(L"Segoe UI", 13.0f, FontStyleBold, UnitPixel);
    m_fSmall = new Font(L"Segoe UI", 11.0f, FontStyleRegular, UnitPixel);
    m_fScore = new Font(L"Segoe UI", 32.0f, FontStyleBold, UnitPixel);
    m_fHuge = new Font(L"Segoe UI", 28.0f, FontStyleBold, UnitPixel);
    m_fConsole = new Font(L"Consolas", 12.0f, FontStyleRegular, UnitPixel);

    m_browsers = DetectAllBrowsers();
    m_tweaks = TweakEngine::GetAllTweaks();
    m_snapshots = BackupEngine::ListSnapshots();

    AddLog(L"ChromeDebloater Pro Native v3.5 started.", L"INFO");
    AddLog(L"Universal Chromium Compatibility & Windows Network Shield Initialized.", L"INFO");

    RunAuditAsync();
    RunCheckUpdatesAsync();
    RunCheckShieldAsync();
}

void AppWindow::OnDestroy() {
    delete m_fMainTitle;
    delete m_fTitle;
    delete m_fSubtitle;
    delete m_fNormal;
    delete m_fBold;
    delete m_fSmall;
    delete m_fScore;
    delete m_fHuge;
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

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    Graphics g(memDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    SolidBrush bgBrush(Theme::BgDark);
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    float sidebarW = 240.0f;
    RectF sidebarRect(0, 0, sidebarW, (float)height);
    RectF contentRect(sidebarW, 0, (float)width - sidebarW, (float)height);

    RenderSidebar(g, sidebarRect);

    switch (m_currentPage) {
        case NavPage::Dashboard: RenderDashboard(g, contentRect); break;
        case NavPage::Tweaks:    RenderTweaks(g, contentRect); break;
        case NavPage::Shield:    RenderShield(g, contentRect); break;
        case NavPage::Cleaner:   RenderCleaner(g, contentRect); break;
        case NavPage::Updates:   RenderUpdates(g, contentRect); break;
        case NavPage::Backups:   RenderBackups(g, contentRect); break;
        case NavPage::Logs:      RenderLogs(g, contentRect); break;
    }

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void AppWindow::RenderSidebar(Graphics& g, const RectF& rect) {
    SolidBrush sbBrush(Theme::BgSidebar);
    g.FillRectangle(&sbBrush, rect);

    Pen divPen(Theme::BorderSubtle, 1.0f);
    g.DrawLine(&divPen, rect.Width, 0.0f, rect.Width, rect.Height);

    // Header matching ui.png: "Shield" title + version + green pill badge
    SolidBrush titleBrush(Theme::TextPrimary);
    g.DrawString(L"🛡 Shield", -1, m_fTitle, PointF(22.0f, 20.0f), &titleBrush);

    SolidBrush verBrush(Theme::TextMuted);
    g.DrawString(L"ChromeDebloater v3.5 Pro", -1, m_fSmall, PointF(24.0f, 44.0f), &verBrush);

    RectF adminBadge(22.0f, 68.0f, 136.0f, 22.0f);
    RenderUtils::DrawBadge(g, adminBadge, L"ADMINISTRATOR", Color(255, 16, 50, 35), Color(255, 16, 185, 129), m_fSmall);

    // Browser Target Selector (Segmented pills)
    RectF browCard(16.0f, 102.0f, rect.Width - 32.0f, 44.0f);
    RenderUtils::DrawCard(g, browCard, Theme::BgCard, Theme::BorderSubtle, 8.0f);

    float btnW = (browCard.Width - 8.0f) / 3.0f;
    const wchar_t* bNames[] = { L"Chrome", L"Brave", L"Edge" };
    for (int i = 0; i < 3; ++i) {
        RectF bPill(browCard.X + 4.0f + i * btnW, browCard.Y + 4.0f, btnW - 2.0f, 36.0f);
        bool isSel = (m_selectedBrowserIdx == i);
        Color pillBg = isSel ? Color(255, 30, 41, 59) : Color(0, 0, 0, 0);
        Color pillTxt = isSel ? Color(255, 255, 255, 255) : Theme::TextSecondary;

        if (isSel) {
            SolidBrush pillBrush(pillBg);
            RenderUtils::FillRoundedRect(g, pillBrush, bPill, 6.0f);
        }

        SolidBrush pillTxtBrush(pillTxt);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(bNames[i], -1, isSel ? m_fBold : m_fSmall, bPill, &sf, &pillTxtBrush);
    }

    // Navigation Items (Exact order in ui.png)
    struct NavEntry {
        NavPage page;
        const wchar_t* label;
    };
    const NavEntry navEntries[] = {
        { NavPage::Dashboard, L"田  Dashboard" },
        { NavPage::Tweaks,    L"⚙  Optimizations" },
        { NavPage::Cleaner,   L"🧹  Deep Cleaner" },
        { NavPage::Shield,    L"🛡  Network Shield" },
        { NavPage::Updates,   L"🌐  Browser Updates" },
        { NavPage::Backups,   L"🕒  Backups & Restore" },
        { NavPage::Logs,      L">_  Activity Console" }
    };

    float navY = 166.0f;
    float navH = 38.0f;
    for (int i = 0; i < 7; ++i) {
        RectF navItem(16.0f, navY + i * (navH + 6.0f), rect.Width - 32.0f, navH);
        bool isActive = (m_currentPage == navEntries[i].page);

        if (isActive) {
            SolidBrush activeBrush(Color(255, 26, 35, 50));
            RenderUtils::FillRoundedRect(g, activeBrush, navItem, 6.0f);
            SolidBrush barBrush(Theme::AccentBlue);
            g.FillRectangle(&barBrush, navItem.X, navItem.Y + 6.0f, 3.5f, navItem.Height - 12.0f);
        }

        SolidBrush itemTxt(isActive ? Theme::TextPrimary : Theme::TextSecondary);
        StringFormat sf;
        sf.SetLineAlignment(StringAlignmentCenter);
        RectF textRc(navItem.X + 16.0f, navItem.Y, navItem.Width - 20.0f, navItem.Height);
        g.DrawString(navEntries[i].label, -1, isActive ? m_fBold : m_fNormal, textRc, &sf, &itemTxt);
    }

    // Status Card at Bottom
    RectF statusCard(16.0f, rect.Height - 88.0f, rect.Width - 32.0f, 74.0f);
    RenderUtils::DrawCard(g, statusCard, Theme::BgCard, Theme::BorderSubtle, 6.0f);

    SolidBrush statTitle(Theme::TextPrimary);
    g.DrawString(L"Protection Status", -1, m_fBold, PointF(statusCard.X + 12.0f, statusCard.Y + 8.0f), &statTitle);

    std::wstring shieldShort = m_shieldStatus.hostsBlockActive ? L"Shield Active · Telemetry Sunk" : L"Shield Inactive";
    SolidBrush statDesc(Theme::TextMuted);
    g.DrawString(shieldShort.c_str(), -1, m_fSmall, PointF(statusCard.X + 12.0f, statusCard.Y + 28.0f), &statDesc);

    std::wstring statusStr = m_isBusy ? (L"● " + m_progressAction) : L"● Engine Idle";
    SolidBrush statInd(m_isBusy ? Theme::WarningOrange : Theme::SuccessGreen);
    g.DrawString(statusStr.c_str(), -1, m_fSmall, PointF(statusCard.X + 12.0f, statusCard.Y + 46.0f), &statInd);
}

void AppWindow::RenderDashboard(Graphics& g, const RectF& rect) {
    float startX = rect.X + 32.0f;
    float startY = 24.0f;
    float contentW = rect.Width - 64.0f;

    // Progress bar overlay when busy
    if (m_isBusy) {
        RectF pBarBg(rect.X + 12.0f, 4.0f, rect.Width - 24.0f, 10.0f);
        SolidBrush pbBg(Color(255, 20, 28, 42));
        RenderUtils::FillRoundedRect(g, pbBg, pBarBg, 5.0f);
        float filled = (pBarBg.Width - 4.0f) * ((float)m_progressPercent / 100.0f);
        if (filled > 0) {
            RectF pBarFill(pBarBg.X + 2.0f, pBarBg.Y + 2.0f, filled, pBarBg.Height - 4.0f);
            SolidBrush pbFill(Theme::AccentBlue);
            RenderUtils::FillRoundedRect(g, pbFill, pBarFill, 3.0f);
        }
        SolidBrush actBrush(Theme::AccentBlue);
        g.DrawString((L"⚡ " + m_progressAction + L" (" + std::to_wstring(m_progressPercent) + L"%)").c_str(),
            -1, m_fSmall, PointF(startX, 18.0f), &actBrush);
        startY = 44.0f;
    }

    SolidBrush titleBrush(Theme::TextPrimary);
    SolidBrush subBrush(Theme::TextSecondary);

    // Left Header: "Dashboard" title matching ui.png
    g.DrawString(L"Dashboard", -1, m_fMainTitle, PointF(startX, startY), &titleBrush);

    std::wstring sub = L"Active Target: " + m_browsers[m_selectedBrowserIdx].name;
    if (!m_browsers[m_selectedBrowserIdx].version.empty()) {
        sub += L" (v" + m_browsers[m_selectedBrowserIdx].version + L")";
    }
    g.DrawString(sub.c_str(), -1, m_fSubtitle, PointF(startX, startY + 36.0f), &subBrush);

    // Right Header: Health Score Ring matching ui.png
    float ringX = startX + (contentW * 0.52f);
    float ringY = startY;
    float ringR = 64.0f;
    RectF ringBox(ringX - ringR, ringY + 28.0f, ringR * 2.0f, ringR * 2.0f);

    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);

    // "Health Score Ring" title
    RectF ringTitleRc(ringX - 110.0f, ringY, 220.0f, 24.0f);
    g.DrawString(L"Health Score Ring", -1, m_fBold, ringTitleRc, &sfCenter, &titleBrush);

    // Background track arc
    Pen trackPen(Color(255, 30, 41, 59), 16.0f);
    trackPen.SetStartCap(LineCapRound);
    trackPen.SetEndCap(LineCapRound);
    g.DrawArc(&trackPen, ringBox, 135, 270);

    // Foreground arc
    Color arcColor = (m_auditReport.score >= 80) ? Color(255, 16, 185, 129) : ((m_auditReport.score >= 50) ? Theme::WarningOrange : Theme::DangerRed);
    Pen fillPen(arcColor, 16.0f);
    fillPen.SetStartCap(LineCapRound);
    fillPen.SetEndCap(LineCapRound);
    float sweep = (270.0f * (float)m_auditReport.score) / 100.0f;
    if (sweep > 0.0f) {
        g.DrawArc(&fillPen, ringBox, 135, sweep);
    }

    // Centered percentage & subtitle inside ring
    std::wstring pctStr = std::to_wstring(m_auditReport.score) + L"%";
    SolidBrush pctBrush(arcColor);
    RectF pctRc(ringX - 60.0f, ringBox.Y + 30.0f, 120.0f, 38.0f);
    g.DrawString(pctStr.c_str(), -1, m_fScore, pctRc, &sfCenter, &pctBrush);

    std::wstring subStr = (m_auditReport.score >= 90) ? L"Fully Hardened" : ((m_auditReport.score >= 70) ? L"Hardened" : L"Action Needed");
    SolidBrush subTxtBrush(arcColor);
    RectF subRc(ringX - 70.0f, ringBox.Y + 70.0f, 140.0f, 20.0f);
    g.DrawString(subStr.c_str(), -1, m_fSmall, subRc, &sfCenter, &subTxtBrush);

    // 2. Three Metric Cards Row (Protected Policies, AI Models, Reclaimable Cache)
    float cardsY = ringBox.Y + ringBox.Height + 20.0f;
    float cardW = (contentW - 32.0f) / 3.0f;
    float cardH = 88.0f;

    // Card 1: Protected Policies
    RectF c1(startX, cardsY, cardW, cardH);
    RenderUtils::DrawCard(g, c1, Theme::BgCard, Theme::BorderSubtle, 10.0f);
    std::wstring polStr = std::to_wstring(m_auditReport.optimizedCount);
    RectF c1Num(c1.X + 16.0f, c1.Y + 12.0f, c1.Width - 32.0f, 36.0f);
    g.DrawString(polStr.c_str(), -1, m_fHuge, c1Num, &sfCenter, &titleBrush);
    RectF c1Lbl(c1.X + 16.0f, c1.Y + 50.0f, c1.Width - 32.0f, 22.0f);
    g.DrawString(L"Protected Policies", -1, m_fSmall, c1Lbl, &sfCenter, &subBrush);

    // Card 2: AI Models (Eliminated)
    RectF c2(startX + cardW + 16.0f, cardsY, cardW, cardH);
    RenderUtils::DrawCard(g, c2, Theme::BgCard, Theme::BorderSubtle, 10.0f);
    RectF c2Num(c2.X + 16.0f, c2.Y + 12.0f, c2.Width - 32.0f, 36.0f);
    g.DrawString(L"0", -1, m_fHuge, c2Num, &sfCenter, &titleBrush);
    RectF c2Lbl(c2.X + 16.0f, c2.Y + 50.0f, c2.Width - 32.0f, 22.0f);
    g.DrawString(L"AI Models (Eliminated)", -1, m_fSmall, c2Lbl, &sfCenter, &subBrush);

    // Card 3: Reclaimable Cache
    RectF c3(startX + (cardW + 16.0f) * 2.0f, cardsY, cardW, cardH);
    RenderUtils::DrawCard(g, c3, Theme::BgCard, Theme::BorderSubtle, 10.0f);
    std::wstring cacheStr = std::to_wstring(m_auditReport.reclaimableBytes / (1024 * 1024)) + L" MB";
    RectF c3Num(c3.X + 16.0f, c3.Y + 12.0f, c3.Width - 32.0f, 36.0f);
    g.DrawString(cacheStr.c_str(), -1, m_fHuge, c3Num, &sfCenter, &titleBrush);
    RectF c3Lbl(c3.X + 16.0f, c3.Y + 50.0f, c3.Width - 32.0f, 22.0f);
    g.DrawString(L"Reclaimable Cache", -1, m_fSmall, c3Lbl, &sfCenter, &subBrush);

    // 3. Five Quick Action Buttons Row (including Hardened Gaming Mode)
    float btnY = cardsY + cardH + 16.0f;
    float btn1W = 250.0f;
    float otherBtnW = (contentW - btn1W - 48.0f) / 4.0f;
    float btnH = 42.0f;

    // Primary: Apply Maximum Optimization (lavender/indigo pill with dark text)
    RectF b1(startX, btnY, btn1W, btnH);
    SolidBrush b1Bg(Color(255, 219, 226, 255));
    RenderUtils::FillRoundedRect(g, b1Bg, b1, 21.0f);
    SolidBrush b1Txt(Color(255, 15, 23, 42));
    g.DrawString(L"⚡  Apply Maximum", -1, m_fBold, b1, &sfCenter, &b1Txt);

    // Button 2: Re-Scan System
    RectF b2(startX + btn1W + 12.0f, btnY, otherBtnW, btnH);
    RenderUtils::DrawCard(g, b2, Theme::BgCard, Theme::BorderSubtle, 21.0f);
    g.DrawString(L"🔄  Re-Scan", -1, m_fBold, b2, &sfCenter, &titleBrush);

    // Button 3: Install Gaming Mode
    RectF b3(startX + btn1W + 12.0f + (otherBtnW + 12.0f), btnY, otherBtnW, btnH);
    RenderUtils::DrawCard(g, b3, Theme::BgCard, Theme::BorderSubtle, 21.0f);
    g.DrawString(L"🎮  Gaming Mode", -1, m_fBold, b3, &sfCenter, &titleBrush);

    // Button 4: Create Snapshot
    RectF b4(startX + btn1W + 12.0f + (otherBtnW + 12.0f) * 2.0f, btnY, otherBtnW, btnH);
    RenderUtils::DrawCard(g, b4, Theme::BgCard, Theme::BorderSubtle, 21.0f);
    g.DrawString(L"📸  Snapshot", -1, m_fBold, b4, &sfCenter, &titleBrush);

    // Button 5: Engage Shield
    RectF b5(startX + btn1W + 12.0f + (otherBtnW + 12.0f) * 3.0f, btnY, otherBtnW, btnH);
    RenderUtils::DrawCard(g, b5, Theme::BgCard, Theme::BorderSubtle, 21.0f);
    g.DrawString(L"🛡  Engage Shield", -1, m_fBold, b5, &sfCenter, &titleBrush);

    // 4. Audit Findings Table Card (matching ui.png)
    float tableY = btnY + btnH + 18.0f;
    float tableH = rect.Height - tableY - 20.0f;
    RectF tableCard(startX, tableY, contentW, tableH);
    RenderUtils::DrawCard(g, tableCard, Theme::BgCard, Theme::BorderSubtle, 10.0f);

    // Table Header
    RectF tHeader(tableCard.X + 20.0f, tableCard.Y + 12.0f, tableCard.Width - 40.0f, 22.0f);
    g.DrawString(L"Audit Findings", -1, m_fBold, PointF(tHeader.X, tHeader.Y), &titleBrush);
    StringFormat sfRight;
    sfRight.SetAlignment(StringAlignmentFar);
    g.DrawString(L"Status", -1, m_fBold, tHeader, &sfRight, &titleBrush);

    Pen rowDiv(Theme::BorderSubtle, 1.0f);
    g.DrawLine(&rowDiv, tableCard.X, tableCard.Y + 38.0f, tableCard.X + tableCard.Width, tableCard.Y + 38.0f);

    // Table Rows
    float rY = tableCard.Y + 42.0f;
    float rH = 44.0f;
    for (size_t i = 0; i < m_auditReport.items.size() && (rY + rH) <= (tableCard.Y + tableCard.Height - 8.0f); ++i) {
        bool isOpt = (m_auditReport.items[i].status == AuditStatus::Optimized);
        Color dotColor = isOpt ? Color(255, 16, 185, 129) : Theme::WarningOrange;
        SolidBrush dotBrush(dotColor);

        // Green filled circle checkmark icon
        RectF iconRc(tableCard.X + 20.0f, rY + 12.0f, 20.0f, 20.0f);
        g.FillEllipse(&dotBrush, iconRc);
        SolidBrush chkBrush(Color(255, 11, 14, 20));
        g.DrawString(isOpt ? L"✓" : L"!", -1, m_fSmall, iconRc, &sfCenter, &chkBrush);

        // Finding Title
        SolidBrush nameBrush(Theme::TextPrimary);
        g.DrawString(m_auditReport.items[i].name.c_str(), -1, m_fBold, PointF(tableCard.X + 48.0f, rY + 12.0f), &nameBrush);

        // Status pill badge: [ Fully Hardened ]
        RectF pillRc(tableCard.X + tableCard.Width - 146.0f, rY + 10.0f, 126.0f, 24.0f);
        RenderUtils::DrawBadge(
            g, pillRc,
            isOpt ? L"Fully Hardened" : L"Action Needed",
            isOpt ? Color(255, 16, 50, 35) : Color(255, 60, 35, 15),
            isOpt ? Color(255, 16, 185, 129) : Theme::WarningOrange,
            m_fSmall
        );

        rY += rH;
        if (i + 1 < m_auditReport.items.size() && (rY + rH) <= (tableCard.Y + tableCard.Height - 8.0f)) {
            g.DrawLine(&rowDiv, tableCard.X + 16.0f, rY, tableCard.X + tableCard.Width - 16.0f, rY);
        }
    }
}

void AppWindow::RenderShield(Graphics& g, const RectF& rect) {
    float startX = rect.X + 28.0f;
    float startY = 24.0f;
    float contentW = rect.Width - 56.0f;

    SolidBrush titleBrush(Theme::TextPrimary);
    g.DrawString(L"Windows Firewall & Hosts Network Shield", -1, m_fTitle, PointF(startX, startY), &titleBrush);

    SolidBrush subBrush(Theme::TextSecondary);
    g.DrawString(L"Kernel-level blocking of Google telemetry, crashpad memory dumps, and tracking domains", -1, m_fSubtitle, PointF(startX, startY + 26.0f), &subBrush);

    // Master Action Bar
    float by = startY + 56.0f;
    RectF actAllBtn(startX, by, 320.0f, 42.0f);
    RenderUtils::DrawGradientButton(g, actAllBtn, L"🛡🔥  ENABLE FULL NETWORK SHIELD", false, false, m_fBold, !m_isBusy);

    RectF disAllBtn(startX + 332.0f, by, 220.0f, 42.0f);
    RenderUtils::DrawCard(g, disAllBtn, Theme::BgCard, Theme::BorderSubtle, 8.0f);
    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(L"🔓  Restore Network Rules", -1, m_fBold, disAllBtn, &sfCenter, &titleBrush);

    // Two Detailed Cards
    float cardY = by + 56.0f;
    float cardH = 150.0f;

    // Card 1: Windows Hosts File Sinkhole
    RectF c1(startX, cardY, contentW, cardH);
    RenderUtils::DrawCard(g, c1, Theme::BgCard, Theme::BorderSubtle, 8.0f);

    g.DrawString(L"🌐  Windows Hosts File Telemetry Sinkhole", -1, m_fBold, PointF(c1.X + 16.0f, c1.Y + 14.0f), &titleBrush);

    RectF badge1(c1.X + c1.Width - 170.0f, c1.Y + 14.0f, 154.0f, 24.0f);
    if (m_shieldStatus.hostsBlockActive) {
        RenderUtils::DrawBadge(g, badge1, L"● ACTIVE (SINKHOLED)", Theme::SuccessBadge, Theme::SuccessGreen, m_fSmall);
    } else {
        RenderUtils::DrawBadge(g, badge1, L"● INACTIVE", Theme::WarningBadge, Theme::WarningOrange, m_fSmall);
    }

    std::wstring hDesc = L"Redirects known Google telemetry, crashpad, analytics, and experiment domains to 0.0.0.0 (null route).\n"
                         L"Domains blocked: telemetry.google.com, crashpad.google.com, variations.google.com, google-analytics.com,\n"
                         L"optimizationguide-pa.googleapis.com, adservice.google.com, doubleclick.net, and 20+ tracking endpoints.";
    g.DrawString(hDesc.c_str(), -1, m_fSmall, PointF(c1.X + 16.0f, c1.Y + 44.0f), &subBrush);

    RectF btnHOn(c1.X + 16.0f, c1.Y + 104.0f, 160.0f, 32.0f);
    RenderUtils::DrawGradientButton(g, btnHOn, L"🛡 Block via Hosts", false, false, m_fSmall, !m_isBusy);

    RectF btnHOff(c1.X + 186.0f, c1.Y + 104.0f, 160.0f, 32.0f);
    RenderUtils::DrawCard(g, btnHOff, Theme::BgCardHover, Theme::BorderSubtle, 4.0f);
    g.DrawString(L"🔓 Restore Hosts", -1, m_fSmall, btnHOff, &sfCenter, &subBrush);

    // Card 2: Windows Defender Firewall Outbound Rules
    RectF c2(startX, cardY + cardH + 16.0f, contentW, cardH);
    RenderUtils::DrawCard(g, c2, Theme::BgCard, Theme::BorderSubtle, 8.0f);

    g.DrawString(L"🔥  Windows Defender Firewall Outbound Block Rules", -1, m_fBold, PointF(c2.X + 16.0f, c2.Y + 14.0f), &titleBrush);

    RectF badge2(c2.X + c2.Width - 170.0f, c2.Y + 14.0f, 154.0f, 24.0f);
    if (m_shieldStatus.firewallBlockActive) {
        RenderUtils::DrawBadge(g, badge2, L"● ACTIVE (FIREWALL ON)", Theme::SuccessBadge, Theme::SuccessGreen, m_fSmall);
    } else {
        RenderUtils::DrawBadge(g, badge2, L"● INACTIVE", Theme::WarningBadge, Theme::WarningOrange, m_fSmall);
    }

    std::wstring fDesc = L"Enforces Windows Filtering Platform (WFP) outbound block rules against updater and telemetry executables.\n"
                         L"Executable targets: GoogleUpdate.exe, GoogleUpdater.exe, crashpad_handler.exe, MicrosoftEdgeUpdate.exe,\n"
                         L"and BraveUpdate.exe — completely blocking unauthorized outbound background communication.";
    g.DrawString(fDesc.c_str(), -1, m_fSmall, PointF(c2.X + 16.0f, c2.Y + 44.0f), &subBrush);

    RectF btnFOn(c2.X + 16.0f, c2.Y + 104.0f, 160.0f, 32.0f);
    RenderUtils::DrawGradientButton(g, btnFOn, L"🔥 Enable Firewall Rules", false, false, m_fSmall, !m_isBusy);

    RectF btnFOff(c2.X + 186.0f, c2.Y + 104.0f, 160.0f, 32.0f);
    RenderUtils::DrawCard(g, btnFOff, Theme::BgCardHover, Theme::BorderSubtle, 4.0f);
    g.DrawString(L"🔓 Remove Firewall Rules", -1, m_fSmall, btnFOff, &sfCenter, &subBrush);

    // Live Status Summary
    float stY = cardY + cardH * 2.0f + 32.0f;
    RectF stBar(startX, stY, contentW, 40.0f);
    RenderUtils::DrawCard(g, stBar, Theme::BgInput, Theme::BorderSubtle, 6.0f);
    std::wstring liveSt = L"● Current Protection State: " + m_shieldStatus.statusSummary;
    SolidBrush liveBrush(m_shieldStatus.hostsBlockActive ? Theme::SuccessGreen : Theme::WarningOrange);
    g.DrawString(liveSt.c_str(), -1, m_fSmall, PointF(stBar.X + 14.0f, stBar.Y + 12.0f), &liveBrush);
}

void AppWindow::RenderTweaks(Graphics& g, const RectF& rect) {
    float startX = rect.X + 28.0f;
    float startY = 24.0f;
    float contentW = rect.Width - 56.0f;

    SolidBrush titleBrush(Theme::TextPrimary);
    g.DrawString(L"Hardening & Optimization Modules (15 Subsystems)", -1, m_fTitle, PointF(startX, startY), &titleBrush);

    SolidBrush subBrush(Theme::TextSecondary);
    g.DrawString(L"Granular enterprise policies, memory clamps, network shield & performance flags", -1, m_fSubtitle, PointF(startX, startY + 26.0f), &subBrush);

    if (m_isBusy) {
        float pbY = startY + 48.0f;
        RectF pBarBg(startX, pbY, contentW, 8.0f);
        SolidBrush pbBg(Color(255, 20, 28, 42));
        RenderUtils::FillRoundedRect(g, pbBg, pBarBg, 4.0f);
        float filled = (contentW - 4.0f) * ((float)m_progressPercent / 100.0f);
        if (filled > 0) {
            RectF pBarFill(pBarBg.X + 2.0f, pBarBg.Y + 1.0f, filled, 6.0f);
            SolidBrush pbFill(Theme::AccentBlue);
            RenderUtils::FillRoundedRect(g, pbFill, pBarFill, 3.0f);
        }
        SolidBrush actBrush(Theme::AccentBlue);
        std::wstring statusMsg = L"⚡ " + m_progressAction + L" (" + std::to_wstring(m_progressPercent) + L"%)";
        g.DrawString(statusMsg.c_str(), -1, m_fSmall, PointF(startX, pbY + 12.0f), &actBrush);
        startY = pbY + 30.0f;
    }

    // 5 Presets Row (including Gaming Mode)
    float presY = startY + 52.0f;
    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);

    RectF p1(startX, presY, 130.0f, 32.0f);
    RenderUtils::DrawCard(g, p1, Theme::BgCardHover, Theme::BorderSubtle, 4.0f);
    g.DrawString(L"⚡ Maximum", -1, m_fSmall, p1, &sfCenter, &titleBrush);

    RectF p2(startX + 138.0f, presY, 130.0f, 32.0f);
    RenderUtils::DrawCard(g, p2, Theme::BgCard, Theme::BorderSubtle, 4.0f);
    g.DrawString(L"⚖ Balanced", -1, m_fSmall, p2, &sfCenter, &subBrush);

    RectF p3(startX + 276.0f, presY, 140.0f, 32.0f);
    RenderUtils::DrawCard(g, p3, Theme::BgCard, Theme::BorderSubtle, 4.0f);
    g.DrawString(L"🔒 Privacy Only", -1, m_fSmall, p3, &sfCenter, &subBrush);

    RectF p4(startX + 424.0f, presY, 170.0f, 32.0f);
    RenderUtils::DrawCard(g, p4, Theme::BgCard, Theme::BorderSubtle, 4.0f);
    g.DrawString(L"🧊 Ultra-Low Resource", -1, m_fSmall, p4, &sfCenter, &subBrush);

    RectF p5(startX + 602.0f, presY, 150.0f, 32.0f);
    RenderUtils::DrawCard(g, p5, Theme::BgCard, Theme::BorderSubtle, 4.0f);
    g.DrawString(L"🎮 Gaming Mode", -1, m_fSmall, p5, &sfCenter, &titleBrush);

    // Tweak Cards
    float cardY = presY + 42.0f - (float)m_tweakScrollY;
    for (size_t i = 0; i < m_tweaks.size(); ++i) {
        if (cardY + 68.0f < 0 || cardY > rect.Height) {
            cardY += 74.0f;
            continue;
        }

        RectF tc(startX, cardY, contentW, 66.0f);
        RenderUtils::DrawCard(g, tc, Theme::BgCard, Theme::BorderSubtle, 8.0f);

        g.DrawString(m_tweaks[i].title.c_str(), -1, m_fBold, PointF(tc.X + 16.0f, tc.Y + 11.0f), &titleBrush);
        g.DrawString(m_tweaks[i].description.c_str(), -1, m_fSmall, PointF(tc.X + 16.0f, tc.Y + 34.0f), &subBrush);

        RectF tagRc(tc.X + tc.Width - 170.0f, tc.Y + 20.0f, 106.0f, 22.0f);
        RenderUtils::DrawBadge(g, tagRc, m_tweaks[i].impactTag, Theme::BgCardHover, Theme::TextSecondary, m_fSmall);

        RectF togRc(tc.X + tc.Width - 52.0f, tc.Y + 20.0f, 40.0f, 22.0f);
        RenderUtils::DrawToggle(g, togRc, m_tweaks[i].enabled, false);

        cardY += 74.0f;
    }

    // Apply Button
    RectF applyBtn(startX, rect.Height - 60.0f, contentW, 44.0f);
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

    float cy = startY + 68.0f;
    const wchar_t* cleanItems[][2] = {
        { L"SQLite Database Vacuum & Reindex", L"Compacts History, Favicons, and Shortcuts databases via winsqlite3.dll" },
        { L"Purge GPU & Shader Caches", L"Cleans stale DawnCache, GrShaderCache, and GPUCache folders" },
        { L"Crashpad & Diagnostics Sweep", L"Removes accumulated crash telemetry and crashpad memory dumps" },
        { L"Flush Windows DNS Resolver", L"Clears host cache via DnsFlushResolverCache for instant name updates" }
    };

    for (int i = 0; i < 4; ++i) {
        RectF cRect(startX, cy + i * 78.0f, contentW, 66.0f);
        RenderUtils::DrawCard(g, cRect, Theme::BgCard, Theme::BorderSubtle, 8.0f);

        g.DrawString(cleanItems[i][0], -1, m_fBold, PointF(cRect.X + 16.0f, cRect.Y + 12.0f), &titleBrush);
        g.DrawString(cleanItems[i][1], -1, m_fSmall, PointF(cRect.X + 16.0f, cRect.Y + 36.0f), &subBrush);

        RectF rdyBadge(cRect.X + cRect.Width - 110.0f, cRect.Y + 22.0f, 96.0f, 22.0f);
        RenderUtils::DrawBadge(g, rdyBadge, L"READY", Theme::SuccessBadge, Theme::SuccessGreen, m_fSmall);
    }

    RectF runCleanBtn(startX, cy + 330.0f, 280.0f, 46.0f);
    RenderUtils::DrawGradientButton(g, runCleanBtn, L"🧹  RUN DEEP CLEAN NOW", false, false, m_fBold, !m_isBusy);
}

void AppWindow::RenderUpdates(Graphics& g, const RectF& rect) {
    float startX = rect.X + 28.0f;
    float startY = 24.0f;
    float contentW = rect.Width - 56.0f;

    SolidBrush titleBrush(Theme::TextPrimary);
    g.DrawString(L"Multi-Browser Update & Version Center", -1, m_fTitle, PointF(startX, startY), &titleBrush);

    SolidBrush subBrush(Theme::TextSecondary);
    g.DrawString(L"Live upstream release checking and permanent update lockdown for Chrome, Brave, and Edge", -1, m_fSubtitle, PointF(startX, startY + 26.0f), &subBrush);

    float by = startY + 56.0f;
    RectF checkBtn(startX, by, 280.0f, 38.0f);
    RenderUtils::DrawGradientButton(
        g, checkBtn,
        m_isCheckingUpdates ? L"⏳  Querying Upstream APIs..." : L"🔍  Check Upstream Releases Now",
        false, false, m_fBold, !m_isCheckingUpdates
    );

    float cardY = by + 52.0f;
    float cardH = 110.0f;

    for (size_t i = 0; i < 3; ++i) {
        RectF uCard(startX, cardY + i * (cardH + 12.0f), contentW, cardH);
        RenderUtils::DrawCard(g, uCard, Theme::BgCard, Theme::BorderSubtle, 8.0f);

        BrowserUpdateInfo info;
        if (i < m_updateInfos.size()) {
            info = m_updateInfos[i];
        } else {
            info.browserName = m_browsers[i].name;
            info.isInstalled = m_browsers[i].isInstalled;
            info.installedVersion = m_browsers[i].version;
            info.statusText = L"Querying upstream status...";
        }

        g.DrawString(info.browserName.c_str(), -1, m_fBold, PointF(uCard.X + 16.0f, uCard.Y + 14.0f), &titleBrush);

        std::wstring verLine = L"Installed: " + (info.isInstalled ? (L"v" + info.installedVersion) : L"Not Detected");
        if (!info.latestVersion.empty()) {
            verLine += L"   ·   Upstream Stable: v" + info.latestVersion;
        }
        g.DrawString(verLine.c_str(), -1, m_fSmall, PointF(uCard.X + 16.0f, uCard.Y + 40.0f), &subBrush);

        SolidBrush stBrush(info.isUpdateLocked ? Theme::SuccessGreen : Theme::AccentBlue);
        g.DrawString(info.statusText.c_str(), -1, m_fSmall, PointF(uCard.X + 16.0f, uCard.Y + 68.0f), &stBrush);

        RectF badgeRc(uCard.X + uCard.Width - 170.0f, uCard.Y + 14.0f, 154.0f, 24.0f);
        if (!info.isInstalled) {
            RenderUtils::DrawBadge(g, badgeRc, L"NOT DETECTED", Theme::BgCardHover, Theme::TextMuted, m_fSmall);
        } else if (info.isUpdateLocked) {
            RenderUtils::DrawBadge(g, badgeRc, L"● LOCKED & FROZEN", Theme::SuccessBadge, Theme::SuccessGreen, m_fSmall);
        } else if (info.isUpToDate) {
            RenderUtils::DrawBadge(g, badgeRc, L"● UP TO DATE", Theme::SuccessBadge, Theme::AccentBlue, m_fSmall);
        } else {
            RenderUtils::DrawBadge(g, badgeRc, L"● UPDATE PENDING", Theme::WarningBadge, Theme::WarningOrange, m_fSmall);
        }

        if (info.isInstalled) {
            RectF lockBtn(uCard.X + uCard.Width - 280.0f, uCard.Y + 54.0f, 130.0f, 36.0f);
            RenderUtils::DrawCard(g, lockBtn, Theme::BgCardHover, Theme::BorderSubtle, 4.0f);
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            g.DrawString(L"🔒 Freeze", -1, m_fSmall, lockBtn, &sf, &titleBrush);

            RectF unlockBtn(uCard.X + uCard.Width - 140.0f, uCard.Y + 54.0f, 130.0f, 36.0f);
            RenderUtils::DrawCard(g, unlockBtn, Theme::BgCardHover, Theme::BorderSubtle, 4.0f);
            g.DrawString(L"🔓 Allow", -1, m_fSmall, unlockBtn, &sf, &subBrush);
        }
    }
}

void AppWindow::RenderBackups(Graphics& g, const RectF& rect) {
    float startX = rect.X + 28.0f;
    float startY = 24.0f;
    float contentW = rect.Width - 56.0f;

    SolidBrush titleBrush(Theme::TextPrimary);
    g.DrawString(L"Registry Snapshot & Rollback Center", -1, m_fTitle, PointF(startX, startY), &titleBrush);

    SolidBrush subBrush(Theme::TextSecondary);
    g.DrawString(L"ChromeDebloater automatically backs up registry states before applying tweaks", -1, m_fSubtitle, PointF(startX, startY + 26.0f), &subBrush);

    float by = startY + 68.0f;
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
            RectF sRc(startX, sy + i * 58.0f, contentW, 50.0f);
            RenderUtils::DrawCard(g, sRc, Theme::BgCard, Theme::BorderSubtle, 6.0f);

            g.DrawString(m_snapshots[i].id.c_str(), -1, m_fBold, PointF(sRc.X + 16.0f, sRc.Y + 15.0f), &titleBrush);

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

    if (m_isBusy) {
        SolidBrush actBrush(Theme::AccentBlue);
        std::wstring busyMsg = L"⚡ Engine Active: " + m_progressAction + L" (" + std::to_wstring(m_progressPercent) + L"%)";
        g.DrawString(busyMsg.c_str(), -1, m_fSubtitle, PointF(startX, startY + 26.0f), &actBrush);

        RectF pBarBg(startX, startY + 50.0f, contentW, 6.0f);
        SolidBrush pbBg(Color(255, 20, 28, 42));
        RenderUtils::FillRoundedRect(g, pbBg, pBarBg, 3.0f);
        float filled = (contentW - 4.0f) * ((float)m_progressPercent / 100.0f);
        if (filled > 0) {
            RectF pBarFill(pBarBg.X + 2.0f, pBarBg.Y + 1.0f, filled, 4.0f);
            SolidBrush pbFill(Theme::AccentBlue);
            RenderUtils::FillRoundedRect(g, pbFill, pBarFill, 2.0f);
        }
        startY = 68.0f;
    } else {
        g.DrawString(L"Real-time event streaming and Win32 kernel command execution", -1, m_fSubtitle, PointF(startX, startY + 26.0f), &subBrush);

        RectF clearBtn(startX + contentW - 100.0f, startY + 20.0f, 96.0f, 26.0f);
        RenderUtils::DrawCard(g, clearBtn, Theme::BgCardHover, Theme::BorderSubtle, 4.0f);
        StringFormat sfCtr;
        sfCtr.SetAlignment(StringAlignmentCenter);
        sfCtr.SetLineAlignment(StringAlignmentCenter);
        SolidBrush clrBrush(Theme::TextSecondary);
        g.DrawString(L"Clear", -1, m_fSmall, clearBtn, &sfCtr, &clrBrush);
        startY = 60.0f;
    }

    float termY = startY;
    float termH = rect.Height - termY - 12.0f;
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
        else if (m_logs[i].level == L"INFO") lvlColor = Theme::TextSecondary;

        SolidBrush lvlBrush(lvlColor);
        std::wstring tag = L"[" + m_logs[i].level + L"]";
        g.DrawString(tag.c_str(), -1, m_fConsole, PointF(termRc.X + 90.0f, logLineY), &lvlBrush);

        SolidBrush msgBrush(Theme::TextPrimary);
        g.DrawString(m_logs[i].text.c_str(), -1, m_fConsole, PointF(termRc.X + 180.0f, logLineY), &msgBrush);

        logLineY += 22.0f;
    }

    if (!m_isBusy && !m_logs.empty()) {
        SolidBrush readyBrush(Theme::TextMuted);
        g.DrawString(L"● Engine Idle — ready for next operation", -1, m_fSmall,
            PointF(termRc.X + 16.0f, termRc.Y + termH - 22.0f), &readyBrush);
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
        int maxScroll = (int)(m_tweaks.size() * 74) - 340;
        if (maxScroll < 0) maxScroll = 0;
        if (m_tweakScrollY > maxScroll) m_tweakScrollY = maxScroll;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void AppWindow::OnLButtonDown(int x, int y) {
    RECT cr;
    GetClientRect(m_hwnd, &cr);
    float clientW = (float)(cr.right - cr.left);
    float clientH = (float)(cr.bottom - cr.top);
    float sidebarW = 240.0f;
    float contentW = clientW - sidebarW;

    // 1. Sidebar Browser Picker
    RectF browCard(16.0f, 102.0f, sidebarW - 32.0f, 44.0f);
    if (x >= browCard.X && x <= browCard.X + browCard.Width && y >= browCard.Y && y <= browCard.Y + browCard.Height) {
        float btnW = (browCard.Width - 8.0f) / 3.0f;
        int clickedIdx = (int)((x - (browCard.X + 4.0f)) / btnW);
        if (clickedIdx >= 0 && clickedIdx < 3 && clickedIdx != m_selectedBrowserIdx) {
            m_selectedBrowserIdx = clickedIdx;
            RunAuditAsync();
            RunCheckShieldAsync();
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        }
    }

    // 2. Sidebar Navigation (7 items matching navEntries order in RenderSidebar)
    const NavPage navPages[] = {
        NavPage::Dashboard,
        NavPage::Tweaks,
        NavPage::Cleaner,
        NavPage::Shield,
        NavPage::Updates,
        NavPage::Backups,
        NavPage::Logs
    };
    float navY = 166.0f;
    float navH = 38.0f;
    for (int i = 0; i < 7; ++i) {
        RectF navItem(16.0f, navY + i * (navH + 6.0f), sidebarW - 32.0f, navH);
        if (x >= navItem.X && x <= navItem.X + navItem.Width && y >= navItem.Y && y <= navItem.Y + navItem.Height) {
            m_currentPage = navPages[i];
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        }
    }

    // 3. Page Actions
    float startX = sidebarW + 28.0f;
    float startY = 24.0f;
    float pageContentW = contentW - 56.0f;

    if (m_currentPage == NavPage::Dashboard) {
        float dashStartX = sidebarW + 32.0f;
        float dashContentW = contentW - 64.0f;
        float dStartY = m_isBusy ? 44.0f : 24.0f;
        float ringR = 64.0f;
        float ringBoxH = ringR * 2.0f;
        float cardsY = dStartY + 28.0f + ringBoxH + 20.0f;
        float cardH = 88.0f;
        float btnY = cardsY + cardH + 16.0f;
        float btn1W = 250.0f;
        float otherBtnW = (dashContentW - btn1W - 48.0f) / 4.0f;
        float btnH = 42.0f;

        // Button 1: Apply Maximum Optimization
        RectF b1(dashStartX, btnY, btn1W, btnH);
        if (x >= b1.X && x <= b1.X + b1.Width && y >= b1.Y && y <= b1.Y + b1.Height) {
            TweakEngine::ApplyPreset(m_tweaks, TweakPreset::Maximum);
            RunApplyTweaksAsync();
            return;
        }
        // Button 2: Re-Scan System
        RectF b2(dashStartX + btn1W + 12.0f, btnY, otherBtnW, btnH);
        if (x >= b2.X && x <= b2.X + b2.Width && y >= b2.Y && y <= b2.Y + b2.Height) {
            RunAuditAsync();
            RunCheckShieldAsync();
            return;
        }
        // Button 3: Install Gaming Mode
        RectF b3(dashStartX + btn1W + 12.0f + (otherBtnW + 12.0f), btnY, otherBtnW, btnH);
        if (x >= b3.X && x <= b3.X + b3.Width && y >= b3.Y && y <= b3.Y + b3.Height) {
            RunInstallGamingModeAsync();
            return;
        }
        // Button 4: Create Snapshot
        RectF b4(dashStartX + btn1W + 12.0f + (otherBtnW + 12.0f) * 2.0f, btnY, otherBtnW, btnH);
        if (x >= b4.X && x <= b4.X + b4.Width && y >= b4.Y && y <= b4.Y + b4.Height) {
            RunCreateBackupAsync();
            return;
        }
        // Button 5: Engage Shield
        RectF b5(dashStartX + btn1W + 12.0f + (otherBtnW + 12.0f) * 3.0f, btnY, otherBtnW, btnH);
        if (x >= b5.X && x <= b5.X + b5.Width && y >= b5.Y && y <= b5.Y + b5.Height) {
            RunToggleAllShieldAsync(true);
            return;
        }
    } else if (m_currentPage == NavPage::Tweaks) {
        float presY = startY + 52.0f;
        if (m_isBusy) presY += 30.0f;
        if (y >= presY && y <= presY + 32.0f) {
            if (x >= startX && x <= startX + 130.0f) {
                TweakEngine::ApplyPreset(m_tweaks, TweakPreset::Maximum);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            } else if (x >= startX + 138.0f && x <= startX + 268.0f) {
                TweakEngine::ApplyPreset(m_tweaks, TweakPreset::Balanced);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            } else if (x >= startX + 276.0f && x <= startX + 416.0f) {
                TweakEngine::ApplyPreset(m_tweaks, TweakPreset::PrivacyOnly);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            } else if (x >= startX + 424.0f && x <= startX + 594.0f) {
                TweakEngine::ApplyPreset(m_tweaks, TweakPreset::UltraLowResource);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            } else if (x >= startX + 602.0f && x <= startX + 752.0f) {
                TweakEngine::ApplyPreset(m_tweaks, TweakPreset::GamingMode);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            }
        }

        // Toggles
        float cardY = presY + 42.0f - (float)m_tweakScrollY;
        for (size_t i = 0; i < m_tweaks.size(); ++i) {
            RectF togRc(startX + pageContentW - 52.0f, cardY + 20.0f, 40.0f, 22.0f);
            if (x >= togRc.X - 10.0f && x <= togRc.X + togRc.Width + 10.0f && y >= togRc.Y && y <= togRc.Y + togRc.Height) {
                m_tweaks[i].enabled = !m_tweaks[i].enabled;
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            }
            cardY += 74.0f;
        }

        // Apply Button
        RectF applyBtn(startX, clientH - 60.0f, pageContentW, 44.0f);
        if (x >= applyBtn.X && x <= applyBtn.X + applyBtn.Width && y >= applyBtn.Y && y <= applyBtn.Y + applyBtn.Height) {
            RunApplyTweaksAsync();
            return;
        }
    } else if (m_currentPage == NavPage::Shield) {
        float by = startY + 56.0f;
        // Master Buttons
        if (x >= startX && x <= startX + 320.0f && y >= by && y <= by + 42.0f) {
            RunToggleAllShieldAsync(true);
            return;
        } else if (x >= startX + 332.0f && x <= startX + 552.0f && y >= by && y <= by + 42.0f) {
            RunToggleAllShieldAsync(false);
            return;
        }

        // Card 1 Hosts Buttons
        float cardY = by + 56.0f;
        float cardH = 150.0f;
        RectF c1(startX, cardY, pageContentW, cardH);
        if (x >= c1.X + 16.0f && x <= c1.X + 176.0f && y >= c1.Y + 104.0f && y <= c1.Y + 136.0f) {
            RunToggleHostsBlockAsync(true);
            return;
        } else if (x >= c1.X + 186.0f && x <= c1.X + 346.0f && y >= c1.Y + 104.0f && y <= c1.Y + 136.0f) {
            RunToggleHostsBlockAsync(false);
            return;
        }

        // Card 2 Firewall Buttons
        RectF c2(startX, cardY + cardH + 16.0f, pageContentW, cardH);
        if (x >= c2.X + 16.0f && x <= c2.X + 176.0f && y >= c2.Y + 104.0f && y <= c2.Y + 136.0f) {
            RunToggleFirewallBlockAsync(true);
            return;
        } else if (x >= c2.X + 186.0f && x <= c2.X + 346.0f && y >= c2.Y + 104.0f && y <= c2.Y + 136.0f) {
            RunToggleFirewallBlockAsync(false);
            return;
        }
    } else if (m_currentPage == NavPage::Cleaner) {
        float cy = startY + 68.0f;
        float runY = cy + 330.0f;
        if (x >= startX && x <= startX + 280.0f && y >= runY && y <= runY + 46.0f) {
            RunDeepCleanAsync();
            return;
        }
    } else if (m_currentPage == NavPage::Updates) {
        float by = startY + 56.0f;
        if (x >= startX && x <= startX + 280.0f && y >= by && y <= by + 38.0f) {
            RunCheckUpdatesAsync();
            return;
        }

        float cardY = by + 52.0f;
        float cardH = 110.0f;

        for (int i = 0; i < 3; ++i) {
            RectF uCard(startX, cardY + i * (cardH + 12.0f), pageContentW, cardH);
            RectF lockBtn(uCard.X + uCard.Width - 280.0f, uCard.Y + 54.0f, 130.0f, 36.0f);
            RectF unlockBtn(uCard.X + uCard.Width - 140.0f, uCard.Y + 54.0f, 130.0f, 36.0f);

            if (x >= lockBtn.X && x <= lockBtn.X + lockBtn.Width && y >= lockBtn.Y && y <= lockBtn.Y + lockBtn.Height) {
                RunToggleUpdateLockAsync(i, true);
                return;
            } else if (x >= unlockBtn.X && x <= unlockBtn.X + unlockBtn.Width && y >= unlockBtn.Y && y <= unlockBtn.Y + unlockBtn.Height) {
                RunToggleUpdateLockAsync(i, false);
                return;
            }
        }
    } else if (m_currentPage == NavPage::Backups) {
        float by = startY + 68.0f;
        if (x >= startX && x <= startX + 220.0f && y >= by && y <= by + 40.0f) {
            RunCreateBackupAsync();
            return;
        } else if (x >= startX + 236.0f && x <= startX + 456.0f && y >= by && y <= by + 40.0f) {
            RunResetPoliciesAsync();
            return;
        }

        float listY = by + 60.0f;
        float sy = listY + 22.0f;
        for (size_t i = 0; i < m_snapshots.size() && i < 5; ++i) {
            RectF sRc(startX, sy + i * 58.0f, pageContentW, 50.0f);
            RectF rst(sRc.X + sRc.Width - 110.0f, sRc.Y + 12.0f, 96.0f, 26.0f);
            if (x >= rst.X && x <= rst.X + rst.Width && y >= rst.Y && y <= rst.Y + rst.Height) {
                RunRestoreBackupAsync(m_snapshots[i].id);
                return;
            }
        }
    } else if (m_currentPage == NavPage::Logs) {
        if (!m_isBusy) {
            float clearBtnX = startX + pageContentW - 100.0f;
            if (x >= clearBtnX && x <= clearBtnX + 96.0f && y >= startY + 20.0f && y <= startY + 46.0f) {
                m_logs.clear();
                AddLog(L"Activity console cleared.", L"INFO");
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Async Thread Dispatchers
// ─────────────────────────────────────────────────────────────────────────────

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

void AppWindow::RunCheckUpdatesAsync() {
    if (m_isCheckingUpdates) return;
    m_isCheckingUpdates = true;
    HWND hwnd = m_hwnd;

    AddLog(L"Checking upstream release channels for Chrome, Brave, and Edge...", L"ACTION");

    CreateThread(NULL, 0, [](LPVOID p) -> DWORD {
        HWND h = (HWND)p;
        AppWindow* win = (AppWindow*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (!win) return 0;

        auto infos = UpdateChecker::CheckAll(win->m_browsers);
        auto* pInfos = new std::vector<BrowserUpdateInfo>(infos);
        PostMessageW(h, WM_APP_UPDATES_DONE, 0, (LPARAM)pInfos);
        return 0;
    }, hwnd, 0, NULL);
}

void AppWindow::RunCheckShieldAsync() {
    HWND hwnd = m_hwnd;
    BrowserTarget target = m_browsers[m_selectedBrowserIdx];

    CreateThread(NULL, 0, [](LPVOID p) -> DWORD {
        HWND h = (HWND)p;
        AppWindow* win = (AppWindow*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (!win) return 0;

        BrowserTarget t = win->m_browsers[win->m_selectedBrowserIdx];
        NetworkShieldStatus st = NetworkShield::GetStatus(t);
        NetworkShieldStatus* pSt = new NetworkShieldStatus(st);
        PostMessageW(h, WM_APP_SHIELD_DONE, 0, (LPARAM)pSt);
        return 0;
    }, hwnd, 0, NULL);
}

void AppWindow::RunToggleHostsBlockAsync(bool enable) {
    m_currentPage = NavPage::Logs;
    HWND hwnd = m_hwnd;

    CreateThread(NULL, 0, [](LPVOID p) -> DWORD {
        HWND h = (HWND)p;
        AppWindow* win = (AppWindow*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (!win) return 0;

        auto logCb = [h](const std::wstring& msg, const std::wstring& lvl) {
            std::wstring* pMsg = new std::wstring(msg);
            std::wstring* pLvl = new std::wstring(lvl);
            PostMessageW(h, WM_APP_ENGINE_LOG, (WPARAM)pLvl, (LPARAM)pMsg);
        };

        if (win->m_shieldStatus.hostsBlockActive) {
            NetworkShield::DisableHostsBlock(logCb);
        } else {
            NetworkShield::EnableHostsBlock(logCb);
        }

        PostMessageW(h, WM_APP_ENGINE_DONE, 0, 0);
        return 0;
    }, hwnd, 0, NULL);
}

void AppWindow::RunToggleFirewallBlockAsync(bool enable) {
    m_currentPage = NavPage::Logs;
    HWND hwnd = m_hwnd;

    CreateThread(NULL, 0, [](LPVOID p) -> DWORD {
        HWND h = (HWND)p;
        AppWindow* win = (AppWindow*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (!win) return 0;
        BrowserTarget t = win->m_browsers[win->m_selectedBrowserIdx];

        auto logCb = [h](const std::wstring& msg, const std::wstring& lvl) {
            std::wstring* pMsg = new std::wstring(msg);
            std::wstring* pLvl = new std::wstring(lvl);
            PostMessageW(h, WM_APP_ENGINE_LOG, (WPARAM)pLvl, (LPARAM)pMsg);
        };

        if (win->m_shieldStatus.firewallBlockActive) {
            NetworkShield::DisableFirewallBlock(logCb);
        } else {
            NetworkShield::EnableFirewallBlock(t, logCb);
        }

        PostMessageW(h, WM_APP_ENGINE_DONE, 0, 0);
        return 0;
    }, hwnd, 0, NULL);
}

void AppWindow::RunToggleAllShieldAsync(bool enable) {
    m_currentPage = NavPage::Logs;
    HWND hwnd = m_hwnd;

    CreateThread(NULL, 0, [](LPVOID p) -> DWORD {
        HWND h = (HWND)p;
        AppWindow* win = (AppWindow*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (!win) return 0;
        BrowserTarget t = win->m_browsers[win->m_selectedBrowserIdx];

        auto logCb = [h](const std::wstring& msg, const std::wstring& lvl) {
            std::wstring* pMsg = new std::wstring(msg);
            std::wstring* pLvl = new std::wstring(lvl);
            PostMessageW(h, WM_APP_ENGINE_LOG, (WPARAM)pLvl, (LPARAM)pMsg);
        };

        if (win->m_shieldStatus.hostsBlockActive && win->m_shieldStatus.firewallBlockActive) {
            NetworkShield::DisableAll(logCb);
        } else {
            NetworkShield::EnableAll(t, logCb);
        }

        PostMessageW(h, WM_APP_ENGINE_DONE, 0, 0);
        return 0;
    }, hwnd, 0, NULL);
}

void AppWindow::RunToggleUpdateLockAsync(int browserIdx, bool lock) {
    if (browserIdx < 0 || browserIdx >= (int)m_browsers.size()) return;
    const BrowserTarget& b = m_browsers[browserIdx];

    if (lock) {
        AddLog(L"Enforcing 4-layer update freeze for " + b.name + L"...", L"ACTION");
        TweakEngine::WriteDualRegDword(b.updateKey, L"UpdateDefault", 0);
        TweakEngine::WriteDualRegDword(b.updateKey, L"AutoUpdateCheckPeriodMinutes", 0);
        TweakEngine::WriteDualRegDword(b.updateKey, L"DisableAutoUpdateChecksCheckboxValue", 1);
        TweakEngine::WriteDualRegDword(b.updateKey, L"InstallDefault", 0);

        if (!b.appGuid.empty()) {
            TweakEngine::WriteDualRegDword(b.updateKey, L"Update" + b.appGuid, 0);
            TweakEngine::WriteDualRegDword(b.updateKey, L"Install" + b.appGuid, 0);
            if (!b.version.empty()) {
                TweakEngine::WriteDualRegString(b.updateKey, L"TargetVersionPrefix" + b.appGuid, b.version);
            }
        }
        AddLog(L"[✓] " + b.name + L" frozen at installed version.", L"SUCCESS");
    } else {
        AddLog(L"Restoring update policies for " + b.name + L"...", L"ACTION");
        TweakEngine::DeleteRegValue(HKEY_LOCAL_MACHINE, b.updateKey, L"UpdateDefault");
        TweakEngine::DeleteRegValue(HKEY_CURRENT_USER, b.updateKey, L"UpdateDefault");
        TweakEngine::DeleteRegValue(HKEY_LOCAL_MACHINE, b.updateKey, L"AutoUpdateCheckPeriodMinutes");
        TweakEngine::DeleteRegValue(HKEY_CURRENT_USER, b.updateKey, L"AutoUpdateCheckPeriodMinutes");
        if (!b.appGuid.empty()) {
            TweakEngine::DeleteRegValue(HKEY_LOCAL_MACHINE, b.updateKey, L"TargetVersionPrefix" + b.appGuid);
            TweakEngine::DeleteRegValue(HKEY_CURRENT_USER, b.updateKey, L"TargetVersionPrefix" + b.appGuid);
        }
        AddLog(L"[✓] " + b.name + L" update policies restored to factory defaults.", L"SUCCESS");
    }

    RunCheckUpdatesAsync();
    RunAuditAsync();
}

struct AsyncParam {
    HWND hwnd;
    BrowserTarget browser;
    std::vector<int> tweakIds;
};

void AppWindow::RunApplyTweaksAsync() {
    if (m_isBusy) return;
    m_isBusy = true;
    m_progressPercent = 0;
    m_progressAction = L"Optimizing...";

    m_currentPage = NavPage::Logs;
    AddLog(L"══════════════════════════════════════════════", L"INFO");
    AddLog(L"Starting optimization sweep for " + m_browsers[m_selectedBrowserIdx].name, L"ACTION");

    std::vector<int> activeIds;
    for (const auto& tw : m_tweaks) {
        if (tw.enabled) activeIds.push_back(tw.id);
    }

    AsyncParam* param = new AsyncParam();
    param->hwnd = m_hwnd;
    param->browser = m_browsers[m_selectedBrowserIdx];
    param->tweakIds = activeIds;

    std::wstring bPath;
    BackupEngine::CreateSnapshot(param->browser, bPath);
    AddLog(L"Safety snapshot generated: " + bPath, L"ACTION");

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

void AppWindow::RunInstallGamingModeAsync() {
    if (m_isBusy) return;
    m_isBusy = true;
    m_progressPercent = 0;
    m_progressAction = L"Installing Gaming Mode...";

    m_currentPage = NavPage::Logs;
    AddLog(L"══════════════════════════════════════════════", L"INFO");
    AddLog(L"Installing Hardened Ultra Gaming Mode for " + m_browsers[m_selectedBrowserIdx].name, L"ACTION");

    HWND hwnd = m_hwnd;
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

        ShortcutManager::InstallGamingMode(b, logCb);

        // Also apply GamingMode policy preset to live tweaks list
        TweakEngine::ApplyPreset(win->m_tweaks, TweakPreset::GamingMode);

        PostMessageW(h, WM_APP_ENGINE_DONE, 0, 0);
        return 0;
    }, hwnd, 0, NULL);
}

void AppWindow::RunDeepCleanAsync() {
    if (m_isBusy) return;
    m_isBusy = true;
    m_progressPercent = 0;
    m_progressAction = L"Running deep clean...";

    m_currentPage = NavPage::Logs;
    AddLog(L"══════════════════════════════════════════════", L"INFO");
    AddLog(L"Starting Deep Cleaner — SQLite vacuum, cache sweep, DNS flush", L"ACTION");

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
    }, m_hwnd, 0, NULL);
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
        RunCheckUpdatesAsync();
        RunCheckShieldAsync();
    }
}

void AppWindow::RunRestoreBackupAsync(const std::wstring& path) {
    std::wstring targetPath = path;
    for (const auto& snap : m_snapshots) {
        if (snap.id == path) {
            targetPath = snap.filePath;
            break;
        }
    }

    std::wstring prompt = L"Restore registry state from snapshot " + path + L"?";
    if (MessageBoxW(m_hwnd, prompt.c_str(), L"Confirm Restore", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        if (BackupEngine::RestoreSnapshot(targetPath)) {
            AddLog(L"Snapshot restored successfully: " + path, L"SUCCESS");
            RunAuditAsync();
            RunCheckUpdatesAsync();
            RunCheckShieldAsync();
        } else {
            AddLog(L"Failed to restore snapshot: " + path, L"WARNING");
        }
    }
}
