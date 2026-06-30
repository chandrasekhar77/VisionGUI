#include "pch.h"
#include "framework.h"
#include "LoggingPane.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace {

struct LogMsg { LogLevel level; CString time; CString text; };

COLORREF LevelColor(LogLevel lv)
{
    switch (lv) {
    case LogLevel::Warning: return RGB(0xFF, 0xD5, 0x4F);
    case LogLevel::Error:   return RGB(0xEF, 0x53, 0x50);
    case LogLevel::Debug:   return RGB(0x9D, 0x9D, 0x9D);
    default:                return Theme::TEXT;
    }
}

LPCTSTR LevelStr(LogLevel lv)
{
    switch (lv) {
    case LogLevel::Debug:   return _T("DEBUG");
    case LogLevel::Warning: return _T("WARN ");
    case LogLevel::Error:   return _T("ERROR");
    default:                return _T("INFO ");
    }
}

} // namespace

// ---------------------------------------------------------------------------

IMPLEMENT_DYNAMIC(CLoggingPane, CDockablePane)

BEGIN_MESSAGE_MAP(CLoggingPane, CDockablePane)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_MESSAGE(Theme::WM_LOG_MESSAGE, &CLoggingPane::OnLogMessage)
END_MESSAGE_MAP()

// ---------------------------------------------------------------------------
// ILogger — called from any thread
// ---------------------------------------------------------------------------

void CLoggingPane::Log(LogLevel level, LPCTSTR message)
{
    if (!m_hWnd) return;

    SYSTEMTIME st;
    GetLocalTime(&st);

    auto* msg  = new LogMsg;
    msg->level = level;
    msg->time.Format(_T("%02d:%02d:%02d"), st.wHour, st.wMinute, st.wSecond);
    msg->text  = message;

    PostMessage(Theme::WM_LOG_MESSAGE, 0, reinterpret_cast<LPARAM>(msg));
}

void CLoggingPane::Clear()
{
    if (m_richEdit.m_hWnd)
        m_richEdit.SetWindowText(_T(""));
}

// ---------------------------------------------------------------------------
// Creation
// ---------------------------------------------------------------------------

int CLoggingPane::OnCreate(LPCREATESTRUCT lp)
{
    if (CDockablePane::OnCreate(lp) == -1) return -1;

    m_richEdit.Create(
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_VSCROLL |
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_NOHIDESEL,
        CRect(0, 0, 0, 0), this, 1);

    m_richEdit.SetBackgroundColor(FALSE, Theme::BG);

    // Default character format — Consolas 9pt
    CHARFORMAT2 cf  = {};
    cf.cbSize       = sizeof(cf);
    cf.dwMask       = CFM_COLOR | CFM_FACE | CFM_SIZE | CFM_BOLD;
    cf.crTextColor  = Theme::TEXT;
    cf.yHeight      = 180;   // 9pt in twips (1pt = 20 twips)
    cf.dwEffects    = 0;
    _tcscpy_s(cf.szFaceName, _T("Consolas"));
    m_richEdit.SetDefaultCharFormat(cf);

    return 0;
}

// ---------------------------------------------------------------------------
// Layout — offset below pane caption
// ---------------------------------------------------------------------------

void CLoggingPane::OnSize(UINT nType, int cx, int cy)
{
    CDockablePane::OnSize(nType, cx, cy);
    if (!m_richEdit.m_hWnd) return;

    m_richEdit.MoveWindow(0, 0, cx, cy);

    // SWP_FRAMECHANGED sends WM_NCCALCSIZE which marks the entire NC frame
    // (including the caption) dirty before WM_NCPAINT fires, so GetWindowDC
    // is not pre-clipped to just the bottom-border NC update region.
    static bool bInFrameUpdate = false;
    if (!bInFrameUpdate)
    {
        bInFrameUpdate = true;
        SetWindowPos(nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        bInFrameUpdate = false;
    }
}

BOOL CLoggingPane::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);
    pDC->FillSolidRect(&rc, Theme::BG);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Message handler — UI thread only
// ---------------------------------------------------------------------------

LRESULT CLoggingPane::OnLogMessage(WPARAM, LPARAM lParam)
{
    auto* msg = reinterpret_cast<LogMsg*>(lParam);
    if (!msg) return 0;

    CString line;
    line.Format(_T("[%s]  %-5s  %s\r\n"),
        (LPCTSTR)msg->time, LevelStr(msg->level), (LPCTSTR)msg->text);

    // Move caret to end
    long len = m_richEdit.GetTextLength();
    m_richEdit.SetSel(len, len);

    // Set colour for this line
    CHARFORMAT2 cf  = {};
    cf.cbSize       = sizeof(cf);
    cf.dwMask       = CFM_COLOR;
    cf.crTextColor  = LevelColor(msg->level);
    m_richEdit.SetSelectionCharFormat(cf);

    m_richEdit.ReplaceSel(line);

    // Scroll to bottom
    m_richEdit.SendMessage(WM_VSCROLL, SB_BOTTOM, 0);

    delete msg;
    return 0;
}
