#pragma once
#include "UITheme.h"
#include "PCBModel.h"

// Second top-docked row, Dashboard-tab only: channel select, Save/Display Init,
// LOT/ID + Apply, model name display. Chrome-like (no float/resize), same
// contract as CTopBar, but content is Dashboard-specific so it stays a
// separate pane and hides on other nav tabs.
class CDashboardToolbar : public CDockablePane
{
    DECLARE_DYNAMIC(CDashboardToolbar)

public:
    CDashboardToolbar() = default;
    virtual BOOL CanFloat()     const override { return FALSE; }
    virtual BOOL CanBeResized() const override { return FALSE; }

    LightChannel GetActiveChannel() const { return m_activeChannel; }

protected:
    CButton m_radioOrigin, m_radioRed, m_radioGreen, m_radioBlue;
    CButton m_btnSave, m_btnDisplayInit, m_btnApply;
    CStatic m_lblLot, m_lblId, m_lblModel;
    CEdit   m_editLot, m_editId;
    CStatic m_staticModel;
    CFont   m_font;

    LightChannel m_activeChannel = LightChannel::White;

    afx_msg int     OnCreate(LPCREATESTRUCT lpcs);
    afx_msg void    OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL    OnEraseBkgnd(CDC* pDC);
    afx_msg void    OnChannelClicked(UINT nID);
    afx_msg void    OnApplyClicked();
    afx_msg LRESULT OnModelLoaded(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()
};
