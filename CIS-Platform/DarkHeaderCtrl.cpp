#include "pch.h"
#include "framework.h"
#include "DarkHeaderCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CDarkHeaderCtrl, CHeaderCtrl)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CDarkHeaderCtrl::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);
    pDC->FillSolidRect(&rc, Theme::TOP_BG);
    return TRUE;
}

void CDarkHeaderCtrl::OnPaint()
{
    CPaintDC dc(this);

    CRect rcClient;
    GetClientRect(&rcClient);
    dc.FillSolidRect(&rcClient, Theme::TOP_BG);

    CFont* pOldFont = dc.SelectObject(GetParent()->GetFont());
    dc.SetBkMode(TRANSPARENT);

    int count = GetItemCount();
    for (int i = 0; i < count; i++)
    {
        CRect rc;
        GetItemRect(i, &rc);

        TCHAR text[256] = {};
        HDITEM hdi      = {};
        hdi.mask        = HDI_TEXT | HDI_FORMAT;
        hdi.pszText     = text;
        hdi.cchTextMax  = _countof(text);
        GetItem(i, &hdi);

        // Column separator
        dc.FillSolidRect(rc.right - 1, rc.top, 1, rc.Height(), Theme::SEPARATOR);

        // Bottom border
        dc.FillSolidRect(rc.left, rc.bottom - 1, rc.Width(), 1, Theme::SEPARATOR);

        // Label
        rc.DeflateRect(6, 0);
        dc.SetTextColor(Theme::TEXT_DIM);
        dc.DrawText(text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    dc.SelectObject(pOldFont);
}
