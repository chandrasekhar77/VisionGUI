
// MainFrm.cpp : implementation of the CMainFrame class
//

#include "pch.h"
#include "framework.h"
#include "VisionGUI.h"
#include "MainFrm.h"
#include "InspectionModule.h"

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
CMainFrame::~CMainFrame() { DestroyModuleViews(); }

// ---------------------------------------------------------------------------
// Creation
// ---------------------------------------------------------------------------

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
		return -1;

	SetMenu(nullptr);

	// Placeholder view at AFX_IDW_PANE_FIRST — MFC layout engine requires this.
	// It is always hidden behind module views.
	if (!m_wndView.Create(nullptr, nullptr, AFX_WS_DEFAULT_VIEW,
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, nullptr))
	{
		TRACE0("Failed to create primary view placeholder\n");
		return -1;
	}

	if (!m_wndStatusBar.Create(this))
	{
		TRACE0("Failed to create status bar\n");
		return -1;
	}
	m_wndStatusBar.SetIndicators(indicators, sizeof(indicators) / sizeof(UINT));

	// Docking
	EnableDocking(CBRS_ALIGN_ANY);

	// Top bar — docked first so all other panes sit below it
	m_wndTopBar.Create(_T(""), this, CRect(0, 0, 0, Theme::TOP_BAR_H), FALSE,
		ID_TOPBAR, WS_CHILD | WS_VISIBLE | CBRS_TOP);
	m_wndTopBar.EnableGripper(FALSE);
	m_wndTopBar.EnableDocking(CBRS_ALIGN_TOP);
	DockPane(&m_wndTopBar, AFX_IDW_DOCKBAR_TOP);

	m_wndLogPane.Create(_T("Log"), this, CRect(0, 0, 600, 160), TRUE,
		ID_PANE_LOG,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_BOTTOM | CBRS_FLOAT_MULTI);
	m_wndLogPane.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT | CBRS_ALIGN_BOTTOM);
	DockPane(&m_wndLogPane);

	m_wndDefectPane.Create(_T("Defects"), this, CRect(0, 0, 340, 500), TRUE,
		ID_PANE_DEFECTS,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_RIGHT | CBRS_FLOAT_MULTI);
	m_wndDefectPane.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT | CBRS_ALIGN_BOTTOM);
	DockPane(&m_wndDefectPane);

	m_wndModelPane.Create(_T("Models"), this, CRect(0, 0, 220, 500), TRUE,
		ID_PANE_MODELS,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_LEFT | CBRS_FLOAT_MULTI);
	m_wndModelPane.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT | CBRS_ALIGN_BOTTOM);
	DockPane(&m_wndModelPane);

	// Register global logger — all DLLs and layers can now call VisionLog::Info() etc.
	VisionLog::Set(&m_wndLogPane);
	VisionLog::Info(_T("VisionGUI started"));

	BOOL darkMode = TRUE;
	::DwmSetWindowAttribute(m_hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

	MARGINS margins = { 1, 1, 1, 1 };
	::DwmExtendFrameIntoClientArea(m_hWnd, &margins);

	SetWindowPos(nullptr, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

	// Register all modules, load the default one
	RegisterModules();
	if (!m_modules.empty())
		LoadModule(m_modules[0].get());

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
// Module management
// ---------------------------------------------------------------------------

void CMainFrame::RegisterModules()
{
	m_modules.push_back(std::make_unique<CInspectionModule>());
	// Add more modules here:
	// m_modules.push_back(std::make_unique<CCalibrationModule>());
}

void CMainFrame::LoadModule(IVisionModule* pModule)
{
	DestroyModuleViews();

	m_pActiveModule = pModule;
	m_wndTopBar.SetModule(pModule);

	if (!pModule) return;

	// Ask the module to create a view for each of its nav tabs
	int count = pModule->GetNavCount();
	m_moduleViews.reserve(count);
	for (int i = 0; i < count; i++)
	{
		UINT id = (UINT)(ID_MODULE_VIEW_BASE + i);
		CWnd* pView = pModule->CreateView(i, this, id);
		m_moduleViews.push_back(pView);
	}

	// Position views and show the first one
	RecalcLayout();
	ShowView(0);
	UpdatePaneVisibility(0);
}

void CMainFrame::DestroyModuleViews()
{
	for (CWnd* p : m_moduleViews)
	{
		if (p && p->m_hWnd)
			p->DestroyWindow();
		delete p;
	}
	m_moduleViews.clear();
}

// ---------------------------------------------------------------------------
// Layout — keep module views co-located with the primary view placeholder
// ---------------------------------------------------------------------------

void CMainFrame::RecalcLayout(BOOL bNotify)
{
	CFrameWndEx::RecalcLayout(bNotify);

	CWnd* pPrimary = GetDlgItem(AFX_IDW_PANE_FIRST);
	if (!pPrimary || !pPrimary->m_hWnd) return;

	CRect rc;
	pPrimary->GetWindowRect(&rc);
	ScreenToClient(&rc);

	for (CWnd* pView : m_moduleViews)
	{
		if (pView && pView->m_hWnd)
			pView->MoveWindow(&rc, FALSE);
	}
}

// ---------------------------------------------------------------------------
// View switching
// ---------------------------------------------------------------------------

void CMainFrame::ShowView(int navIndex)
{
	for (int i = 0; i < (int)m_moduleViews.size(); i++)
	{
		CWnd* p = m_moduleViews[i];
		if (p && p->m_hWnd)
			p->ShowWindow(i == navIndex ? SW_SHOW : SW_HIDE);
	}
}

LRESULT CMainFrame::OnNavChanged(WPARAM wParam, LPARAM /*lParam*/)
{
	int navIndex = (int)wParam;
	ShowView(navIndex);
	UpdatePaneVisibility(navIndex);
	return 0;
}

// ---------------------------------------------------------------------------
// Pane visibility — rules:
//   Log pane      : always visible
//   Defect pane   : Monitoring view only
//   Model pane    : Recipe view only
// ---------------------------------------------------------------------------

void CMainFrame::UpdatePaneVisibility(int navIndex)
{
	m_wndDefectPane.ShowPane(navIndex == Theme::VIEW_DASHBOARD, FALSE, FALSE);
	m_wndModelPane.ShowPane (navIndex == Theme::VIEW_RECIPE,     FALSE, FALSE);
}

// ---------------------------------------------------------------------------
// Misc
// ---------------------------------------------------------------------------

#ifdef _DEBUG
void CMainFrame::AssertValid() const { CFrameWndEx::AssertValid(); }
void CMainFrame::Dump(CDumpContext& dc) const { CFrameWndEx::Dump(dc); }
#endif

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
	CFrameWndEx::OnSize(nType, cx, cy);
	RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

void CMainFrame::OnSetFocus(CWnd* /*pOldWnd*/)
{
	if (!m_moduleViews.empty() && m_moduleViews[0] && m_moduleViews[0]->m_hWnd)
		m_moduleViews[0]->SetFocus();
	else
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
		int navCount = m_pActiveModule ? m_pActiveModule->GetNavCount() : 0;
		int navEnd   = wr.left + Theme::NAV_BTN_W * navCount;
		int actStart = wr.right - Theme::BTN_W * 3 - Theme::ACT_BTN_W * 3 - 8;
		if (point.x > navEnd && point.x < actStart)
			return HTCAPTION;
		return HTCLIENT;
	}

	return HTCLIENT;
}
