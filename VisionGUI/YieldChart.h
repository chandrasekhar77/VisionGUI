#pragma once
#include "UITheme.h"
#include <vector>

// Generic reusable GDI+ trend-line widget (not a dockpane) — dark grid
// background, filled area under the line, optional dashed threshold, and
// point markers. Used by CYieldChartPane for "Today Yield Data".
class CYieldChart : public CWnd
{
public:
    struct Point
    {
        CString label;
        double  value;
    };

    CYieldChart() = default;

    BOOL Create(CWnd* pParent, UINT nID);

    void SetDataPoints(const std::vector<Point>& pts);
    void SetThreshold(double value, COLORREF color = RGB(0xE0, 0x50, 0x50));
    void SetLineColor(COLORREF c = Theme::GREEN);
    void SetRange(double minY, double maxY); // optional; auto-computed otherwise

protected:
    std::vector<Point> m_points;
    bool    m_hasThreshold = false;
    double  m_threshold    = 0.0;
    COLORREF m_thresholdColor = RGB(0xE0, 0x50, 0x50);
    COLORREF m_lineColor      = Theme::GREEN;
    bool    m_hasRange = false;
    double  m_rangeMin = 0.0, m_rangeMax = 100.0;

    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()
};
