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

int CTopBar::OnCreate(LPCREATESTRUCT lpcs)
{
	if (CDockablePane::OnCreate(lpcs) == -1)
		return -1;

	m_font.CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));
	return 0;
}

// ---------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------

/*static*/ void CTopBar::GetActBtnRects(int w, CRect& rcConnect, CRect& rcStart, CRect& rcStop)
{
	int right = w - 8;
	int bw    = ACT_BTN_W;
	int h     = TOP_BAR_H;
	rcStop    = { right - bw,     0, right,      h };
	rcStart   = { right - bw * 2, 0, right - bw, h };
	rcConnect = { right - bw * 3, 0, right - bw * 2, h };
}

/*static*/ TopBtn CTopBar::HitTest(CPoint pt, int w)
{
	if (pt.y < 0 || pt.y >= TOP_BAR_H) return TOP_NONE;

	if (pt.x < NAV_BTN_W * NAV_COUNT)
		return (TopBtn)(TOP_NAV_MONITOR + pt.x / NAV_BTN_W);

	CRect rcConnect, rcStart, rcStop;
	GetActBtnRects(w, rcConnect, rcStart, rcStop);
	if (rcConnect.PtInRect(pt)) return TOP_ACT_CONNECT;
	if (rcStart.PtInRect(pt))   return TOP_ACT_START;
	if (rcStop.PtInRect(pt))    return TOP_ACT_STOP;

	return TOP_NONE;
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

	static const LPCTSTR navLabels[NAV_COUNT] = {
		_T("Monitoring"), _T("Results"), _T("Recipe"), _T("Statistics"), _T("Config")
	};

	for (int i = 0; i < NAV_COUNT; i++)
	{
		CRect rc(i * NAV_BTN_W, 0, (i + 1) * NAV_BTN_W, TOP_BAR_H);
		bool active = (i == (int)m_activeView);
		bool hover  = (m_hoverTopBtn == (TopBtn)(TOP_NAV_MONITOR + i));

		if (hover && !active)
			dc.FillSolidRect(&rc, HOVER);
		if (active)
			dc.FillSolidRect(rc.left, rc.bottom - 2, rc.Width(), 2, ACCENT);

		dc.SetTextColor(active ? TEXT : TEXT_DIM);
		dc.DrawText(navLabels[i], &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}

	// Separator after nav zone
	dc.FillSolidRect(NAV_BTN_W * NAV_COUNT, 8, 1, TOP_BAR_H - 16, SEPARATOR);

	CRect rcConnect, rcStart, rcStop;
	GetActBtnRects(w, rcConnect, rcStart, rcStop);

	// Connect
	COLORREF clrConn = m_connected ? GREEN
	                 : (m_hoverTopBtn == TOP_ACT_CONNECT ? HOVER : TOP_BG);
	dc.FillSolidRect(&rcConnect, clrConn);
	dc.SetTextColor(TEXT);
	dc.DrawText(m_connected ? _T("Disconnect") : _T("Connect"),
		&rcConnect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	// Start
	COLORREF clrStart = m_running ? ACCENT
	                  : (m_hoverTopBtn == TOP_ACT_START ? HOVER : TOP_BG);
	dc.FillSolidRect(&rcStart, clrStart);
	dc.DrawText(_T("Start"), &rcStart, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	// Stop
	dc.FillSolidRect(&rcStop, (m_hoverTopBtn == TOP_ACT_STOP && m_running) ? HOVER : TOP_BG);
	dc.SetTextColor(m_running ? TEXT : TEXT_DIM);
	dc.DrawText(_T("Stop"), &rcStop, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	dc.SelectObject(pOld);
}

BOOL CTopBar::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(&rc);
	pDC->FillSolidRect(&rc, TOP_BG);
	return TRUE;
}

void CTopBar::OnPaint()
{
	CPaintDC dc(this);
	CRect client;
	GetClientRect(&client);
	DrawBar(dc, client.Width());
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

void CTopBar::OnLButtonDown(UINT nFlags, CPoint point)
{
	CRect client;
	GetClientRect(&client);
	int w = client.Width();

	switch (HitTest(point, w))
	{
	case TOP_NAV_MONITOR: case TOP_NAV_RESULTS:
	case TOP_NAV_RECIPE:  case TOP_NAV_STATS: case TOP_NAV_CONFIG:
		m_activeView = (NavView)(HitTest(point, w) - TOP_NAV_MONITOR);
		Invalidate();
		break;
	case TOP_ACT_CONNECT:
		m_connected = !m_connected;
		if (!m_connected) m_running = false;
		Invalidate();
		break;
	case TOP_ACT_START:
		if (m_connected) { m_running = true;  Invalidate(); }
		break;
	case TOP_ACT_STOP:
		if (m_running)   { m_running = false; Invalidate(); }
		break;
	default:
		CDockablePane::OnLButtonDown(nFlags, point);
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

	CRect client;
	GetClientRect(&client);
	TopBtn prev    = m_hoverTopBtn;
	m_hoverTopBtn  = HitTest(point, client.Width());

	if (m_hoverTopBtn != prev) Invalidate();

	CDockablePane::OnMouseMove(nFlags, point);
}

void CTopBar::OnMouseLeave()
{
	m_trackingMouse = false;
	if (m_hoverTopBtn != TOP_NONE)
	{
		m_hoverTopBtn = TOP_NONE;
		Invalidate();
	}
}
