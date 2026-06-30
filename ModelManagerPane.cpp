#include "pch.h"
#include "framework.h"
#include "ModelManagerPane.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CModelManagerPane, CDockablePane)

BEGIN_MESSAGE_MAP(CModelManagerPane, CDockablePane)
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

void CModelManagerPane::OnSize(UINT nType, int cx, int cy)
{
    CDockablePane::OnSize(nType, cx, cy);
    Invalidate(FALSE);
    UpdateWindow();

    static bool bInFrameUpdate = false;
    if (!bInFrameUpdate)
    {
        bInFrameUpdate = true;
        SetWindowPos(nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        bInFrameUpdate = false;
    }
}

BOOL CModelManagerPane::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);
    pDC->FillSolidRect(&rc, Theme::BG);
    return TRUE;
}
