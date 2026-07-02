#pragma once
#include "UITheme.h"

// Small 4-tab strip living inside CMonitoringView's own client area
// (page-local navigation, NOT a dockable frame pane).
// Posts Theme::WM_SUBTAB_CHANGED (wParam = Theme::DashSubTab) to its parent on click.
class CDashboardTabStrip : public CWnd
{
public:
    CDashboardTabStrip() = default;

    BOOL Create(CWnd* pParent, UINT nID);

    int  GetActiveTab() const { return m_activeTab; }
    void SetActiveTab(int idx);

private:
    static constexpr int kTabCount = 4;
    static LPCTSTR TabLabel(int idx);

    int    m_activeTab = Theme::SUBTAB_MAIN;
    int    m_hoverTab  = -1;
    bool   m_trackingMouse = false;
    CFont  m_font;

    int  HitTest(CPoint pt) const;

protected:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

    afx_msg int  OnCreate(LPCREATESTRUCT lpcs);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    DECLARE_MESSAGE_MAP()
};
