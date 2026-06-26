
// MainFrm.h : interface of the CMainFrame class
//

#pragma once
#include <vector>
#include <memory>
#include "UITheme.h"
#include "DarkStatusBar.h"
#include "DarkPane.h"
#include "TopBar.h"
#include "MonitoringView.h"
#include "IVisionModule.h"

#define ID_TOPBAR        1999
#define ID_PANE_RESULTS  2000
#define ID_PANE_OUTPUT   2001

// Base ID for views created by the active module
#define ID_MODULE_VIEW_BASE  (AFX_IDW_PANE_FIRST + 10)

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
	CMonitoringView m_wndView;          // AFX_IDW_PANE_FIRST — placeholder for MFC layout engine
	CTopBar         m_wndTopBar;
	CDarkPane       m_wndResultsPane;
	CDarkPane       m_wndOutputPane;

	// Module management
	std::vector<std::unique_ptr<IVisionModule>> m_modules;
	IVisionModule*  m_pActiveModule = nullptr;
	std::vector<CWnd*> m_moduleViews;    // owned by us; created by the active module

	void RegisterModules();
	void LoadModule(IVisionModule* pModule);
	void ShowView(int navIndex);
	void DestroyModuleViews();

protected:
	afx_msg int     OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void    OnSetFocus(CWnd* pOldWnd);
	afx_msg void    OnSize(UINT nType, int cx, int cy);
	afx_msg void    OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg LRESULT OnNavChanged(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
};
