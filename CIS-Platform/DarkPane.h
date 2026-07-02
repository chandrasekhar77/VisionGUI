#pragma once
#include "UITheme.h"

class CDarkPane : public CDockablePane
{
	DECLARE_DYNAMIC(CDarkPane)
public:
	CDarkPane() = default;

protected:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};
