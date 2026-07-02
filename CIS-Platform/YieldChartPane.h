#pragma once
#include "YieldChart.h"
#include "UITheme.h"

// Bottom of the middle dashboard column — "Today Yield Data" trend chart.
class CYieldChartPane : public CDockablePane
{
    DECLARE_DYNAMIC(CYieldChartPane)

public:
    CYieldChartPane() = default;

protected:
    CYieldChart m_chart;

    afx_msg int  OnCreate(LPCREATESTRUCT lp);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()
};
