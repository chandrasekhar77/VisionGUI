
// MainFrm.cpp : implementation of the CMainFrame class
//

#include "pch.h"
#include "framework.h"
#include "VisionGUI.h"

#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWndEx)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
	ON_WM_NCCALCSIZE()
	ON_WM_NCHITTEST()
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

// CMainFrame construction/destruction

CMainFrame::CMainFrame() noexcept
{
	// TODO: add member initialization code here
}

CMainFrame::~CMainFrame()
{
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
		return -1;

	SetMenu(nullptr);

	if (!m_wndView.Create(nullptr, nullptr, AFX_WS_DEFAULT_VIEW, CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, nullptr))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}

	if (!m_wndStatusBar.Create(this))
	{
		TRACE0("Failed to create status bar\n");
		return -1;
	}
	m_wndStatusBar.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT));

	// Docking
	EnableDocking(CBRS_ALIGN_ANY);

	m_wndResultsPane.Create(_T("Inspection Results"), this, CRect(0, 0, 280, 400), TRUE,
		ID_PANE_RESULTS,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_RIGHT | CBRS_FLOAT_MULTI);
	m_wndResultsPane.EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndResultsPane);

	m_wndOutputPane.Create(_T("Output"), this, CRect(0, 0, 400, 140), TRUE,
		ID_PANE_OUTPUT,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_BOTTOM | CBRS_FLOAT_MULTI);
	m_wndOutputPane.EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndOutputPane);

	BOOL darkMode = TRUE;
	::DwmSetWindowAttribute(m_hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

	// Preserve DWM drop shadow on frameless window
	MARGINS margins = { 1, 1, 1, 1 };
	::DwmExtendFrameIntoClientArea(m_hWnd, &margins);

	// Trigger NC recalculation so the frame has no chrome
	SetWindowPos(nullptr, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if (!CFrameWndEx::PreCreateWindow(cs))
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.lpszClass = AfxRegisterWndClass(0);
	return TRUE;
}

// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWndEx::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWndEx::Dump(dc);
}
#endif //_DEBUG


// CMainFrame message handlers

void CMainFrame::OnSetFocus(CWnd* /*pOldWnd*/)
{
	// forward focus to the view window
	m_wndView.SetFocus();
}

BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	if (m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return TRUE;
	return CFrameWndEx::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CMainFrame::OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp)
{
	if (!bCalcValidRects)
		return;

	// When maximized, constrain to the monitor work area to avoid overscan
	if (IsZoomed())
	{
		HMONITOR hMon = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi = { sizeof(mi) };
		GetMonitorInfo(hMon, &mi);
		lpncsp->rgrc[0] = mi.rcWork;
	}
	// Return without calling base — entire window rect becomes client area
}

LRESULT CMainFrame::OnNcHitTest(CPoint point)
{
	CRect wr;
	GetWindowRect(&wr);

	// Resize borders when not maximized
	if (!IsZoomed())
	{
		const int b = 4;
		if (point.x <= wr.left + b  && point.y <= wr.top + b)     return HTTOPLEFT;
		if (point.x >= wr.right - b && point.y <= wr.top + b)     return HTTOPRIGHT;
		if (point.x <= wr.left + b  && point.y >= wr.bottom - b)  return HTBOTTOMLEFT;
		if (point.x >= wr.right - b && point.y >= wr.bottom - b)  return HTBOTTOMRIGHT;
		if (point.x <= wr.left + b)  return HTLEFT;
		if (point.x >= wr.right - b) return HTRIGHT;
		if (point.y <= wr.top + b)   return HTTOP;
		if (point.y >= wr.bottom - b) return HTBOTTOM;
	}

	// Caption zone — lets Windows handle drag and system double-click
	// Exclude the rightmost 3 buttons so they get WM_LBUTTONDOWN in CChildView
	if (point.y < wr.top + Theme::TITLE_H &&
		point.x < wr.right - Theme::BTN_W * 3)
		return HTCAPTION;

	return HTCLIENT;
}


