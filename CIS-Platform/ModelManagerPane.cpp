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
    PostMessage(WM_NCPAINT, 1, 0);
}

BOOL CModelManagerPane::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);
    pDC->FillSolidRect(&rc, Theme::BG);
    return TRUE;
}
