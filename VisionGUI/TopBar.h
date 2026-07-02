#pragma once
#include "UITheme.h"
#include "IVisionModule.h"

class CTopBar : public CDockablePane
{
	DECLARE_DYNAMIC(CTopBar)
public:
	CTopBar() = default;
	virtual BOOL CanFloat()      const override { return FALSE; }
	virtual BOOL CanBeResized()  const override { return FALSE; }

	void SetModule(IVisionModule* pModule);
	int  GetActiveNav() const { return m_activeNav; }

private:
	IVisionModule*  m_pModule      = nullptr;
	int             m_activeNav    = 0;
	Theme::TopBtn   m_hoverBtn     = Theme::TOP_NONE;
	bool            m_trackingMouse = false;
	CFont           m_font;

	void DrawBar(CDC& dc, int w);
	Theme::TopBtn HitTest(CPoint pt, int w) const;
	static void GetActBtnRects(int w, CRect& rcConnect, CRect& rcStart, CRect& rcStop);
	static void GetWinBtnRects(int w, CRect& rcClose, CRect& rcMax, CRect& rcMin);

protected:
	afx_msg int  OnCreate(LPCREATESTRUCT lpcs);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
	DECLARE_MESSAGE_MAP()
};
