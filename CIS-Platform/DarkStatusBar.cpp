#include "pch.h"
#include "framework.h"
#include "DarkStatusBar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CDarkStatusBar, CStatusBar)

BEGIN_MESSAGE_MAP(CDarkStatusBar, CStatusBar)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
END_MESSAGE_MAP()

BOOL CDarkStatusBar::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(&rc);
	pDC->FillSolidRect(&rc, Theme::BG);
	return TRUE;
}

void CDarkStatusBar::OnPaint()
{
	CPaintDC dc(this);
	CRect rcClient;
	GetClientRect(&rcClient);
	dc.FillSolidRect(&rcClient, Theme::BG);

	CFont* pOldFont = dc.SelectObject(GetFont());
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(Theme::TEXT);

	int nCount = GetCount();
	for (int i = 0; i < nCount; i++)
	{
		CRect rcPane;
		GetItemRect(i, &rcPane);

		// Subtle separator between panes
		if (i > 0)
			dc.FillSolidRect(rcPane.left, rcPane.top + 2, 1, rcPane.Height() - 4, RGB(0x3A, 0x3A, 0x3A));

		CString text;
		GetPaneText(i, text);
		if (!text.IsEmpty())
		{
			rcPane.DeflateRect(6, 0);
			dc.DrawText(text, &rcPane, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
		}
	}

	if (pOldFont) dc.SelectObject(pOldFont);
}
