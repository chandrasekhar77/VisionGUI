#include "pch.h"
#include "framework.h"
#include "DefectSummaryPane.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

enum Col { COL_SWATCH, COL_TYPE, COL_COUNT, COL_N };

#define ID_DEFSUMMARY_LIST 1

namespace
{
    const COLORREF kSwatchColors[3] = {
        RGB(0x42, 0x85, 0xF4), // Defect Type 1 — blue
        RGB(0x9C, 0x27, 0xB0), // Defect Type 2 — purple
        RGB(0x26, 0x26, 0x26), // Defect Type 3 — near-black
    };
}

IMPLEMENT_DYNAMIC(CDefectSummaryPane, CDockablePane)

BEGIN_MESSAGE_MAP(CDefectSummaryPane, CDockablePane)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_NOTIFY(NM_CUSTOMDRAW, ID_DEFSUMMARY_LIST, &CDefectSummaryPane::OnNMCustomDraw)
END_MESSAGE_MAP()

int CDefectSummaryPane::OnCreate(LPCREATESTRUCT lp)
{
    if (CDockablePane::OnCreate(lp) == -1) return -1;

    m_list.Create(
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
        LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL | LVS_NOCOLUMNHEADER,
        CRect(0, 0, 0, 0), this, ID_DEFSUMMARY_LIST);

    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    m_list.SetBkColor(Theme::BG);
    m_list.SetTextBkColor(Theme::BG);
    m_list.SetTextColor(Theme::TEXT);

    m_list.InsertColumn(COL_SWATCH, _T(""),           LVCFMT_LEFT,  28);
    m_list.InsertColumn(COL_TYPE,   _T("Defect Type"), LVCFMT_LEFT,  120);
    m_list.InsertColumn(COL_COUNT,  _T("Count"),       LVCFMT_RIGHT, 60);

    m_font.CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, _T("Segoe UI"));
    m_boldFont.CreateFont(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, _T("Segoe UI"));
    m_list.SetFont(&m_font);

    for (int i = 0; i < kRowCount; i++)
        m_list.InsertItem(i, _T(""));

    // Placeholder counts — no real L2/L3 pipeline exists yet.
    SetCounts(12, 5, 3);

    return 0;
}

void CDefectSummaryPane::SetCounts(int c1, int c2, int c3)
{
    m_counts[0] = c1;
    m_counts[1] = c2;
    m_counts[2] = c3;
    RefreshRows();
}

void CDefectSummaryPane::RefreshRows()
{
    if (!m_list.m_hWnd) return;

    CString s;
    for (int i = 0; i < 3; i++)
    {
        s.Format(_T("Defect Type %d"), i + 1);
        m_list.SetItemText(i, COL_TYPE, s);
        s.Format(_T("%d"), m_counts[i]);
        m_list.SetItemText(i, COL_COUNT, s);
    }

    m_list.SetItemText(3, COL_TYPE, _T("Total"));
    s.Format(_T("%d"), m_counts[0] + m_counts[1] + m_counts[2]);
    m_list.SetItemText(3, COL_COUNT, s);
}

void CDefectSummaryPane::OnSize(UINT nType, int cx, int cy)
{
    CDockablePane::OnSize(nType, cx, cy);
    if (m_list.m_hWnd) m_list.MoveWindow(0, 0, cx, cy);
    PostMessage(WM_NCPAINT, 1, 0);
}

BOOL CDefectSummaryPane::OnEraseBkgnd(CDC* pDC)
{
    CRect rc; GetClientRect(&rc);
    pDC->FillSolidRect(&rc, Theme::BG);
    return TRUE;
}

void CDefectSummaryPane::OnNMCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
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
            if (row == kRowCount - 1) // Total row — bold text
            {
                ::SelectObject(pCD->nmcd.hdc, m_boldFont.GetSafeHandle());
                pCD->clrText   = Theme::TEXT;
                pCD->clrTextBk = Theme::BG;
                *pResult       = CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
            }
            else
            {
                pCD->clrText   = Theme::TEXT;
                pCD->clrTextBk = Theme::BG;
                *pResult       = CDRF_NOTIFYSUBITEMDRAW;
            }
        }
        break;

    case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
        {
            int row = static_cast<int>(pCD->nmcd.dwItemSpec);
            if (pCD->iSubItem == COL_SWATCH && row < 3)
            {
                CRect rc;
                m_list.GetSubItemRect(row, COL_SWATCH, LVIR_BOUNDS, rc);
                CDC dc;
                dc.Attach(pCD->nmcd.hdc);
                dc.FillSolidRect(&rc, Theme::BG);
                CRect swatch(rc.left + 6, rc.top + 4, rc.left + 18, rc.bottom - 4);
                dc.FillSolidRect(&swatch, kSwatchColors[row]);
                dc.Detach();
                *pResult = CDRF_SKIPDEFAULT;
            }
            else
            {
                *pResult = CDRF_DODEFAULT;
            }
        }
        break;
    }
}
