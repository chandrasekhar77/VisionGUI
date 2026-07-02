#pragma once
#include "UITheme.h"
#include "DashboardTabStrip.h"
#include "ContentView.h"

// Dashboard tab's own content area. Hosts the "Main View | Camera | Light |
// Inspect" sub-tab strip; Main View's content is the surrounding dockpanes
// (owned by CMainFrame), so this view just hides all stub sub-screens when
// Main View is active and shows the matching stub otherwise.
class CMonitoringView : public CWnd
{
public:
	CMonitoringView() = default;
	virtual ~CMonitoringView() = default;

	int GetActiveSubTab() const { return m_wndTabStrip.GetActiveTab(); }

protected:
	CDashboardTabStrip m_wndTabStrip;
	CContentView       m_wndCameraStub;
	CContentView       m_wndLightStub;
	CContentView       m_wndInspectStub;

	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	afx_msg int     OnCreate(LPCREATESTRUCT lpcs);
	afx_msg void    OnPaint();
	afx_msg BOOL    OnEraseBkgnd(CDC* pDC);
	afx_msg void    OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void    OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnSubTabChanged(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
};
