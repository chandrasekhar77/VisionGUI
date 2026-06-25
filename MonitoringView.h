#pragma once
#include "UITheme.h"

class CMonitoringView : public CWnd
{
public:
	CMonitoringView() = default;
	virtual ~CMonitoringView() = default;

protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

private:
	enum HoverBtn { HOVER_NONE, HOVER_MIN, HOVER_MAX, HOVER_CLOSE };
	HoverBtn m_hoverBtn      = HOVER_NONE;
	bool     m_trackingMouse = false;
	bool     m_showButtons   = false;

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
	DECLARE_MESSAGE_MAP()
};
