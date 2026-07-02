#include "pch.h"
#include "framework.h"
#include "DashboardTabStrip.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace Theme;

BEGIN_MESSAGE_MAP(CDashboardTabStrip, CWnd)
    ON_WM_CREATE()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

/*static*/ LPCTSTR CDashboardTabStrip::TabLabel(int idx)
{
    static const LPCTSTR labels[] = { _T("Main View"), _T("Camera"), _T("Light"), _T("Inspect") };
    return (idx >= 0 && idx < kTabCount) ? labels[idx] : _T("");
}

BOOL CDashboardTabStrip::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CWnd::PreCreateWindow(cs))
        return FALSE;

    cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
    cs.style     &= ~WS_BORDER;
    cs.style     |= WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
        ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)::GetStockObject(NULL_BRUSH), nullptr);

    return TRUE;
}

BOOL CDashboardTabStrip::Create(CWnd* pParent, UINT nID)
{
    return CWnd::Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE,
        CRect(0, 0, 0, 0), pParent, nID);
}

int CDashboardTabStrip::OnCreate(LPCREATESTRUCT lpcs)
{
    if (CWnd::OnCreate(lpcs) == -1) return -1;

    m_font.CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));
    return 0;
}

void CDashboardTabStrip::SetActiveTab(int idx)
{
    if (idx < 0 || idx >= kTabCount || idx == m_activeTab) return;
    m_activeTab = idx;
    Invalidate();
}

int CDashboardTabStrip::HitTest(CPoint pt) const
{
    CRect client; GetClientRect(&client);
    if (pt.y < 0 || pt.y >= client.Height()) return -1;
    int idx = pt.x / SUBTAB_BTN_W;
    return (idx >= 0 && idx < kTabCount) ? idx : -1;
}

static void FillPill(CDC& dc, const CRect& rc, COLORREF color)
{
    using namespace Gdiplus;
    Graphics g(dc.GetSafeHdc());
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    REAL x = (REAL)rc.left,  y = (REAL)rc.top;
    REAL w = (REAL)rc.Width(), h = (REAL)rc.Height();
    REAL d = h;

    GraphicsPath path;
    path.AddArc(x,         y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y, d, d,   0, 90);
    path.AddArc(x,         y, d, d,  90, 90);
    path.CloseFigure();

    SolidBrush brush(Gdiplus::Color(GetRValue(color), GetGValue(color), GetBValue(color)));
    g.FillPath(&brush, &path);
}

void CDashboardTabStrip::OnPaint()
{
    CPaintDC dc(this);
    CRect client; GetClientRect(&client);

    CDC     memDC;
    CBitmap bmp;
    memDC.CreateCompatibleDC(&dc);
    bmp.CreateCompatibleBitmap(&dc, client.Width(), client.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    memDC.FillSolidRect(&client, TOP_BG);
    memDC.FillSolidRect(0, client.Height() - 1, client.Width(), 1, SEPARATOR);

    CFont* pOldFont = memDC.SelectObject(&m_font);
    memDC.SetBkMode(TRANSPARENT);

    for (int i = 0; i < kTabCount; i++)
    {
        CRect rc(i * SUBTAB_BTN_W, 0, (i + 1) * SUBTAB_BTN_W, client.Height());
        bool active = (i == m_activeTab);
        bool hover  = (i == m_hoverTab);

        if (active || hover)
        {
            CRect rcPill(rc.left + 6, rc.top + 4, rc.right - 6, rc.bottom - 5);
            FillPill(memDC, rcPill, active ? ACCENT : HOVER);
        }

        memDC.SetTextColor(active || hover ? TEXT : TEXT_DIM);
        memDC.DrawText(TabLabel(i), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    memDC.SelectObject(pOldFont);
    dc.BitBlt(0, 0, client.Width(), client.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

BOOL CDashboardTabStrip::OnEraseBkgnd(CDC* pDC)
{
    CRect rc; GetClientRect(&rc);
    pDC->FillSolidRect(&rc, TOP_BG);
    return TRUE;
}

void CDashboardTabStrip::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
    int idx = HitTest(point);
    if (idx >= 0 && idx != m_activeTab)
    {
        m_activeTab = idx;
        Invalidate();
        GetParent()->PostMessage(Theme::WM_SUBTAB_CHANGED, (WPARAM)m_activeTab, 0);
    }
}

void CDashboardTabStrip::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_trackingMouse)
    {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
        TrackMouseEvent(&tme);
        m_trackingMouse = true;
    }

    int prev = m_hoverTab;
    m_hoverTab = HitTest(point);
    if (m_hoverTab != prev) Invalidate();

    CWnd::OnMouseMove(nFlags, point);
}

void CDashboardTabStrip::OnMouseLeave()
{
    m_trackingMouse = false;
    if (m_hoverTab != -1) { m_hoverTab = -1; Invalidate(); }
}
