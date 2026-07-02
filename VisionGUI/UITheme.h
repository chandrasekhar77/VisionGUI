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

    // Dashboard toolbar / sub-tab strip (Dashboard tab only)
    constexpr int DASH_TOOLBAR_H = 32;
    constexpr int SUBTAB_H       = 30;
    constexpr int SUBTAB_BTN_W   = 96;

    // Navigation / action enums
    enum NavView { VIEW_DASHBOARD, VIEW_RESULTS, VIEW_RECIPE, VIEW_STATS, VIEW_CONFIG };

    // Dashboard sub-tabs: Main View | Camera | Light | Inspect
    enum DashSubTab { SUBTAB_MAIN, SUBTAB_CAMERA, SUBTAB_LIGHT, SUBTAB_INSPECT };

    // Posted from CTopBar to CMainFrame when the active nav view changes (wParam = NavView)
    constexpr UINT WM_NAV_CHANGED  = WM_APP + 1;
    // Posted from CImagePanel to CTeachView when a new ROI is drawn
    constexpr UINT WM_ROI_ADDED    = WM_APP + 2;
    // Posted from CImagePanel to CTeachView when an ROI is selected/deselected (wParam = id, -1 = none)
    constexpr UINT WM_ROI_SELECTED = WM_APP + 3;
    // Posted by ILogger::Log() to CLoggingPane (wParam unused, lParam = LogMsg* — pane deletes it)
    constexpr UINT WM_LOG_MESSAGE  = WM_APP + 4;
    // Posted by L2 to CDefectListPane when a new defect is found (lParam = DefectInfo* — pane deletes it)
    constexpr UINT WM_DEFECT_ADDED = WM_APP + 5;
    // Posted by L2 to CDefectListPane to clear all rows (new panel starting)
    constexpr UINT WM_DEFECT_CLEAR = WM_APP + 6;
    // Posted by CDefectListPane to MainFrm when user clicks a row (wParam = defectId)
    constexpr UINT WM_DEFECT_SELECTED = WM_APP + 7;
    // Posted by CModelManagerPane to MainFrm when user loads a model (lParam = LPCTSTR name)
    constexpr UINT WM_MODEL_LOADED    = WM_APP + 8;
    // Posted by CDashboardTabStrip to CMonitoringView, relayed to CMainFrame (wParam = DashSubTab)
    constexpr UINT WM_SUBTAB_CHANGED  = WM_APP + 9;
    // Posted by L3 to CPanelListPane when a panel completes (lParam = PanelRecord* — pane deletes it)
    constexpr UINT WM_PANEL_ADDED     = WM_APP + 10;
    // Posted by L3 to CPanelListPane to clear all rows (new lot starting)
    constexpr UINT WM_PANEL_CLEAR     = WM_APP + 11;

    // Model Manager toolbar button IDs
    constexpr UINT ID_MODEL_LOAD      = 3000;
    constexpr UINT ID_MODEL_NEW       = 3001;
    constexpr UINT ID_MODEL_DELETE    = 3002;
    constexpr UINT ID_MODEL_DUPLICATE = 3003;
    enum TopBtn  { TOP_NONE,
                   TOP_NAV_MONITOR, TOP_NAV_RESULTS, TOP_NAV_RECIPE, TOP_NAV_STATS, TOP_NAV_CONFIG,
                   TOP_ACT_CONNECT, TOP_ACT_START, TOP_ACT_STOP,
                   TOP_WIN_MIN, TOP_WIN_MAX, TOP_WIN_CLOSE };

    // Dashboard toolbar control IDs
    constexpr UINT ID_CHAN_ORIGIN         = 3100;
    constexpr UINT ID_CHAN_RED            = 3101;
    constexpr UINT ID_CHAN_GREEN          = 3102;
    constexpr UINT ID_CHAN_BLUE           = 3103;
    constexpr UINT ID_DASH_SAVE           = 3104;
    constexpr UINT ID_DASH_DISPINIT       = 3105;
    constexpr UINT ID_DASH_LOT_EDIT       = 3106;
    constexpr UINT ID_DASH_ID_EDIT        = 3107;
    constexpr UINT ID_DASH_APPLY          = 3108;

    // Defect list pane action button IDs
    constexpr UINT ID_DEFECT_FOLDEROPEN   = 3110;
    constexpr UINT ID_DEFECT_PROGRAM      = 3111;
    constexpr UINT ID_DEFECT_INSPRESULT   = 3112;

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
