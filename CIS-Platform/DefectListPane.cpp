#include "pch.h"
#include "framework.h"
#include "DefectListPane.h"
#include "Contracts/LoggerRegistry.h"
#include <shellapi.h>
#include <atlimage.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Column indices (append-only — COL_IMAGE/COL_PO added after the original set
// to minimize regression risk to the existing severity custom-draw code)
enum Col { COL_ID, COL_PANEL, COL_ROI, COL_TYPE, COL_SEVERITY, COL_X, COL_Y, COL_IMAGE, COL_PO, COL_COUNT };

#define ID_DEFECTLIST_LIST         1
#define ID_DEFECTLIST_FOLDEROPEN   2
#define ID_DEFECTLIST_PROGRAM      3
#define ID_DEFECTLIST_INSPRESULT   4

// ---------------------------------------------------------------------------

IMPLEMENT_DYNAMIC(CDefectListPane, CDockablePane)

BEGIN_MESSAGE_MAP(CDefectListPane, CDockablePane)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_MESSAGE(Theme::WM_DEFECT_ADDED, &CDefectListPane::OnDefectAdded)
    ON_MESSAGE(Theme::WM_DEFECT_CLEAR,  &CDefectListPane::OnDefectClear)
    ON_NOTIFY(NM_CUSTOMDRAW, ID_DEFECTLIST_LIST, &CDefectListPane::OnNMCustomDraw)
    ON_NOTIFY(NM_DBLCLK,     ID_DEFECTLIST_LIST, &CDefectListPane::OnNMDblClk)
    ON_NOTIFY(LVN_ITEMCHANGED, ID_DEFECTLIST_LIST, &CDefectListPane::OnListItemChanged)
    ON_BN_CLICKED(ID_DEFECTLIST_FOLDEROPEN, &CDefectListPane::OnFolderOpenClicked)
    ON_BN_CLICKED(ID_DEFECTLIST_PROGRAM,    &CDefectListPane::OnProgramClicked)
    ON_BN_CLICKED(ID_DEFECTLIST_INSPRESULT, &CDefectListPane::OnInspResultClicked)
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
        CRect(0, 0, 0, 0), this, ID_DEFECTLIST_LIST);

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
    m_list.InsertColumn(COL_IMAGE,    _T("Image"),    LVCFMT_LEFT,   48);
    m_list.InsertColumn(COL_PO,       _T("po%"),      LVCFMT_RIGHT,  50);

    m_font.CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, _T("Segoe UI"));
    m_list.SetFont(&m_font);

    // Dummy 1x40 image list — a standard MFC trick to force a taller
    // LVS_REPORT row height so the thumbnail column has room to draw into.
    m_rowHeightHack.Create(1, 40, ILC_COLOR, 1, 1);
    m_list.SetImageList(&m_rowHeightHack, LVSIL_SMALL);

    m_header.SubclassWindow(m_list.GetHeaderCtrl()->GetSafeHwnd());

    // Dark scrollbars on Windows 10/11
    SetWindowTheme(m_list.GetSafeHwnd(), L"DarkMode_Explorer", nullptr);

    DWORD btnStyle = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED;
    m_btnFolderOpen.Create(_T("Folder Open"), btnStyle, CRect(0, 0, 0, 0), this, ID_DEFECTLIST_FOLDEROPEN);
    m_btnProgram.Create   (_T("Program"),     btnStyle, CRect(0, 0, 0, 0), this, ID_DEFECTLIST_PROGRAM);
    m_btnInspResult.Create(_T("Insp Result"), btnStyle, CRect(0, 0, 0, 0), this, ID_DEFECTLIST_INSPRESULT);
    m_btnFolderOpen.SetFont(&m_font);
    m_btnProgram.SetFont(&m_font);
    m_btnInspResult.SetFont(&m_font);

    return 0;
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void CDefectListPane::OnSize(UINT nType, int cx, int cy)
{
    CDockablePane::OnSize(nType, cx, cy);
    if (!m_list.m_hWnd) return;

    constexpr int kBtnStripH = 28, kMargin = 4, kBtnW = 90;
    int listH = max(0, cy - kBtnStripH);
    m_list.MoveWindow(0, 0, cx, listH);

    int x = kMargin, y = listH + (kBtnStripH - 22) / 2;
    m_btnFolderOpen.MoveWindow(x, y, kBtnW, 22); x += kBtnW + kMargin;
    m_btnProgram.MoveWindow(x, y, kBtnW, 22);    x += kBtnW + kMargin;
    m_btnInspResult.MoveWindow(x, y, kBtnW, 22);

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
    m_thumbnails.clear();
    UpdateActionButtons();
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

    s.Format(_T("%.1f%%"), d.confidence);
    m_list.SetItemText(row, COL_PO, s);

    // Thumbnail cell keeps only the path as item text (drawn manually in
    // custom-draw); the bitmap itself is loaded once here and cached,
    // index-aligned with list rows, so paint doesn't touch the file system.
    m_list.SetItemText(row, COL_IMAGE, d.thumbnailPath);
    std::unique_ptr<CBitmap> bmp;
    if (d.thumbnailPath[0] != _T('\0'))
    {
        CImage img;
        if (SUCCEEDED(img.Load(d.thumbnailPath)))
        {
            bmp = std::make_unique<CBitmap>();
            bmp->Attach(img.Detach());
        }
    }
    m_thumbnails.push_back(std::move(bmp));

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
            *pResult       = CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
        }
        break;

    case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
        {
            int row = static_cast<int>(pCD->nmcd.dwItemSpec);
            if (pCD->iSubItem == COL_IMAGE)
            {
                CRect rc;
                m_list.GetSubItemRect(row, COL_IMAGE, LVIR_BOUNDS, rc);
                CDC dc;
                dc.Attach(pCD->nmcd.hdc);
                dc.FillSolidRect(&rc, Theme::BG);

                CBitmap* bmp = (row >= 0 && row < (int)m_thumbnails.size()) ? m_thumbnails[row].get() : nullptr;
                CRect img(rc.left + 2, rc.top + 2, rc.right - 2, rc.bottom - 2);
                if (bmp && bmp->GetSafeHandle())
                {
                    CDC memDC;
                    memDC.CreateCompatibleDC(&dc);
                    CBitmap* pOld = memDC.SelectObject(bmp);
                    BITMAP bm; bmp->GetBitmap(&bm);
                    dc.SetStretchBltMode(HALFTONE);
                    dc.StretchBlt(img.left, img.top, img.Width(), img.Height(),
                        &memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                    memDC.SelectObject(pOld);
                }
                else
                {
                    dc.Draw3dRect(&img, Theme::SEPARATOR, Theme::SEPARATOR);
                    dc.SetBkMode(TRANSPARENT);
                    dc.SetTextColor(Theme::TEXT_DIM);
                    dc.DrawText(_T("N/A"), &img, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
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

// ---------------------------------------------------------------------------
// Action buttons — enabled only with a row selected
// ---------------------------------------------------------------------------

void CDefectListPane::OnListItemChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    UpdateActionButtons();
    *pResult = 0;
}

void CDefectListPane::UpdateActionButtons()
{
    BOOL hasSel = m_list.GetNextItem(-1, LVNI_SELECTED) >= 0;
    m_btnFolderOpen.EnableWindow(hasSel);
    m_btnProgram.EnableWindow(hasSel);
    m_btnInspResult.EnableWindow(hasSel);
}

void CDefectListPane::OnFolderOpenClicked()
{
    int sel = m_list.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0) return;

    CString path = m_list.GetItemText(sel, COL_IMAGE);
    if (path.IsEmpty()) return;

    CString dir = path.Left(path.ReverseFind(_T('\\')));
    if (dir.IsEmpty()) return;

    ShellExecute(nullptr, _T("open"), dir, nullptr, nullptr, SW_SHOWNORMAL);
}

void CDefectListPane::OnProgramClicked()
{
    // Placeholder — no real recipe/program pipeline exists yet.
    VisionLog::Info(_T("Program: not yet implemented"));
}

void CDefectListPane::OnInspResultClicked()
{
    // Placeholder — no real inspection-result pipeline exists yet.
    VisionLog::Info(_T("Insp Result: not yet implemented"));
}
