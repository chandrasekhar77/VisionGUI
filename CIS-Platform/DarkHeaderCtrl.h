#pragma once
#include "UITheme.h"

// Subclassed CHeaderCtrl that paints with the dark theme.
// Usage: call SubclassWindow(pList->GetHeaderCtrl()->GetSafeHwnd()) after list creation.
class CDarkHeaderCtrl : public CHeaderCtrl
{
public:
    CDarkHeaderCtrl() = default;

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()
};
