#pragma once
#include "UITheme.h"

class CTopBar : public CDockablePane
{
	DECLARE_DYNAMIC(CTopBar)
public:
	CTopBar() = default;
	virtual BOOL CanFloat() const override { return FALSE; }

private:
	Theme::NavView  m_activeView   = Theme::VIEW_MONITORING;
	Theme::TopBtn   m_hoverTopBtn  = Theme::TOP_NONE;
	bool            m_connected    = false;
	bool            m_running      = false;
	bool            m_trackingMouse = false;
	CFont           m_font;

	void DrawBar(CDC& dc, int w);
	static Theme::TopBtn HitTest(CPoint pt, int w);
	static void GetActBtnRects(int w, CRect& rcConnect, CRect& rcStart, CRect& rcStop);

protected:
	afx_msg int  OnCreate(LPCREATESTRUCT lpcs);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
	DECLARE_MESSAGE_MAP()
};
