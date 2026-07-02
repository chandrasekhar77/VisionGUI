#pragma once
#include "UITheme.h"

// Left-side dock pane — Model Manager.
// Controls will be added later on request.
class CModelManagerPane : public CDockablePane
{
    DECLARE_DYNAMIC(CModelManagerPane)

protected:
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    DECLARE_MESSAGE_MAP()
};
