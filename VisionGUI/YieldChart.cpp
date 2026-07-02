#include "pch.h"
#include "framework.h"
#include "YieldChart.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace Theme;

BEGIN_MESSAGE_MAP(CYieldChart, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CYieldChart::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CWnd::PreCreateWindow(cs))
        return FALSE;

    cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
    cs.style     &= ~WS_BORDER;
    cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)::GetStockObject(NULL_BRUSH), nullptr);

    return TRUE;
}

BOOL CYieldChart::Create(CWnd* pParent, UINT nID)
{
    return CWnd::Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE,
        CRect(0, 0, 0, 0), pParent, nID);
}

void CYieldChart::SetDataPoints(const std::vector<Point>& pts)
{
    m_points = pts;
    if (m_hWnd) Invalidate();
}

void CYieldChart::SetThreshold(double value, COLORREF color)
{
    m_hasThreshold  = true;
    m_threshold     = value;
    m_thresholdColor = color;
    if (m_hWnd) Invalidate();
}

void CYieldChart::SetLineColor(COLORREF c)
{
    m_lineColor = c;
    if (m_hWnd) Invalidate();
}

void CYieldChart::SetRange(double minY, double maxY)
{
    m_hasRange = true;
    m_rangeMin = minY;
    m_rangeMax = maxY;
    if (m_hWnd) Invalidate();
}

BOOL CYieldChart::OnEraseBkgnd(CDC* pDC)
{
    CRect rc; GetClientRect(&rc);
    pDC->FillSolidRect(&rc, BG);
    return TRUE;
}

void CYieldChart::OnPaint()
{
    CPaintDC dc(this);
    CRect client; GetClientRect(&client);

    CDC     memDC;
    CBitmap bmp;
    memDC.CreateCompatibleDC(&dc);
    bmp.CreateCompatibleBitmap(&dc, client.Width(), client.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    memDC.FillSolidRect(&client, BG);

    if (m_points.size() >= 2)
    {
        double minY = m_rangeMin, maxY = m_rangeMax;
        if (!m_hasRange)
        {
            minY = m_points[0].value;
            maxY = m_points[0].value;
            for (const auto& p : m_points)
            {
                minY = min(minY, p.value);
                maxY = max(maxY, p.value);
            }
            if (m_hasThreshold)
            {
                minY = min(minY, m_threshold);
                maxY = max(maxY, m_threshold);
            }
            double pad = (maxY - minY) * 0.1;
            if (pad <= 0.0) pad = 1.0;
            minY -= pad;
            maxY += pad;
        }

        constexpr int kMarginL = 10, kMarginR = 10, kMarginT = 10, kMarginB = 10;
        CRect plot(client.left + kMarginL, client.top + kMarginT,
                   client.right - kMarginR, client.bottom - kMarginB);
        int pw = plot.Width(), ph = plot.Height();

        auto toScreen = [&](size_t idx, double value) -> CPoint
        {
            double t = (m_points.size() > 1) ? (double)idx / (m_points.size() - 1) : 0.0;
            double u = (maxY > minY) ? (value - minY) / (maxY - minY) : 0.5;
            int x = plot.left + static_cast<int>(t * pw);
            int y = plot.bottom - static_cast<int>(u * ph);
            return CPoint(x, y);
        };

        // Grid lines
        for (int i = 0; i <= 4; i++)
        {
            int y = plot.top + ph * i / 4;
            memDC.FillSolidRect(plot.left, y, pw, 1, SEPARATOR);
        }

        using namespace Gdiplus;
        Graphics g(memDC.GetSafeHdc());
        g.SetSmoothingMode(SmoothingModeAntiAlias);

        // Filled area under the trend line
        {
            GraphicsPath path;
            std::vector<Gdiplus::PointF> pts;
            pts.reserve(m_points.size() + 2);
            for (size_t i = 0; i < m_points.size(); i++)
            {
                CPoint p = toScreen(i, m_points[i].value);
                pts.push_back(Gdiplus::PointF((REAL)p.x, (REAL)p.y));
            }
            pts.push_back(Gdiplus::PointF((REAL)plot.right, (REAL)plot.bottom));
            pts.push_back(Gdiplus::PointF((REAL)plot.left,  (REAL)plot.bottom));
            path.AddPolygon(pts.data(), (INT)pts.size());

            SolidBrush fillBrush(Gdiplus::Color(60, GetRValue(m_lineColor), GetGValue(m_lineColor), GetBValue(m_lineColor)));
            g.FillPath(&fillBrush, &path);
        }

        // Threshold — dashed line
        if (m_hasThreshold)
        {
            CPoint a = toScreen(0, m_threshold);
            CPoint b = toScreen(m_points.size() - 1, m_threshold);
            Pen pen(Gdiplus::Color(255, GetRValue(m_thresholdColor), GetGValue(m_thresholdColor), GetBValue(m_thresholdColor)), 1.0f);
            REAL dashPattern[] = { 4.0f, 3.0f };
            pen.SetDashPattern(dashPattern, 2);
            g.DrawLine(&pen, (REAL)a.x, (REAL)a.y, (REAL)b.x, (REAL)b.y);
        }

        // Trend line + markers
        {
            Pen linePen(Gdiplus::Color(255, GetRValue(m_lineColor), GetGValue(m_lineColor), GetBValue(m_lineColor)), 1.6f);
            for (size_t i = 1; i < m_points.size(); i++)
            {
                CPoint a = toScreen(i - 1, m_points[i - 1].value);
                CPoint b = toScreen(i, m_points[i].value);
                g.DrawLine(&linePen, (REAL)a.x, (REAL)a.y, (REAL)b.x, (REAL)b.y);
            }
            SolidBrush markerBrush(Gdiplus::Color(255, GetRValue(m_lineColor), GetGValue(m_lineColor), GetBValue(m_lineColor)));
            for (size_t i = 0; i < m_points.size(); i++)
            {
                CPoint p = toScreen(i, m_points[i].value);
                g.FillEllipse(&markerBrush, (REAL)(p.x - 2), (REAL)(p.y - 2), 4.0f, 4.0f);
            }
        }
    }

    dc.BitBlt(0, 0, client.Width(), client.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}
