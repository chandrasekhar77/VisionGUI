// MonitoringView.cpp : implementation of the CMonitoringView class
//

#include "pch.h"
#include "framework.h"
#include "VisionGUI.h"
#include "MonitoringView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CMonitoringView, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

BOOL CMonitoringView::PreCreateWindow(CREATESTRUCT& cs)
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

void CMonitoringView::OnPaint()
{
	CPaintDC dc(this);
	CRect client;
	GetClientRect(&client);
	dc.FillSolidRect(&client, Theme::BG);
}

BOOL CMonitoringView::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;
}

void CMonitoringView::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
	ClientToScreen(&point);
	ReleaseCapture();
	GetParentFrame()->PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
}
