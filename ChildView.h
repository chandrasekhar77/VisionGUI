#pragma once
#include "UITheme.h"

class CChildView : public CWnd
{
public:
	CChildView();
	virtual ~CChildView();

	enum NavView { VIEW_MONITORING, VIEW_RESULTS, VIEW_RECIPE, VIEW_STATS, VIEW_CONFIG };
	enum TopBtn  { TOP_NONE,
	               TOP_NAV_MONITOR, TOP_NAV_RESULTS, TOP_NAV_RECIPE, TOP_NAV_STATS, TOP_NAV_CONFIG,
	               TOP_ACT_CONNECT, TOP_ACT_START, TOP_ACT_STOP };

protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

private:
	// Overlay window buttons state
	enum HoverBtn { HOVER_NONE, HOVER_MIN, HOVER_MAX, HOVER_CLOSE };
	HoverBtn m_hoverBtn      = HOVER_NONE;
	bool     m_trackingMouse = false;
	bool     m_showButtons   = false;

	// Top bar state
	NavView  m_activeView    = VIEW_MONITORING;
	TopBtn   m_hoverTopBtn   = TOP_NONE;
	bool     m_connected     = false;
	bool     m_running       = false;
	CFont    m_topBarFont;

	void      DrawTopBar(CDC& dc, int w);
	static TopBtn HitTestTopBar(CPoint pt, int w);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
	DECLARE_MESSAGE_MAP()
};
