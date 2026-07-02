#include "pch.h"
#include "framework.h"
#include "YieldChartPane.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CYieldChartPane, CDockablePane)

BEGIN_MESSAGE_MAP(CYieldChartPane, CDockablePane)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

int CYieldChartPane::OnCreate(LPCREATESTRUCT lp)
{
    if (CDockablePane::OnCreate(lp) == -1) return -1;

    m_chart.Create(this, 1);
    m_chart.SetLineColor(Theme::GREEN);
    m_chart.SetThreshold(90.0);
    m_chart.SetRange(0.0, 100.0);

    // Placeholder trend — no real L3 yield pipeline exists yet.
    std::vector<CYieldChart::Point> pts;
    const double values[] = {
        62, 58, 65, 80, 88, 92, 90, 87, 93, 95, 96, 94,
        91, 89, 92, 97, 98, 96, 94, 95
    };
    for (double v : values)
        pts.push_back({ _T(""), v });
    m_chart.SetDataPoints(pts);

    return 0;
}

void CYieldChartPane::OnSize(UINT nType, int cx, int cy)
{
    CDockablePane::OnSize(nType, cx, cy);
    if (m_chart.m_hWnd) m_chart.MoveWindow(0, 0, cx, cy);
}

BOOL CYieldChartPane::OnEraseBkgnd(CDC* pDC)
{
    CRect rc; GetClientRect(&rc);
    pDC->FillSolidRect(&rc, Theme::BG);
    return TRUE;
}
