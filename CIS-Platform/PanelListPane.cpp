#include "pch.h"
#include "framework.h"
#include "PanelListPane.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Column indices
enum Col { COL_INDEX, COL_LOT, COL_ID, COL_DFCOUNT, COL_DFAREA, COL_STATUS, COL_COUNT };

#define ID_PANELLIST_CLEAR 1
#define ID_PANELLIST_LIST  2

// ---------------------------------------------------------------------------

IMPLEMENT_DYNAMIC(CPanelListPane, CDockablePane)

BEGIN_MESSAGE_MAP(CPanelListPane, CDockablePane)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_MESSAGE(Theme::WM_PANEL_ADDED, &CPanelListPane::OnPanelAdded)
    ON_MESSAGE(Theme::WM_PANEL_CLEAR, &CPanelListPane::OnPanelClear)
    ON_NOTIFY(NM_CUSTOMDRAW, ID_PANELLIST_LIST, &CPanelListPane::OnNMCustomDraw)
    ON_BN_CLICKED(ID_PANELLIST_CLEAR, &CPanelListPane::OnClearClicked)
END_MESSAGE_MAP()

// ---------------------------------------------------------------------------
// IPanelListListener — called from worker threads
// ---------------------------------------------------------------------------

void CPanelListPane::OnPanelRecorded(const PanelRecord& record)
{
    if (!m_hWnd) return;
    auto* copy = new PanelRecord(record);
    PostMessage(Theme::WM_PANEL_ADDED, 0, reinterpret_cast<LPARAM>(copy));
}

void CPanelListPane::OnPanelListClear()
{
    if (m_hWnd)
        PostMessage(Theme::WM_PANEL_CLEAR, 0, 0);
}

// ---------------------------------------------------------------------------
// Creation
// ---------------------------------------------------------------------------

int CPanelListPane::OnCreate(LPCREATESTRUCT lp)
{
    if (CDockablePane::OnCreate(lp) == -1) return -1;

    m_btnClear.Create(_T("Clear"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(0, 0, 0, 0), this, ID_PANELLIST_CLEAR);

    m_list.Create(
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
        LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | LVS_SINGLESEL,
        CRect(0, 0, 0, 0), this, ID_PANELLIST_LIST);

    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    m_list.SetBkColor(Theme::BG);
    m_list.SetTextBkColor(Theme::BG);
    m_list.SetTextColor(Theme::TEXT);

    m_list.InsertColumn(COL_INDEX,   _T("Index"),    LVCFMT_RIGHT, 44);
    m_list.InsertColumn(COL_LOT,     _T("Lot"),       LVCFMT_LEFT,  70);
    m_list.InsertColumn(COL_ID,      _T("ID"),        LVCFMT_LEFT,  70);
    m_list.InsertColumn(COL_DFCOUNT, _T("Df Count"),  LVCFMT_RIGHT, 60);
    m_list.InsertColumn(COL_DFAREA,  _T("Df Area"),   LVCFMT_RIGHT, 60);
    m_list.InsertColumn(COL_STATUS,  _T("Status"),    LVCFMT_LEFT,  56);

    m_font.CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, _T("Segoe UI"));
    m_list.SetFont(&m_font);
    m_btnClear.SetFont(&m_font);

    m_header.SubclassWindow(m_list.GetHeaderCtrl()->GetSafeHwnd());
    SetWindowTheme(m_list.GetSafeHwnd(), L"DarkMode_Explorer", nullptr);

    // Seed placeholder rows — no real L3 pipeline exists yet.
    static const TCHAR* kLots[] = { _T("20260514"), _T("20260514"), _T("20260514"), _T("20260514"), _T("20260514"), _T("20260514") };
    static const TCHAR* kIds[]  = { _T("141453"), _T("141527"), _T("141546"), _T("143602"), _T("144629"), _T("145701") };
    const int kDfCounts[] = { 2, 1, 0, 0, 2, 1 };
    const double kDfAreas[] = { 14, 13, 0, 0, 14, 8 };
    const PanelStatus kStatus[] = { PanelStatus::OK, PanelStatus::OK, PanelStatus::OK, PanelStatus::OK, PanelStatus::NG, PanelStatus::OK };

    for (int i = 0; i < 6; i++)
    {
        PanelRecord r{};
        r.index = i;
        _tcscpy_s(r.lot, kLots[i]);
        _tcscpy_s(r.id, kIds[i]);
        r.defectCount = kDfCounts[i];
        r.defectArea  = kDfAreas[i];
        r.status      = kStatus[i];
        AppendRow(r);
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void CPanelListPane::OnSize(UINT nType, int cx, int cy)
{
    CDockablePane::OnSize(nType, cx, cy);
    if (!m_list.m_hWnd) return;

    constexpr int kBtnH = 24, kMargin = 4;
    m_btnClear.MoveWindow(kMargin, kMargin, 70, kBtnH);
    m_list.MoveWindow(0, kBtnH + kMargin * 2, cx, cy - kBtnH - kMargin * 2);
    PostMessage(WM_NCPAINT, 1, 0);
}

BOOL CPanelListPane::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);
    pDC->FillSolidRect(&rc, Theme::BG);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Message handlers — UI thread only
// ---------------------------------------------------------------------------

LRESULT CPanelListPane::OnPanelAdded(WPARAM, LPARAM lParam)
{
    auto* r = reinterpret_cast<PanelRecord*>(lParam);
    if (!r) return 0;
    AppendRow(*r);
    delete r;
    return 0;
}

LRESULT CPanelListPane::OnPanelClear(WPARAM, LPARAM)
{
    m_list.DeleteAllItems();
    return 0;
}

void CPanelListPane::OnClearClicked()
{
    m_list.DeleteAllItems();
}

void CPanelListPane::AppendRow(const PanelRecord& r)
{
    CString s;
    int row = m_list.GetItemCount();

    s.Format(_T("%d"), r.index);
    m_list.InsertItem(row, s);

    m_list.SetItemText(row, COL_LOT, r.lot);
    m_list.SetItemText(row, COL_ID,  r.id);

    s.Format(_T("%d"), r.defectCount);
    m_list.SetItemText(row, COL_DFCOUNT, s);

    s.Format(_T("%.0f"), r.defectArea);
    m_list.SetItemText(row, COL_DFAREA, s);

    m_list.SetItemText(row, COL_STATUS, StatusStr(r.status));
    m_list.SetItemData(row, (DWORD_PTR)r.status);
    m_list.EnsureVisible(row, FALSE);
}

COLORREF CPanelListPane::StatusColor(PanelStatus s) const
{
    switch (s)
    {
    case PanelStatus::OK:  return Theme::GREEN;
    case PanelStatus::NG:  return RGB(0xEF, 0x53, 0x50);
    default:                return Theme::TEXT_DIM;
    }
}

LPCTSTR CPanelListPane::StatusStr(PanelStatus s) const
{
    switch (s)
    {
    case PanelStatus::OK:  return _T("OK");
    case PanelStatus::NG:  return _T("NG");
    default:                return _T("Pending");
    }
}

void CPanelListPane::OnNMCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
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
            auto status = static_cast<PanelStatus>(m_list.GetItemData(static_cast<int>(pCD->nmcd.dwItemSpec)));
            pCD->clrText   = StatusColor(status);
            pCD->clrTextBk = Theme::BG;
            *pResult       = CDRF_NEWFONT;
        }
        break;
    }
}
