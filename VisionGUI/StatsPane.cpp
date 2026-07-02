#include "pch.h"
#include "framework.h"
#include "StatsPane.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

enum Col { COL_LABEL, COL_VALUE, COL_N };
enum Row { ROW_TOTAL, ROW_OK, ROW_NG, ROW_DATE, ROW_OKPCT, ROW_NGPCT };

#define ID_STATS_LIST 1

IMPLEMENT_DYNAMIC(CStatsPane, CDockablePane)

BEGIN_MESSAGE_MAP(CStatsPane, CDockablePane)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_NOTIFY(NM_CUSTOMDRAW, ID_STATS_LIST, &CStatsPane::OnNMCustomDraw)
END_MESSAGE_MAP()

int CStatsPane::OnCreate(LPCREATESTRUCT lp)
{
    if (CDockablePane::OnCreate(lp) == -1) return -1;

    m_list.Create(
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
        LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL | LVS_NOCOLUMNHEADER,
        CRect(0, 0, 0, 0), this, ID_STATS_LIST);

    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    m_list.SetBkColor(Theme::BG);
    m_list.SetTextBkColor(Theme::BG);
    m_list.SetTextColor(Theme::TEXT);

    m_list.InsertColumn(COL_LABEL, _T("Label"), LVCFMT_LEFT,  90);
    m_list.InsertColumn(COL_VALUE, _T("Value"), LVCFMT_RIGHT, 90);

    m_font.CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, _T("Segoe UI"));
    m_list.SetFont(&m_font);

    static const LPCTSTR labels[kRowCount] = { _T("Total"), _T("OK"), _T("NG"), _T("Date"), _T("OK %"), _T("NG %") };
    for (int i = 0; i < kRowCount; i++)
    {
        m_list.InsertItem(i, labels[i]);
    }

    // Placeholder values — no real L3 pipeline exists yet.
    CTime now = CTime::GetCurrentTime();
    SetStats(150, 142, 8, now.Format(_T("%Y-%m-%d")));

    return 0;
}

void CStatsPane::SetStats(int total, int ok, int ng, const CString& date)
{
    if (!m_list.m_hWnd) return;

    CString s;
    double okPct = total > 0 ? (100.0 * ok / total) : 0.0;
    double ngPct = total > 0 ? (100.0 * ng / total) : 0.0;

    s.Format(_T("%d"), total); m_list.SetItemText(ROW_TOTAL, COL_VALUE, s);
    s.Format(_T("%d"), ok);    m_list.SetItemText(ROW_OK,    COL_VALUE, s);
    s.Format(_T("%d"), ng);    m_list.SetItemText(ROW_NG,    COL_VALUE, s);
    m_list.SetItemText(ROW_DATE, COL_VALUE, date);
    s.Format(_T("%.1f %%"), okPct); m_list.SetItemText(ROW_OKPCT, COL_VALUE, s);
    s.Format(_T("%.1f %%"), ngPct); m_list.SetItemText(ROW_NGPCT, COL_VALUE, s);
}

void CStatsPane::OnSize(UINT nType, int cx, int cy)
{
    CDockablePane::OnSize(nType, cx, cy);
    if (m_list.m_hWnd) m_list.MoveWindow(0, 0, cx, cy);
    PostMessage(WM_NCPAINT, 1, 0);
}

BOOL CStatsPane::OnEraseBkgnd(CDC* pDC)
{
    CRect rc; GetClientRect(&rc);
    pDC->FillSolidRect(&rc, Theme::BG);
    return TRUE;
}

void CStatsPane::OnNMCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto* pCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
    *pResult  = CDRF_DODEFAULT;

    switch (pCD->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        *pResult = CDRF_NOTIFYITEMDRAW;
        break;

    case CDDS_ITEMPREPAINT:
        {
            int row = static_cast<int>(pCD->nmcd.dwItemSpec);
            pCD->clrTextBk = Theme::BG;
            pCD->clrText   = (row == ROW_OK || row == ROW_OKPCT) ? Theme::GREEN
                            : (row == ROW_NG || row == ROW_NGPCT) ? RGB(0xEF, 0x53, 0x50)
                            : Theme::TEXT;
            *pResult = CDRF_NEWFONT;
        }
        break;
    }
}
