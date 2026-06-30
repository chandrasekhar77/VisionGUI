#include "pch.h"
#include "framework.h"
#include "DefectListPane.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Column indices
enum Col { COL_ID, COL_PANEL, COL_ROI, COL_TYPE, COL_SEVERITY, COL_X, COL_Y, COL_COUNT };

// ---------------------------------------------------------------------------

IMPLEMENT_DYNAMIC(CDefectListPane, CDockablePane)

BEGIN_MESSAGE_MAP(CDefectListPane, CDockablePane)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_MESSAGE(Theme::WM_DEFECT_ADDED, &CDefectListPane::OnDefectAdded)
    ON_MESSAGE(Theme::WM_DEFECT_CLEAR,  &CDefectListPane::OnDefectClear)
    ON_NOTIFY(NM_CUSTOMDRAW, 1, &CDefectListPane::OnNMCustomDraw)
    ON_NOTIFY(NM_DBLCLK,     1, &CDefectListPane::OnNMDblClk)
END_MESSAGE_MAP()

// ---------------------------------------------------------------------------
// IDefectListListener — called from worker threads
// ---------------------------------------------------------------------------

void CDefectListPane::OnDefectAdded(const DefectInfo& defect)
{
    if (!m_hWnd) return;
    auto* copy = new DefectInfo(defect);   // copied — caller's stack frame goes away
    PostMessage(Theme::WM_DEFECT_ADDED, 0, reinterpret_cast<LPARAM>(copy));
}

void CDefectListPane::OnClear()
{
    if (m_hWnd)
        PostMessage(Theme::WM_DEFECT_CLEAR, 0, 0);
}

// ---------------------------------------------------------------------------
// Creation
// ---------------------------------------------------------------------------

int CDefectListPane::OnCreate(LPCREATESTRUCT lp)
{
    if (CDockablePane::OnCreate(lp) == -1) return -1;

    m_list.Create(
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
        LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | LVS_SINGLESEL,
        CRect(0, 0, 0, 0), this, 1);

    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    m_list.SetBkColor(Theme::BG);
    m_list.SetTextBkColor(Theme::BG);
    m_list.SetTextColor(Theme::TEXT);

    m_list.InsertColumn(COL_ID,       _T("#"),        LVCFMT_RIGHT,  36);
    m_list.InsertColumn(COL_PANEL,    _T("Panel"),    LVCFMT_RIGHT,  52);
    m_list.InsertColumn(COL_ROI,      _T("ROI"),      LVCFMT_LEFT,   72);
    m_list.InsertColumn(COL_TYPE,     _T("Type"),     LVCFMT_LEFT,   80);
    m_list.InsertColumn(COL_SEVERITY, _T("Severity"), LVCFMT_LEFT,   68);
    m_list.InsertColumn(COL_X,        _T("X"),        LVCFMT_RIGHT,  48);
    m_list.InsertColumn(COL_Y,        _T("Y"),        LVCFMT_RIGHT,  48);

    m_font.CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, _T("Segoe UI"));
    m_list.SetFont(&m_font);

    m_header.SubclassWindow(m_list.GetHeaderCtrl()->GetSafeHwnd());

    // Dark scrollbars on Windows 10/11
    SetWindowTheme(m_list.GetSafeHwnd(), L"DarkMode_Explorer", nullptr);

    return 0;
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void CDefectListPane::OnSize(UINT nType, int cx, int cy)
{
    CDockablePane::OnSize(nType, cx, cy);
    if (!m_list.m_hWnd) return;

    m_list.MoveWindow(0, 0, cx, cy);
    PostMessage(WM_NCPAINT, 1, 0);
}

BOOL CDefectListPane::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);
    pDC->FillSolidRect(&rc, Theme::BG);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Message handlers — UI thread only
// ---------------------------------------------------------------------------

LRESULT CDefectListPane::OnDefectAdded(WPARAM, LPARAM lParam)
{
    auto* d = reinterpret_cast<DefectInfo*>(lParam);
    if (!d) return 0;
    AppendRow(*d);
    delete d;
    return 0;
}

LRESULT CDefectListPane::OnDefectClear(WPARAM, LPARAM)
{
    m_list.DeleteAllItems();
    return 0;
}

void CDefectListPane::AppendRow(const DefectInfo& d)
{
    CString s;
    int row = m_list.GetItemCount();

    s.Format(_T("%d"), d.defectId);
    m_list.InsertItem(row, s);

    s.Format(_T("%d"), d.panelId);
    m_list.SetItemText(row, COL_PANEL, s);

    m_list.SetItemText(row, COL_ROI,      d.roiName);
    m_list.SetItemText(row, COL_TYPE,     d.defectType);
    m_list.SetItemText(row, COL_SEVERITY, SeverityStr(d.severity));

    s.Format(_T("%d"), d.x);
    m_list.SetItemText(row, COL_X, s);

    s.Format(_T("%d"), d.y);
    m_list.SetItemText(row, COL_Y, s);

    // Store severity for custom draw and defectId for click-to-jump
    m_list.SetItemData(row, MAKELPARAM((WORD)d.severity, (WORD)d.defectId));

    m_list.EnsureVisible(row, FALSE);
}

// ---------------------------------------------------------------------------
// Custom draw — color each row by severity
// ---------------------------------------------------------------------------

COLORREF CDefectListPane::SeverityColor(DefectSeverity sev) const
{
    switch (sev) {
    case DefectSeverity::Critical: return RGB(0xEF, 0x53, 0x50); // red
    case DefectSeverity::High:     return RGB(0xFF, 0x8A, 0x65); // orange
    case DefectSeverity::Medium:   return RGB(0xFF, 0xD5, 0x4F); // yellow
    default:                       return Theme::TEXT;
    }
}

LPCTSTR CDefectListPane::SeverityStr(DefectSeverity sev) const
{
    switch (sev) {
    case DefectSeverity::Critical: return _T("Critical");
    case DefectSeverity::High:     return _T("High");
    case DefectSeverity::Medium:   return _T("Medium");
    default:                       return _T("Low");
    }
}

void CDefectListPane::OnNMCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
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
            DWORD_PTR data = m_list.GetItemData(static_cast<int>(pCD->nmcd.dwItemSpec));
            auto sev       = static_cast<DefectSeverity>(LOWORD(data));
            pCD->clrText   = SeverityColor(sev);
            pCD->clrTextBk = Theme::BG;
            *pResult       = CDRF_NEWFONT;
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// Double-click — notify parent to jump to defect in image view
// ---------------------------------------------------------------------------

void CDefectListPane::OnNMDblClk(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    int sel = m_list.GetNextItem(-1, LVNI_SELECTED);
    if (sel >= 0)
    {
        DWORD_PTR data    = m_list.GetItemData(sel);
        int       defectId = static_cast<int>(HIWORD(data));
        GetParent()->PostMessage(Theme::WM_DEFECT_SELECTED, (WPARAM)defectId, 0);
    }
    *pResult = 0;
}
