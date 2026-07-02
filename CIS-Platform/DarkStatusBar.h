#pragma once
#include "UITheme.h"

class CDarkStatusBar : public CStatusBar
{
	DECLARE_DYNAMIC(CDarkStatusBar)
public:
	CDarkStatusBar() = default;

protected:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};
