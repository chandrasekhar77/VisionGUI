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

// Window control buttons sit at the far right edge
/*static*/ void CTopBar::GetWinBtnRects(int w, CRect& rcClose, CRect& rcMax, CRect& rcMin)
{
	rcClose = { w - BTN_W,     0, w,          TOP_BAR_H };
	rcMax   = { w - BTN_W * 2, 0, w - BTN_W,  TOP_BAR_H };
	rcMin   = { w - BTN_W * 3, 0, w - BTN_W * 2, TOP_BAR_H };
}

// Action buttons sit just to the left of the window buttons
/*static*/ void CTopBar::GetActBtnRects(int w, CRect& rcConnect, CRect& rcStart, CRect& rcStop)
{
	int right = w - BTN_W * 3 - 8;
	int bw    = ACT_BTN_W;
	rcStop    = { right - bw,     0, right,      TOP_BAR_H };
	rcStart   = { right - bw * 2, 0, right - bw, TOP_BAR_H };
	rcConnect = { right - bw * 3, 0, right - bw * 2, TOP_BAR_H };
}

/*static*/ TopBtn CTopBar::HitTest(CPoint pt, int w)
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

	// Nav buttons
	if (pt.x < NAV_BTN_W * NAV_COUNT)
		return (TopBtn)(TOP_NAV_MONITOR + pt.x / NAV_BTN_W);

	return TOP_NONE;
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static void DrawWinBtn(CDC& dc, const CRect& rc, Theme::TopBtn type, bool hover, bool zoomed)
{
	COLORREF bg = hover
		? (type == TOP_WIN_CLOSE ? CLOSE_HOVER : HOVER)
		: TOP_BG;
	dc.FillSolidRect(&rc, bg);

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
		if (zoomed)
		{
			dc.MoveTo(cx,     cy - 5); dc.LineTo(cx + 5, cy - 5);
			dc.LineTo(cx + 5, cy + 1); dc.LineTo(cx + 2, cy + 1);
			dc.MoveTo(cx,     cy - 5); dc.LineTo(cx,     cy - 2);
			dc.Rectangle(cx - 5, cy - 2, cx + 3, cy + 6);
		}
		else
		{
			dc.Rectangle(cx - 5, cy - 4, cx + 6, cy + 5);
		}
		break;
	case TOP_WIN_MIN:
		dc.MoveTo(cx - 5, cy + 1); dc.LineTo(cx + 6, cy + 1);
		break;
	default:
		break;
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

	// Nav buttons
	static const LPCTSTR navLabels[NAV_COUNT] = {
		_T("Monitoring"), _T("Results"), _T("Recipe"), _T("Statistics"), _T("Config")
	};
	for (int i = 0; i < NAV_COUNT; i++)
	{
		CRect rc(i * NAV_BTN_W, 0, (i + 1) * NAV_BTN_W, TOP_BAR_H);
		bool active = (i == (int)m_activeView);
		bool hover  = (m_hoverBtn == (TopBtn)(TOP_NAV_MONITOR + i));

		if (hover && !active)
			dc.FillSolidRect(&rc, HOVER);
		if (active)
			dc.FillSolidRect(rc.left, rc.bottom - 2, rc.Width(), 2, ACCENT);

		dc.SetTextColor(active ? TEXT : TEXT_DIM);
		dc.DrawText(navLabels[i], &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}

	// Separator after nav zone
	dc.FillSolidRect(NAV_BTN_W * NAV_COUNT, 8, 1, TOP_BAR_H - 16, SEPARATOR);

	// Action buttons
	CRect rcConnect, rcStart, rcStop;
	GetActBtnRects(w, rcConnect, rcStart, rcStop);

	COLORREF clrConn = m_connected ? GREEN
	                 : (m_hoverBtn == TOP_ACT_CONNECT ? HOVER : TOP_BG);
	dc.FillSolidRect(&rcConnect, clrConn);
	dc.SetTextColor(TEXT);
	dc.DrawText(m_connected ? _T("Disconnect") : _T("Connect"),
		&rcConnect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	COLORREF clrStart = m_running ? ACCENT
	                  : (m_hoverBtn == TOP_ACT_START ? HOVER : TOP_BG);
	dc.FillSolidRect(&rcStart, clrStart);
	dc.DrawText(_T("Start"), &rcStart, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	dc.FillSolidRect(&rcStop, (m_hoverBtn == TOP_ACT_STOP && m_running) ? HOVER : TOP_BG);
	dc.SetTextColor(m_running ? TEXT : TEXT_DIM);
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

	switch (HitTest(point, client.Width()))
	{
	case TOP_NAV_MONITOR: case TOP_NAV_RESULTS:
	case TOP_NAV_RECIPE:  case TOP_NAV_STATS: case TOP_NAV_CONFIG:
		m_activeView = (NavView)(HitTest(point, client.Width()) - TOP_NAV_MONITOR);
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
		// Blank area in the top bar drags the window
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

	CRect client;
	GetClientRect(&client);
	TopBtn prev = m_hoverBtn;
	m_hoverBtn  = HitTest(point, client.Width());

	if (m_hoverBtn != prev) Invalidate();

	CDockablePane::OnMouseMove(nFlags, point);
}

void CTopBar::OnMouseLeave()
{
	m_trackingMouse = false;
	if (m_hoverBtn != TOP_NONE)
	{
		m_hoverBtn = TOP_NONE;
		Invalidate();
	}
}
