
// MainFrm.cpp : implementation of the CMainFrame class
//

#include "pch.h"
#include "framework.h"
#include "VisionGUI.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWndEx)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWndEx)
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
	ON_WM_SIZE()
	ON_WM_NCCALCSIZE()
	ON_WM_NCHITTEST()
	ON_MESSAGE(Theme::WM_NAV_CHANGED, &CMainFrame::OnNavChanged)
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

CMainFrame::CMainFrame() noexcept {}
CMainFrame::~CMainFrame() {}

// ---------------------------------------------------------------------------
// Creation
// ---------------------------------------------------------------------------

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
		return -1;

	SetMenu(nullptr);

	// Primary view — auto-sized by the frame layout engine
	if (!m_wndView.Create(nullptr, nullptr, AFX_WS_DEFAULT_VIEW,
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, nullptr))
	{
		TRACE0("Failed to create monitoring view\n");
		return -1;
	}

	// Secondary views — manually kept in sync with the primary view via RecalcLayout
	m_wndResultsView.Create(_T("Results View"),  this, ID_VIEW_RESULTS);
	m_wndRecipeView .Create(_T("Recipe View"),   this, ID_VIEW_RECIPE);
	m_wndStatsView  .Create(_T("Statistics"),    this, ID_VIEW_STATS);
	m_wndConfigView .Create(_T("Configuration"), this, ID_VIEW_CONFIG);
	// All secondary views start hidden; monitoring view is the default
	m_wndResultsView.ShowWindow(SW_HIDE);
	m_wndRecipeView .ShowWindow(SW_HIDE);
	m_wndStatsView  .ShowWindow(SW_HIDE);
	m_wndConfigView .ShowWindow(SW_HIDE);

	if (!m_wndStatusBar.Create(this))
	{
		TRACE0("Failed to create status bar\n");
		return -1;
	}
	m_wndStatusBar.SetIndicators(indicators, sizeof(indicators) / sizeof(UINT));

	// Docking
	EnableDocking(CBRS_ALIGN_ANY);

	// Top bar — docked first so all other panes appear below it
	m_wndTopBar.Create(_T(""), this, CRect(0, 0, 0, Theme::TOP_BAR_H), FALSE,
		ID_TOPBAR, WS_CHILD | WS_VISIBLE | CBRS_TOP);
	m_wndTopBar.EnableGripper(FALSE);
	m_wndTopBar.EnableDocking(CBRS_ALIGN_TOP);
	DockPane(&m_wndTopBar, AFX_IDW_DOCKBAR_TOP);

	// Content panes — restricted to left/right/bottom only
	m_wndResultsPane.Create(_T("Inspection Results"), this, CRect(0, 0, 280, 400), TRUE,
		ID_PANE_RESULTS,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_RIGHT | CBRS_FLOAT_MULTI);
	m_wndResultsPane.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT | CBRS_ALIGN_BOTTOM);
	DockPane(&m_wndResultsPane);

	m_wndOutputPane.Create(_T("Output"), this, CRect(0, 0, 400, 140), TRUE,
		ID_PANE_OUTPUT,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_BOTTOM | CBRS_FLOAT_MULTI);
	m_wndOutputPane.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT | CBRS_ALIGN_BOTTOM);
	DockPane(&m_wndOutputPane);

	BOOL darkMode = TRUE;
	::DwmSetWindowAttribute(m_hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

	MARGINS margins = { 1, 1, 1, 1 };
	::DwmExtendFrameIntoClientArea(m_hWnd, &margins);

	SetWindowPos(nullptr, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if (!CFrameWndEx::PreCreateWindow(cs))
		return FALSE;

	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.lpszClass = AfxRegisterWndClass(0);
	return TRUE;
}

// ---------------------------------------------------------------------------
// Layout — keep secondary views co-located with the primary view
// ---------------------------------------------------------------------------

void CMainFrame::RecalcLayout(BOOL bNotify)
{
	CFrameWndEx::RecalcLayout(bNotify);

	CWnd* pPrimary = GetDlgItem(AFX_IDW_PANE_FIRST);
	if (!pPrimary || !pPrimary->m_hWnd) return;

	CRect rc;
	pPrimary->GetWindowRect(&rc);
	ScreenToClient(&rc);

	auto refit = [&](CWnd& w) { if (w.m_hWnd) w.MoveWindow(&rc, FALSE); };
	refit(m_wndResultsView);
	refit(m_wndRecipeView);
	refit(m_wndStatsView);
	refit(m_wndConfigView);
}

// ---------------------------------------------------------------------------
// View switching
// ---------------------------------------------------------------------------

void CMainFrame::ShowView(Theme::NavView view)
{
	m_wndView       .ShowWindow(view == Theme::VIEW_MONITORING ? SW_SHOW : SW_HIDE);
	m_wndResultsView.ShowWindow(view == Theme::VIEW_RESULTS    ? SW_SHOW : SW_HIDE);
	m_wndRecipeView .ShowWindow(view == Theme::VIEW_RECIPE     ? SW_SHOW : SW_HIDE);
	m_wndStatsView  .ShowWindow(view == Theme::VIEW_STATS      ? SW_SHOW : SW_HIDE);
	m_wndConfigView .ShowWindow(view == Theme::VIEW_CONFIG     ? SW_SHOW : SW_HIDE);
}

LRESULT CMainFrame::OnNavChanged(WPARAM wParam, LPARAM /*lParam*/)
{
	ShowView(static_cast<Theme::NavView>(wParam));
	return 0;
}

// ---------------------------------------------------------------------------
// Diagnostics / misc
// ---------------------------------------------------------------------------

#ifdef _DEBUG
void CMainFrame::AssertValid() const { CFrameWndEx::AssertValid(); }
void CMainFrame::Dump(CDumpContext& dc) const { CFrameWndEx::Dump(dc); }
#endif

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
	CFrameWndEx::OnSize(nType, cx, cy);
	// Force all children to repaint — needed after maximize/restore because
	// WM_NCCALCSIZE changes the client rect outside MFC's normal layout path.
	RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void CMainFrame::OnSetFocus(CWnd* /*pOldWnd*/)
{
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
	if (!bCalcValidRects) return;

	if (IsZoomed())
	{
		HMONITOR hMon = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi = { sizeof(mi) };
		GetMonitorInfo(hMon, &mi);
		lpncsp->rgrc[0] = mi.rcWork;
	}
}

LRESULT CMainFrame::OnNcHitTest(CPoint point)
{
	CRect wr;
	GetWindowRect(&wr);

	if (!IsZoomed())
	{
		const int b = 4;
		if (point.x <= wr.left + b  && point.y <= wr.top + b)    return HTTOPLEFT;
		if (point.x >= wr.right - b && point.y <= wr.top + b)    return HTTOPRIGHT;
		if (point.x <= wr.left + b  && point.y >= wr.bottom - b) return HTBOTTOMLEFT;
		if (point.x >= wr.right - b && point.y >= wr.bottom - b) return HTBOTTOMRIGHT;
		if (point.x <= wr.left + b)  return HTLEFT;
		if (point.x >= wr.right - b) return HTRIGHT;
		if (point.y <= wr.top + b)   return HTTOP;
		if (point.y >= wr.bottom - b) return HTBOTTOM;
	}

	if (point.y < wr.top + Theme::TOP_BAR_H)
	{
		int navEnd   = wr.left + Theme::NAV_BTN_W * Theme::NAV_COUNT;
		int actStart = wr.right - Theme::BTN_W * 3 - Theme::ACT_BTN_W * 3 - 8;
		if (point.x > navEnd && point.x < actStart)
			return HTCAPTION;
		return HTCLIENT;
	}

	return HTCLIENT;
}
