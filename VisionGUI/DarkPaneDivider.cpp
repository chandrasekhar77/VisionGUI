#include "pch.h"
#include "framework.h"
#include "DarkPaneDivider.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CDarkPaneDivider, CPaneDivider)

BEGIN_MESSAGE_MAP(CDarkPaneDivider, CPaneDivider)
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_MESSAGE(WM_MOUSELEAVE, &CDarkPaneDivider::OnMouseLeave)
END_MESSAGE_MAP()

void CDarkPaneDivider::OnMouseMove(UINT nFlags, CPoint point)
{
    CPaneDivider::OnMouseMove(nFlags, point);

    if (!m_bTracking)
    {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize    = sizeof(tme);
        tme.dwFlags   = TME_LEAVE;
        tme.hwndTrack = m_hWnd;
        TrackMouseEvent(&tme);
        m_bTracking = true;
        Invalidate(FALSE);
    }
}

void CDarkPaneDivider::OnLButtonUp(UINT nFlags, CPoint point)
{
    CPaneDivider::OnLButtonUp(nFlags, point); // calls StopTracking → ReleaseCapture → Invalidate

    // SetCapture during drag silently cancels TrackMouseEvent, leaving m_bTracking stale.
    // Reset it so the next hover re-registers tracking and triggers a repaint.
    m_bTracking = false;
}

LRESULT CDarkPaneDivider::OnMouseLeave(WPARAM, LPARAM)
{
    m_bTracking = false;
    Invalidate(FALSE);
    return 0;
}
