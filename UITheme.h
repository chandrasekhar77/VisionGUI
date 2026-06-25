#pragma once

namespace Theme {
    // Window overlay buttons
    constexpr int TITLE_H      = 32;
    constexpr int BTN_W        = 46;

    // Top bar
    constexpr int TOP_BAR_H    = 36;
    constexpr int NAV_BTN_W    = 100;
    constexpr int ACT_BTN_W    = 76;
    constexpr int NAV_COUNT    = 5;

    // Navigation / action enums
    enum NavView { VIEW_MONITORING, VIEW_RESULTS, VIEW_RECIPE, VIEW_STATS, VIEW_CONFIG };
    enum TopBtn  { TOP_NONE,
                   TOP_NAV_MONITOR, TOP_NAV_RESULTS, TOP_NAV_RECIPE, TOP_NAV_STATS, TOP_NAV_CONFIG,
                   TOP_ACT_CONNECT, TOP_ACT_START, TOP_ACT_STOP,
                   TOP_WIN_MIN, TOP_WIN_MAX, TOP_WIN_CLOSE };

    // Colors
    constexpr COLORREF BG          = RGB(0x1E, 0x1E, 0x1E);
    constexpr COLORREF TOP_BG      = RGB(0x25, 0x25, 0x26);
    constexpr COLORREF TEXT        = RGB(0xD4, 0xD4, 0xD4);
    constexpr COLORREF TEXT_DIM    = RGB(0x9D, 0x9D, 0x9D);
    constexpr COLORREF ACCENT      = RGB(0x00, 0x7A, 0xCC);
    constexpr COLORREF HOVER       = RGB(0x40, 0x40, 0x40);
    constexpr COLORREF SEPARATOR   = RGB(0x3A, 0x3A, 0x3A);
    constexpr COLORREF GREEN       = RGB(0x3C, 0x8A, 0x3C);
    constexpr COLORREF CLOSE_HOVER = RGB(0xC4, 0x2B, 0x1C);
}
