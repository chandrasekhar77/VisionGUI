
// ChildView.cpp : implementation of the CChildView class
//

#include "pch.h"
#include "framework.h"
#include "VisionGUI.h"
#include "ChildView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


CChildView::CChildView()
{
	m_topBarFont.CreateFont(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));
}

CChildView::~CChildView()
{
}


BEGIN_MESSAGE_MAP(CChildView, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()


BOOL CChildView::PreCreateWindow(CREATESTRUCT& cs)
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;

	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.style     &= ~WS_BORDER;
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
		::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)::GetStockObject(NULL_BRUSH), nullptr);

	return TRUE;
}


// ---------------------------------------------------------------------------
// Overlay window button helpers
// ---------------------------------------------------------------------------

static void GetBtnRects(int w, CRect& rcClose, CRect& rcMax, CRect& rcMin)
{
	int h = Theme::TITLE_H, bw = Theme::BTN_W;
	rcClose = { w - bw,      0, w,          h };
	rcMax   = { w - bw * 2,  0, w - bw,     h };
	rcMin   = { w - bw * 3,  0, w - bw * 2, h };
}

enum BtnType { BTN_MIN, BTN_MAX, BTN_CLOSE };

static void DrawTitleBtn(CDC* dc, const CRect& rc, BtnType type, bool hover, bool isZoomed = false)
{
	COLORREF bg = hover
		? (type == BTN_CLOSE ? Theme::CLOSE_HOVER : Theme::HOVER)
		: Theme::TOP_BG;
	dc->FillSolidRect(&rc, bg);

	int cx = rc.left + rc.Width() / 2;
	int cy = rc.top  + rc.Height() / 2;

	CPen pen(PS_SOLID, 1, Theme::TEXT);
	CPen*   pOldPen = dc->SelectObject(&pen);
	CBrush* pOldBr  = (CBrush*)dc->SelectStockObject(NULL_BRUSH);

	switch (type)
	{
	case BTN_CLOSE:
		dc->MoveTo(cx - 5, cy - 5); dc->LineTo(cx + 6, cy + 6);
		dc->MoveTo(cx + 5, cy - 5); dc->LineTo(cx - 6, cy + 6);
		break;
	case BTN_MAX:
		if (isZoomed)
		{
			dc->MoveTo(cx,     cy - 5); dc->LineTo(cx + 5, cy - 5);
			dc->LineTo(cx + 5, cy + 1); dc->LineTo(cx + 2, cy + 1);
			dc->MoveTo(cx,     cy - 5); dc->LineTo(cx,     cy - 2);
			dc->Rectangle(cx - 5, cy - 2, cx + 3, cy + 6);
		}
		else
		{
			dc->Rectangle(cx - 5, cy - 4, cx + 6, cy + 5);
		}
		break;
	case BTN_MIN:
		dc->MoveTo(cx - 5, cy + 1); dc->LineTo(cx + 6, cy + 1);
		break;
	}

	dc->SelectObject(pOldBr);
	dc->SelectObject(pOldPen);
}


// ---------------------------------------------------------------------------
// Top bar helpers
// ---------------------------------------------------------------------------

static void GetActBtnRects(int w, CRect& rcConnect, CRect& rcStart, CRect& rcStop)
{
	int right = w - Theme::BTN_W * 3 - 8;
	int bw    = Theme::ACT_BTN_W;
	int h     = Theme::TOP_BAR_H;
	rcStop    = { right - bw,     0, right,      h };
	rcStart   = { right - bw * 2, 0, right - bw, h };
	rcConnect = { right - bw * 3, 0, right - bw * 2, h };
}

/*static*/ CChildView::TopBtn CChildView::HitTestTopBar(CPoint pt, int w)
{
	if (pt.y < 0 || pt.y >= Theme::TOP_BAR_H)
		return TOP_NONE;

	// Nav buttons (left)
	if (pt.x < Theme::NAV_BTN_W * Theme::NAV_COUNT)
	{
		int i = pt.x / Theme::NAV_BTN_W;
		return (TopBtn)(TOP_NAV_MONITOR + i);
	}

	// Action buttons (right)
	CRect rcConnect, rcStart, rcStop;
	GetActBtnRects(w, rcConnect, rcStart, rcStop);
	if (rcConnect.PtInRect(pt)) return TOP_ACT_CONNECT;
	if (rcStart.PtInRect(pt))   return TOP_ACT_START;
	if (rcStop.PtInRect(pt))    return TOP_ACT_STOP;

	return TOP_NONE;
}

void CChildView::DrawTopBar(CDC& dc, int w)
{
	// Background + bottom border
	dc.FillSolidRect(0, 0, w, Theme::TOP_BAR_H, Theme::TOP_BG);
	dc.FillSolidRect(0, Theme::TOP_BAR_H - 1, w, 1, Theme::SEPARATOR);

	CFont* pOld = dc.SelectObject(&m_topBarFont);
	dc.SetBkMode(TRANSPARENT);

	// --- Nav buttons ---
	static const LPCTSTR navLabels[Theme::NAV_COUNT] = {
		_T("Monitoring"), _T("Results"), _T("Recipe"), _T("Statistics"), _T("Config")
	};

	for (int i = 0; i < Theme::NAV_COUNT; i++)
	{
		CRect rc(i * Theme::NAV_BTN_W, 0, (i + 1) * Theme::NAV_BTN_W, Theme::TOP_BAR_H);
		bool active = (i == (int)m_activeView);
		bool hover  = (m_hoverTopBtn == (TopBtn)(TOP_NAV_MONITOR + i));

		if (hover && !active)
			dc.FillSolidRect(&rc, Theme::HOVER);

		// Active indicator: 2px accent line at bottom
		if (active)
			dc.FillSolidRect(rc.left, rc.bottom - 2, rc.Width(), 2, Theme::ACCENT);

		dc.SetTextColor(active ? Theme::TEXT : Theme::TEXT_DIM);
		dc.DrawText(navLabels[i], &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}

	// Separator after nav zone
	int sepX = Theme::NAV_BTN_W * Theme::NAV_COUNT;
	dc.FillSolidRect(sepX, 8, 1, Theme::TOP_BAR_H - 16, Theme::SEPARATOR);

	// --- Action buttons ---
	CRect rcConnect, rcStart, rcStop;
	GetActBtnRects(w, rcConnect, rcStart, rcStop);

	// Connect / Disconnect
	COLORREF clrConn = m_connected ? Theme::GREEN
	                 : (m_hoverTopBtn == TOP_ACT_CONNECT ? Theme::HOVER : Theme::TOP_BG);
	dc.FillSolidRect(&rcConnect, clrConn);
	dc.SetTextColor(Theme::TEXT);
	dc.DrawText(m_connected ? _T("Disconnect") : _T("Connect"),
		&rcConnect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	// Start
	COLORREF clrStart = m_running ? Theme::ACCENT
	                  : (m_hoverTopBtn == TOP_ACT_START ? Theme::HOVER : Theme::TOP_BG);
	dc.FillSolidRect(&rcStart, clrStart);
	dc.SetTextColor(Theme::TEXT);
	dc.DrawText(_T("Start"), &rcStart, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	// Stop
	COLORREF clrStop = (m_hoverTopBtn == TOP_ACT_STOP && m_running) ? Theme::HOVER : Theme::TOP_BG;
	dc.FillSolidRect(&rcStop, clrStop);
	dc.SetTextColor(m_running ? Theme::TEXT : Theme::TEXT_DIM);
	dc.DrawText(_T("Stop"), &rcStop, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	dc.SelectObject(pOld);
}


// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void CChildView::OnPaint()
{
	CPaintDC dc(this);
	CRect client;
	GetClientRect(&client);
	int w = client.Width();

	// Top bar
	DrawTopBar(dc, w);

	// Content area
	CRect rcContent(0, Theme::TOP_BAR_H, w, client.Height());
	dc.FillSolidRect(&rcContent, Theme::BG);

	// Overlay window buttons — top-right, auto-show on hover
	if (m_showButtons)
	{
		bool zoomed = GetParentFrame()->IsZoomed() != FALSE;
		CRect rcClose, rcMax, rcMin;
		GetBtnRects(w, rcClose, rcMax, rcMin);
		DrawTitleBtn(&dc, rcClose, BTN_CLOSE, m_hoverBtn == HOVER_CLOSE);
		DrawTitleBtn(&dc, rcMax,   BTN_MAX,   m_hoverBtn == HOVER_MAX, zoomed);
		DrawTitleBtn(&dc, rcMin,   BTN_MIN,   m_hoverBtn == HOVER_MIN);
	}
}

BOOL CChildView::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;
}


// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

void CChildView::OnLButtonDown(UINT nFlags, CPoint point)
{
	CRect client;
	GetClientRect(&client);
	int w = client.Width();

	// Overlay window buttons
	if (m_showButtons)
	{
		CRect rcClose, rcMax, rcMin;
		GetBtnRects(w, rcClose, rcMax, rcMin);
		if (rcClose.PtInRect(point)) { GetParentFrame()->PostMessage(WM_CLOSE); return; }
		if (rcMax.PtInRect(point))
		{
			CFrameWnd* pFrame = GetParentFrame();
			pFrame->ShowWindow(pFrame->IsZoomed() ? SW_RESTORE : SW_MAXIMIZE);
			return;
		}
		if (rcMin.PtInRect(point)) { GetParentFrame()->ShowWindow(SW_MINIMIZE); return; }
	}

	// Top bar buttons
	TopBtn hit = HitTestTopBar(point, w);
	switch (hit)
	{
	case TOP_NAV_MONITOR:
	case TOP_NAV_RESULTS:
	case TOP_NAV_RECIPE:
	case TOP_NAV_STATS:
	case TOP_NAV_CONFIG:
		m_activeView = (NavView)(hit - TOP_NAV_MONITOR);
		InvalidateRect(CRect(0, 0, w, Theme::TOP_BAR_H));
		break;
	case TOP_ACT_CONNECT:
		m_connected = !m_connected;
		if (!m_connected) m_running = false;
		InvalidateRect(CRect(0, 0, w, Theme::TOP_BAR_H));
		break;
	case TOP_ACT_START:
		if (m_connected) { m_running = true;  InvalidateRect(CRect(0, 0, w, Theme::TOP_BAR_H)); }
		break;
	case TOP_ACT_STOP:
		if (m_running)   { m_running = false; InvalidateRect(CRect(0, 0, w, Theme::TOP_BAR_H)); }
		break;
	default:
		CWnd::OnLButtonDown(nFlags, point);
	}
}

void CChildView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_trackingMouse)
	{
		TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
		TrackMouseEvent(&tme);
		m_trackingMouse = true;
	}

	CRect client;
	GetClientRect(&client);
	int w = client.Width();

	// --- Overlay window buttons (top-right) ---
	CRect rcClose, rcMax, rcMin;
	GetBtnRects(w, rcClose, rcMax, rcMin);
	CRect rcBtnZone(w - Theme::BTN_W * 3, 0, w, Theme::TITLE_H);

	bool    showNow   = rcBtnZone.PtInRect(point);
	HoverBtn prevHover = m_hoverBtn;
	bool    prevShow  = m_showButtons;

	m_showButtons = showNow;
	if (showNow)
	{
		if      (rcClose.PtInRect(point)) m_hoverBtn = HOVER_CLOSE;
		else if (rcMax.PtInRect(point))   m_hoverBtn = HOVER_MAX;
		else if (rcMin.PtInRect(point))   m_hoverBtn = HOVER_MIN;
		else                              m_hoverBtn = HOVER_NONE;
	}
	else
	{
		m_hoverBtn = HOVER_NONE;
	}

	if (m_showButtons != prevShow || m_hoverBtn != prevHover)
		InvalidateRect(CRect(w - Theme::BTN_W * 3, 0, w, Theme::TITLE_H));

	// --- Top bar buttons ---
	TopBtn prevTop = m_hoverTopBtn;
	m_hoverTopBtn  = HitTestTopBar(point, w);

	if (m_hoverTopBtn != prevTop)
		InvalidateRect(CRect(0, 0, w, Theme::TOP_BAR_H));

	CWnd::OnMouseMove(nFlags, point);
}

void CChildView::OnMouseLeave()
{
	m_trackingMouse = false;

	bool dirty = m_showButtons || m_hoverBtn != HOVER_NONE || m_hoverTopBtn != TOP_NONE;

	m_showButtons  = false;
	m_hoverBtn     = HOVER_NONE;
	m_hoverTopBtn  = TOP_NONE;

	if (dirty)
	{
		CRect client;
		GetClientRect(&client);
		InvalidateRect(CRect(0, 0, client.Width(), Theme::TOP_BAR_H));
	}
}
