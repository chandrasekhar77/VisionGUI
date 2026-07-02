#pragma once
#include "Contracts/IPanelListListener.h"
#include "DarkHeaderCtrl.h"
#include "UITheme.h"

// Top of the middle dashboard column — panel/sheet list & history.
// Implements IPanelListListener so L3 can push completed panels from a
// worker thread. Thread-safe: OnPanelRecorded/OnPanelListClear post WM
// messages; list is only touched on the UI thread.
class CPanelListPane : public CDockablePane, public IPanelListListener
{
    DECLARE_DYNAMIC(CPanelListPane)

public:
    // IPanelListListener — safe to call from any thread
    void OnPanelRecorded(const PanelRecord& record) override;
    void OnPanelListClear() override;

protected:
    CButton          m_btnClear;
    CListCtrl        m_list;
    CDarkHeaderCtrl  m_header;
    CFont            m_font;

    void     AppendRow(const PanelRecord& r);
    COLORREF StatusColor(PanelStatus s) const;
    LPCTSTR  StatusStr(PanelStatus s) const;

    afx_msg int     OnCreate(LPCREATESTRUCT lp);
    afx_msg void    OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL    OnEraseBkgnd(CDC* pDC);
    afx_msg LRESULT OnPanelAdded(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnPanelClear(WPARAM wParam, LPARAM lParam);
    afx_msg void    OnNMCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void    OnClearClicked();

    DECLARE_MESSAGE_MAP()
};
