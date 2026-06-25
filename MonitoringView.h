#pragma once
#include "UITheme.h"

class CMonitoringView : public CWnd
{
public:
	CMonitoringView() = default;
	virtual ~CMonitoringView() = default;

protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	DECLARE_MESSAGE_MAP()
};
