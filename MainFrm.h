
// MainFrm.h : interface of the CMainFrame class
//

#pragma once
#include "UITheme.h"
#include "DarkStatusBar.h"
#include "DarkPane.h"
#include "TopBar.h"
#include "MonitoringView.h"

#define ID_TOPBAR        1999
#define ID_PANE_RESULTS  2000
#define ID_PANE_OUTPUT   2001

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

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	CDarkStatusBar  m_wndStatusBar;
	CMonitoringView m_wndView;
	CTopBar         m_wndTopBar;
	CDarkPane       m_wndResultsPane;
	CDarkPane       m_wndOutputPane;

protected:
	afx_msg int     OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void    OnSetFocus(CWnd* pOldWnd);
	afx_msg void    OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	DECLARE_MESSAGE_MAP()
};
