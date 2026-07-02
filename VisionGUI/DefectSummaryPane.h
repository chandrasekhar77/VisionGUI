#pragma once
#include "DarkHeaderCtrl.h"
#include "UITheme.h"

// Middle dashboard column, below the panel list — compact defect-type
// summary: a color swatch + count per type, plus a bolded Total row.
class CDefectSummaryPane : public CDockablePane
{
    DECLARE_DYNAMIC(CDefectSummaryPane)

public:
    CDefectSummaryPane() = default;

    void SetCounts(int c1, int c2, int c3);

protected:
    static constexpr int kRowCount = 4; // 3 types + Total

    CListCtrl        m_list;
    CDarkHeaderCtrl  m_header;
    CFont            m_font;
    CFont            m_boldFont;

    void RefreshRows();
    int  m_counts[3] = { 0, 0, 0 };

    afx_msg int  OnCreate(LPCREATESTRUCT lp);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnNMCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()
};
