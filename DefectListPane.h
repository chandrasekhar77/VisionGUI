#pragma once
#include "Contracts/IDefectListListener.h"
#include "DarkHeaderCtrl.h"
#include "UITheme.h"

// Right-side dock pane — list of defects found in the current panel.
// Implements IDefectListListener so L2 can push defects from a worker thread.
// Thread-safe: OnDefectAdded/OnClear post WM messages; list is only touched on UI thread.
// Clicking a row posts WM_DEFECT_SELECTED to the parent frame for jump-to-defect.
class CDefectListPane : public CDockablePane, public IDefectListListener
{
    DECLARE_DYNAMIC(CDefectListPane)

public:
    // IDefectListListener — safe to call from any thread
    void OnDefectAdded(const DefectInfo& defect) override;
    void OnClear() override;

protected:
    CListCtrl        m_list;
    CDarkHeaderCtrl  m_header;
    CFont            m_font;

    void        AppendRow(const DefectInfo& d);
    COLORREF    SeverityColor(DefectSeverity sev) const;
    LPCTSTR     SeverityStr(DefectSeverity sev)   const;

    afx_msg int     OnCreate(LPCREATESTRUCT lp);
    afx_msg void    OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL    OnEraseBkgnd(CDC* pDC);
    afx_msg LRESULT OnDefectAdded(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnDefectClear(WPARAM wParam, LPARAM lParam);
    afx_msg void    OnNMCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void    OnNMDblClk(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()
};
