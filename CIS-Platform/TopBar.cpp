#include "pch.h"
#include "framework.h"
#include "TopBar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace Theme;

IMPLEMENT_DYNAMIC(CTopBar, CDockablePane)

BEGIN_MESSAGE_MAP(CTopBar, CDockablePane)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

int CTopBar::OnCreate(LPCREATESTRUCT lpcs)
{
	if (CDockablePane::OnCreate(lpcs) == -1) return -1;

	m_font.CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));
	return 0;
}

void CTopBar::SetModule(IVisionModule* pModule)
{
	m_pModule   = pModule;
	m_activeNav = 0;
	m_hoverBtn  = TOP_NONE;
	Invalidate();
}

// ---------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------

/*static*/ void CTopBar::GetWinBtnRects(int w, CRect& rcClose, CRect& rcMax, CRect& rcMin)
{
	rcClose = { w - BTN_W,     0, w,          TOP_BAR_H };
	rcMax   = { w - BTN_W * 2, 0, w - BTN_W,  TOP_BAR_H };
	rcMin   = { w - BTN_W * 3, 0, w - BTN_W * 2, TOP_BAR_H };
}

/*static*/ void CTopBar::GetActBtnRects(int w, CRect& rcConnect, CRect& rcStart, CRect& rcStop)
{
	int right = w - BTN_W * 3 - 8;
	rcStop    = { right - ACT_BTN_W,     0, right,            TOP_BAR_H };
	rcStart   = { right - ACT_BTN_W * 2, 0, right - ACT_BTN_W, TOP_BAR_H };
	rcConnect = { right - ACT_BTN_W * 3, 0, right - ACT_BTN_W * 2, TOP_BAR_H };
}

Theme::TopBtn CTopBar::HitTest(CPoint pt, int w) const
{
	if (pt.y < 0 || pt.y >= TOP_BAR_H) return TOP_NONE;

	// Window buttons (far right)
	CRect rcClose, rcMax, rcMin;
	GetWinBtnRects(w, rcClose, rcMax, rcMin);
	if (rcClose.PtInRect(pt)) return TOP_WIN_CLOSE;
	if (rcMax.PtInRect(pt))   return TOP_WIN_MAX;
	if (rcMin.PtInRect(pt))   return TOP_WIN_MIN;

	// Action buttons
	CRect rcConnect, rcStart, rcStop;
	GetActBtnRects(w, rcConnect, rcStart, rcStop);
	if (rcConnect.PtInRect(pt)) return TOP_ACT_CONNECT;
	if (rcStart.PtInRect(pt))   return TOP_ACT_START;
	if (rcStop.PtInRect(pt))    return TOP_ACT_STOP;

	// Nav buttons — count comes from the loaded module
	int navCount = m_pModule ? m_pModule->GetNavCount() : 0;
	if (pt.x < NAV_BTN_W * navCount)
		return (TopBtn)(TOP_NAV_MONITOR + pt.x / NAV_BTN_W);

	return TOP_NONE;
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

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

static void DrawWinBtn(CDC& dc, const CRect& rc, Theme::TopBtn type, bool hover, bool zoomed)
{
	dc.FillSolidRect(&rc, hover ? (type == TOP_WIN_CLOSE ? CLOSE_HOVER : HOVER) : TOP_BG);

	int cx = rc.left + rc.Width() / 2;
	int cy = rc.top  + rc.Height() / 2;

	CPen pen(PS_SOLID, 1, TEXT);
	CPen*   pOldPen = dc.SelectObject(&pen);
	CBrush* pOldBr  = (CBrush*)dc.SelectStockObject(NULL_BRUSH);

	switch (type)
	{
	case TOP_WIN_CLOSE:
		dc.MoveTo(cx - 5, cy - 5); dc.LineTo(cx + 6, cy + 6);
		dc.MoveTo(cx + 5, cy - 5); dc.LineTo(cx - 6, cy + 6);
		break;
	case TOP_WIN_MAX:
		if (zoomed) {
			dc.MoveTo(cx,     cy - 5); dc.LineTo(cx + 5, cy - 5);
			dc.LineTo(cx + 5, cy + 1); dc.LineTo(cx + 2, cy + 1);
			dc.MoveTo(cx,     cy - 5); dc.LineTo(cx,     cy - 2);
			dc.Rectangle(cx - 5, cy - 2, cx + 3, cy + 6);
		} else {
			dc.Rectangle(cx - 5, cy - 4, cx + 6, cy + 5);
		}
		break;
	case TOP_WIN_MIN:
		dc.MoveTo(cx - 5, cy + 1); dc.LineTo(cx + 6, cy + 1);
		break;
	default: break;
	}

	dc.SelectObject(pOldBr);
	dc.SelectObject(pOldPen);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void CTopBar::DrawBar(CDC& dc, int w)
{
	dc.FillSolidRect(0, 0, w, TOP_BAR_H, TOP_BG);
	dc.FillSolidRect(0, TOP_BAR_H - 1, w, 1, SEPARATOR);

	CFont* pOld = dc.SelectObject(&m_font);
	dc.SetBkMode(TRANSPARENT);

	// Nav tabs — labels and count from the active module
	int navCount = m_pModule ? m_pModule->GetNavCount() : 0;
	for (int i = 0; i < navCount; i++)
	{
		CRect rc(i * NAV_BTN_W, 0, (i + 1) * NAV_BTN_W, TOP_BAR_H);
		bool active = (i == m_activeNav);
		bool hover  = (m_hoverBtn == (TopBtn)(TOP_NAV_MONITOR + i));

		if (active || hover)
		{
			CRect rcPill(rc.left + 8, rc.top + 5, rc.right - 8, rc.bottom - 5);
			FillPill(dc, rcPill, active ? ACCENT : HOVER);
		}

		dc.SetTextColor(active || hover ? TEXT : TEXT_DIM);
		LPCTSTR label = m_pModule->GetNavLabel(i);
		dc.DrawText(label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}

	// Separator after nav zone
	if (navCount > 0)
		dc.FillSolidRect(NAV_BTN_W * navCount, 8, 1, TOP_BAR_H - 16, SEPARATOR);

	// Action buttons — state from module
	bool connected = m_pModule && m_pModule->IsConnected();
	bool running   = m_pModule && m_pModule->IsRunning();

	CRect rcConnect, rcStart, rcStop;
	GetActBtnRects(w, rcConnect, rcStart, rcStop);

	COLORREF clrConn = connected ? GREEN : (m_hoverBtn == TOP_ACT_CONNECT ? HOVER : TOP_BG);
	dc.FillSolidRect(&rcConnect, clrConn);
	dc.SetTextColor(TEXT);
	dc.DrawText(connected ? _T("Disconnect") : _T("Connect"),
		&rcConnect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	COLORREF clrStart = running ? ACCENT : (m_hoverBtn == TOP_ACT_START ? HOVER : TOP_BG);
	dc.FillSolidRect(&rcStart, clrStart);
	dc.DrawText(_T("Start"), &rcStart, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	dc.FillSolidRect(&rcStop, (m_hoverBtn == TOP_ACT_STOP && running) ? HOVER : TOP_BG);
	dc.SetTextColor(running ? TEXT : TEXT_DIM);
	dc.DrawText(_T("Stop"), &rcStop, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	// Separator before window buttons
	dc.FillSolidRect(w - BTN_W * 3 - 1, 8, 1, TOP_BAR_H - 16, SEPARATOR);

	// Window control buttons
	bool zoomed = GetParentFrame() && GetParentFrame()->IsZoomed();
	CRect rcClose, rcMax, rcMin;
	GetWinBtnRects(w, rcClose, rcMax, rcMin);
	DrawWinBtn(dc, rcClose, TOP_WIN_CLOSE, m_hoverBtn == TOP_WIN_CLOSE, false);
	DrawWinBtn(dc, rcMax,   TOP_WIN_MAX,   m_hoverBtn == TOP_WIN_MAX,   zoomed);
	DrawWinBtn(dc, rcMin,   TOP_WIN_MIN,   m_hoverBtn == TOP_WIN_MIN,   false);

	dc.SelectObject(pOld);
}

BOOL CTopBar::OnEraseBkgnd(CDC* pDC)
{
	CRect rc; GetClientRect(&rc);
	pDC->FillSolidRect(&rc, TOP_BG);
	return TRUE;
}

void CTopBar::OnPaint()
{
	CPaintDC dc(this);
	CRect client; GetClientRect(&client);

	CDC     memDC;
	CBitmap bmp;
	memDC.CreateCompatibleDC(&dc);
	bmp.CreateCompatibleBitmap(&dc, client.Width(), client.Height());
	CBitmap* pOldBmp = memDC.SelectObject(&bmp);

	DrawBar(memDC, client.Width());

	dc.BitBlt(0, 0, client.Width(), client.Height(), &memDC, 0, 0, SRCCOPY);
	memDC.SelectObject(pOldBmp);
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

void CTopBar::OnLButtonDown(UINT nFlags, CPoint point)
{
	CRect client; GetClientRect(&client);
	int w = client.Width();

	switch (HitTest(point, w))
	{
	case TOP_NAV_MONITOR: case TOP_NAV_RESULTS:
	case TOP_NAV_RECIPE:  case TOP_NAV_STATS: case TOP_NAV_CONFIG:
	{
		int navIndex = point.x / NAV_BTN_W;
		if (m_pModule && navIndex < m_pModule->GetNavCount())
		{
			m_activeNav = navIndex;
			Invalidate();
			GetParentFrame()->PostMessage(Theme::WM_NAV_CHANGED, (WPARAM)m_activeNav, 0);
		}
		break;
	}
	case TOP_ACT_CONNECT:
		if (m_pModule) { m_pModule->OnConnect(); Invalidate(); }
		break;
	case TOP_ACT_START:
		if (m_pModule) { m_pModule->OnStart(); Invalidate(); }
		break;
	case TOP_ACT_STOP:
		if (m_pModule) { m_pModule->OnStop(); Invalidate(); }
		break;
	case TOP_WIN_CLOSE:
		GetParentFrame()->PostMessage(WM_CLOSE);
		break;
	case TOP_WIN_MAX:
	{
		CFrameWnd* pFrame = GetParentFrame();
		pFrame->ShowWindow(pFrame->IsZoomed() ? SW_RESTORE : SW_MAXIMIZE);
		break;
	}
	case TOP_WIN_MIN:
		GetParentFrame()->ShowWindow(SW_MINIMIZE);
		break;
	default:
		ClientToScreen(&point);
		ReleaseCapture();
		GetParentFrame()->PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
		break;
	}
}

void CTopBar::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_trackingMouse)
	{
		TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
		TrackMouseEvent(&tme);
		m_trackingMouse = true;
	}

	CRect client; GetClientRect(&client);
	TopBtn prev = m_hoverBtn;
	m_hoverBtn  = HitTest(point, client.Width());
	if (m_hoverBtn != prev) Invalidate();

	CDockablePane::OnMouseMove(nFlags, point);
}

void CTopBar::OnMouseLeave()
{
	m_trackingMouse = false;
	if (m_hoverBtn != TOP_NONE) { m_hoverBtn = TOP_NONE; Invalidate(); }
}
