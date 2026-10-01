#include "render_utils.h"

using namespace Gdiplus;

void RenderUtils::AddRoundedRect(GraphicsPath& path, const RectF& rect, float radius) {
    float d = radius * 2.0f;
    path.AddArc(rect.X, rect.Y, d, d, 180, 90);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270, 90);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0, 90);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90, 90);
    path.CloseFigure();
}

void RenderUtils::FillRoundedRect(Graphics& g, const Brush& brush, const RectF& rect, float radius) {
    GraphicsPath path;
    AddRoundedRect(path, rect, radius);
    g.FillPath(&brush, &path);
}

void RenderUtils::DrawRoundedRect(Graphics& g, const Pen& pen, const RectF& rect, float radius) {
    GraphicsPath path;
    AddRoundedRect(path, rect, radius);
    g.DrawPath(&pen, &path);
}

void RenderUtils::DrawCard(Graphics& g, const RectF& rect, const Color& bgColor, const Color& borderColor, float radius) {
    SolidBrush bgBrush(bgColor);
    FillRoundedRect(g, bgBrush, rect, radius);

    Pen borderPen(borderColor, 1.0f);
    DrawRoundedRect(g, borderPen, rect, radius);
}

void RenderUtils::DrawGradientButton(Graphics& g, const RectF& rect, const std::wstring& text, bool isHovered, bool isPressed, Font* font, bool enabled) {
    if (!enabled) {
        SolidBrush disBrush(Color(255, 30, 41, 59));
        FillRoundedRect(g, disBrush, rect, 8.0f);
        SolidBrush txtBrush(Color(255, 100, 116, 139));
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(text.c_str(), -1, font, rect, &sf, &txtBrush);
        return;
    }

    Color c1 = isHovered ? Color(255, 29, 78, 216) : Color(255, 37, 99, 235);
    Color c2 = isHovered ? Color(255, 109, 40, 217) : Color(255, 124, 58, 237);
    if (isPressed) {
        c1 = Color(255, 30, 64, 175);
        c2 = Color(255, 91, 33, 182);
    }

    LinearGradientBrush gradBrush(rect, c1, c2, LinearGradientModeHorizontal);
    FillRoundedRect(g, gradBrush, rect, 8.0f);

    SolidBrush txtBrush(Color(255, 255, 255, 255));
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(text.c_str(), -1, font, rect, &sf, &txtBrush);
}

void RenderUtils::DrawToggle(Graphics& g, const RectF& rect, bool isOn, bool isHovered) {
    float r = rect.Height / 2.0f;
    Color trackColor;
    if (isOn) {
        trackColor = isHovered ? Color(255, 37, 99, 235) : Color(255, 59, 130, 246);
    } else {
        trackColor = isHovered ? Color(255, 51, 65, 85) : Color(255, 30, 41, 59);
    }

    SolidBrush trackBrush(trackColor);
    FillRoundedRect(g, trackBrush, rect, r);

    // Knob
    float knobSize = rect.Height - 4.0f;
    float knobX = isOn ? (rect.X + rect.Width - knobSize - 2.0f) : (rect.X + 2.0f);
    float knobY = rect.Y + 2.0f;
    RectF knobRect(knobX, knobY, knobSize, knobSize);

    SolidBrush knobBrush(Color(255, 255, 255, 255));
    g.FillEllipse(&knobBrush, knobRect);
}

void RenderUtils::DrawBadge(Graphics& g, const RectF& rect, const std::wstring& text, const Color& bgColor, const Color& textColor, Font* font) {
    SolidBrush bg(bgColor);
    FillRoundedRect(g, bg, rect, 4.0f);

    SolidBrush txt(textColor);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(text.c_str(), -1, font, rect, &sf, &txt);
}

void RenderUtils::DrawScoreGauge(Graphics& g, const RectF& rect, int score, Font* titleFont, Font* numFont) {
    // Card background
    DrawCard(g, rect, Theme::BgCard, Theme::BorderSubtle, 10.0f);

    float cx = rect.X + 60.0f;
    float cy = rect.Y + rect.Height / 2.0f;
    float radius = 38.0f;
    RectF arcRect(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Track
    Pen trackPen(Color(255, 30, 41, 59), 8.0f);
    g.DrawArc(&trackPen, arcRect, 135, 270);

    // Fill Arc
    Color arcColor = (score >= 80) ? Theme::SuccessGreen : ((score >= 50) ? Theme::WarningOrange : Theme::DangerRed);
    Pen fillPen(arcColor, 8.0f);
    fillPen.SetStartCap(LineCapRound);
    fillPen.SetEndCap(LineCapRound);
    float sweep = (270.0f * (float)score) / 100.0f;
    if (sweep > 0.0f) {
        g.DrawArc(&fillPen, arcRect, 135, sweep);
    }

    // Number inside arc
    std::wstring scoreStr = std::to_wstring(score) + L"%";
    SolidBrush numBrush(Theme::TextPrimary);
    StringFormat numSf;
    numSf.SetAlignment(StringAlignmentCenter);
    numSf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(scoreStr.c_str(), -1, numFont, arcRect, &numSf, &numBrush);

    // Text details on right
    RectF textRect(cx + radius + 20.0f, rect.Y + 16.0f, rect.Width - (cx + radius + 30.0f), rect.Height - 32.0f);
    SolidBrush titleBrush(Theme::TextPrimary);
    g.DrawString(L"System Hardening & Optimization Score", -1, titleFont, PointF(textRect.X, textRect.Y), &titleBrush);

    std::wstring statusDesc;
    if (score >= 90) statusDesc = L"Status: Excellent · Fully Hardened & Optimized without AI";
    else if (score >= 70) statusDesc = L"Status: Good · Minor bloat or uncleaned caches detected";
    else statusDesc = L"Status: Action Recommended · AI features and telemetry active";

    SolidBrush descBrush(Theme::TextSecondary);
    g.DrawString(statusDesc.c_str(), -1, titleFont, PointF(textRect.X, textRect.Y + 24.0f), &descBrush);
}
