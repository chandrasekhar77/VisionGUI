#pragma once
#include "UITheme.h"

// Generic placeholder view. Renders a dark background with a centered label.
// Replace with a proper subclass when the view needs real content.
class CContentView : public CWnd
{
	CString m_label;
	CFont   m_font;

public:
	CContentView() = default;

	// Creates the window; call instead of the generic CWnd::Create.
	BOOL Create(LPCTSTR label, CWnd* pParent, UINT nID);

protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

	afx_msg int  OnCreate(LPCREATESTRUCT lpcs);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()
};
