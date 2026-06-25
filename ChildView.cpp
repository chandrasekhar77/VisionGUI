
// ChildView.cpp : implementation of the CChildView class
//

#include "pch.h"
#include "framework.h"
#include "VisionGUI.h"
#include "ChildView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


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
		: Theme::BG;
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
// Paint
// ---------------------------------------------------------------------------

void CChildView::OnPaint()
{
	CPaintDC dc(this);
	CRect client;
	GetClientRect(&client);
	int w = client.Width();

	// Full dark content area
	dc.FillSolidRect(&client, Theme::BG);

	// Overlay window buttons — appear on hover at top-right
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
// Mouse — overlay buttons only
// ---------------------------------------------------------------------------

void CChildView::OnLButtonDown(UINT nFlags, CPoint point)
{
	CRect client;
	GetClientRect(&client);
	int w = client.Width();

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

	CWnd::OnLButtonDown(nFlags, point);
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

	CWnd::OnMouseMove(nFlags, point);
}

void CChildView::OnMouseLeave()
{
	m_trackingMouse = false;
	if (m_showButtons || m_hoverBtn != HOVER_NONE)
	{
		m_showButtons = false;
		m_hoverBtn    = HOVER_NONE;
		CRect client;
		GetClientRect(&client);
		InvalidateRect(CRect(client.Width() - Theme::BTN_W * 3, 0, client.Width(), Theme::TITLE_H));
	}
}
