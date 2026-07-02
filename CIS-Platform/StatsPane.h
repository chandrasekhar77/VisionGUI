#pragma once
#include "DarkHeaderCtrl.h"
#include "UITheme.h"

// Middle dashboard column, below the defect summary — Total/OK/NG/date/%
// statistics as a compact 2-column (Label/Value) list.
class CStatsPane : public CDockablePane
{
    DECLARE_DYNAMIC(CStatsPane)

public:
    CStatsPane() = default;

    void SetStats(int total, int ok, int ng, const CString& date);

protected:
    static constexpr int kRowCount = 6; // Total, OK, NG, Date, OK%, NG%

    CListCtrl        m_list;
    CDarkHeaderCtrl  m_header;
    CFont            m_font;

    afx_msg int  OnCreate(LPCREATESTRUCT lp);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnNMCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()
};
