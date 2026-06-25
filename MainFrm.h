
// MainFrm.h : interface of the CMainFrame class
//

#pragma once
#include "UITheme.h"
#include "DarkStatusBar.h"
#include "DarkPane.h"
#include "TopBar.h"
#include "MonitoringView.h"
#include "ContentView.h"

#define ID_TOPBAR        1999
#define ID_PANE_RESULTS  2000
#define ID_PANE_OUTPUT   2001

// Secondary view IDs (primary = AFX_IDW_PANE_FIRST)
#define ID_VIEW_RESULTS  (AFX_IDW_PANE_FIRST + 1)
#define ID_VIEW_RECIPE   (AFX_IDW_PANE_FIRST + 2)
#define ID_VIEW_STATS    (AFX_IDW_PANE_FIRST + 3)
#define ID_VIEW_CONFIG   (AFX_IDW_PANE_FIRST + 4)

class CMainFrame : public CFrameWndEx
{
public:
	CMainFrame() noexcept;
	virtual ~CMainFrame();

protected:
	DECLARE_DYNAMIC(CMainFrame)

public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	virtual void RecalcLayout(BOOL bNotify = TRUE);

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	CDarkStatusBar  m_wndStatusBar;
	CMonitoringView m_wndView;          // AFX_IDW_PANE_FIRST — auto-sized by frame
	CContentView    m_wndResultsView;
	CContentView    m_wndRecipeView;
	CContentView    m_wndStatsView;
	CContentView    m_wndConfigView;
	CTopBar         m_wndTopBar;
	CDarkPane       m_wndResultsPane;
	CDarkPane       m_wndOutputPane;

	void ShowView(Theme::NavView view);

protected:
	afx_msg int     OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void    OnSetFocus(CWnd* pOldWnd);
	afx_msg void    OnSize(UINT nType, int cx, int cy);
	afx_msg void    OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg LRESULT OnNavChanged(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
};
