#pragma once
#include <afxPaneDivider.h>

// Replaces the default CPaneDivider to enable hover detection.
// Register via CPaneDivider::m_pSliderRTC = RUNTIME_CLASS(CDarkPaneDivider).
class CDarkPaneDivider : public CPaneDivider
{
    DECLARE_DYNCREATE(CDarkPaneDivider)

protected:
    bool m_bTracking = false;

    afx_msg void    OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void    OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg LRESULT OnMouseLeave(WPARAM, LPARAM);

    DECLARE_MESSAGE_MAP()
};
