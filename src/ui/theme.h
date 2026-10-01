#pragma once
#include <windows.h>
#include <gdiplus.h>

namespace Theme {
    using namespace Gdiplus;

    // Dark Palette
    const Color BgDark(255, 11, 14, 20);           // #0B0E14 Window background
    const Color BgSidebar(255, 14, 18, 26);        // #0E121A Sidebar
    const Color BgCard(255, 19, 25, 38);           // #131926 Card background
    const Color BgCardHover(255, 26, 35, 50);      // #1A2332 Card hover
    const Color BgInput(255, 10, 13, 20);          // #0A0D14 Input/console background
    const Color BorderSubtle(255, 30, 41, 59);     // #1E293B Card border
    const Color BorderActive(255, 59, 130, 246);   // #3B82F6 Active border

    // Typography Colors
    const Color TextPrimary(255, 248, 250, 252);   // #F8FAFC
    const Color TextSecondary(255, 148, 163, 184); // #94A3B8
    const Color TextMuted(255, 100, 116, 139);     // #64748B

    // Accent & Status Colors
    const Color AccentBlue(255, 59, 130, 246);     // #3B82F6
    const Color AccentBlueHover(255, 37, 99, 235); // #2563EB
    const Color AccentPurple(255, 139, 92, 246);   // #8B5CF6
    const Color SuccessGreen(255, 16, 185, 129);   // #10B981
    const Color SuccessBadge(255, 6, 78, 59);      // Dark green badge bg
    const Color WarningOrange(255, 245, 158, 11);  // #F59E0B
    const Color WarningBadge(255, 120, 53, 15);    // Dark orange badge bg
    const Color DangerRed(255, 239, 68, 68);       // #EF4444

    // GDI Native equivalents for standard controls if needed
    inline COLORREF ToCOLORREF(const Color& c) {
        return RGB(c.GetR(), c.GetG(), c.GetB());
    }
}
