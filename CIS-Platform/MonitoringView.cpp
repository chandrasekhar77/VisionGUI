// MonitoringView.cpp : implementation of the CMonitoringView class
//

#include "pch.h"
#include "framework.h"
#include "CIS-Platform.h"
#include "MonitoringView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define ID_DASH_TABSTRIP   (AFX_IDW_PANE_FIRST + 900)
#define ID_DASH_CAMERA     (AFX_IDW_PANE_FIRST + 901)
#define ID_DASH_LIGHT      (AFX_IDW_PANE_FIRST + 902)
#define ID_DASH_INSPECT    (AFX_IDW_PANE_FIRST + 903)

BEGIN_MESSAGE_MAP(CMonitoringView, CWnd)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_SIZE()
	ON_MESSAGE(Theme::WM_SUBTAB_CHANGED, &CMonitoringView::OnSubTabChanged)
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

int CMonitoringView::OnCreate(LPCREATESTRUCT lpcs)
{
	if (CWnd::OnCreate(lpcs) == -1) return -1;

	m_wndTabStrip.Create(this, ID_DASH_TABSTRIP);
	m_wndCameraStub.Create(_T("Camera Setup"), this, ID_DASH_CAMERA);
	m_wndLightStub.Create(_T("Light Setup"), this, ID_DASH_LIGHT);
	m_wndInspectStub.Create(_T("Inspect Setup"), this, ID_DASH_INSPECT);

	return 0;
}

void CMonitoringView::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);

	if (m_wndTabStrip.m_hWnd)
		m_wndTabStrip.MoveWindow(0, 0, cx, Theme::SUBTAB_H);

	CRect rcBody(0, Theme::SUBTAB_H, cx, cy);
	if (m_wndCameraStub.m_hWnd)   m_wndCameraStub.MoveWindow(&rcBody);
	if (m_wndLightStub.m_hWnd)    m_wndLightStub.MoveWindow(&rcBody);
	if (m_wndInspectStub.m_hWnd)  m_wndInspectStub.MoveWindow(&rcBody);
}

LRESULT CMonitoringView::OnSubTabChanged(WPARAM wParam, LPARAM /*lParam*/)
{
	int tab = (int)wParam;

	m_wndCameraStub.ShowWindow(tab == Theme::SUBTAB_CAMERA ? SW_SHOW : SW_HIDE);
	m_wndLightStub.ShowWindow(tab == Theme::SUBTAB_LIGHT ? SW_SHOW : SW_HIDE);
	m_wndInspectStub.ShowWindow(tab == Theme::SUBTAB_INSPECT ? SW_SHOW : SW_HIDE);

	// Relay up to CMainFrame, which owns the dashboard-body dockpanes'
	// visibility (Main View's content lives in those panes, not in this view).
	GetParent()->PostMessage(Theme::WM_SUBTAB_CHANGED, wParam, 0);
	return 0;
}

void CMonitoringView::OnPaint()
{
	CPaintDC dc(this);
	CRect client;
	GetClientRect(&client);
	dc.FillSolidRect(&client, Theme::BG);
}

BOOL CMonitoringView::OnEraseBkgnd(CDC* /*pDC*/) { return TRUE; }

void CMonitoringView::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
	ClientToScreen(&point);
	ReleaseCapture();
	GetParentFrame()->PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
}
