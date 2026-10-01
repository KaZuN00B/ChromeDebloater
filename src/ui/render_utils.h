#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include "theme.h"

class RenderUtils {
public:
    static void AddRoundedRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, float radius);
    static void FillRoundedRect(Gdiplus::Graphics& g, const Gdiplus::Brush& brush, const Gdiplus::RectF& rect, float radius);
    static void DrawRoundedRect(Gdiplus::Graphics& g, const Gdiplus::Pen& pen, const Gdiplus::RectF& rect, float radius);

    // Card drawing with subtle border
    static void DrawCard(Gdiplus::Graphics& g, const Gdiplus::RectF& rect, const Gdiplus::Color& bgColor, const Gdiplus::Color& borderColor, float radius = 8.0f);

    // Gradient Action Button
    static void DrawGradientButton(Gdiplus::Graphics& g, const Gdiplus::RectF& rect, const std::wstring& text, bool isHovered, bool isPressed, Gdiplus::Font* font, bool enabled = true);

    // Modern Pill Toggle Switch
    static void DrawToggle(Gdiplus::Graphics& g, const Gdiplus::RectF& rect, bool isOn, bool isHovered);

    // Status Badge
    static void DrawBadge(Gdiplus::Graphics& g, const Gdiplus::RectF& rect, const std::wstring& text, const Gdiplus::Color& bgColor, const Gdiplus::Color& textColor, Gdiplus::Font* font);

    // Health Score Circular / Pill Meter
    static void DrawScoreGauge(Gdiplus::Graphics& g, const Gdiplus::RectF& rect, int score, Gdiplus::Font* titleFont, Gdiplus::Font* numFont);
};
