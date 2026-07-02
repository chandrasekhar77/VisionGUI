#include "pch.h"
#include "framework.h"
#include "DarkPane.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CDarkPane, CDockablePane)

BEGIN_MESSAGE_MAP(CDarkPane, CDockablePane)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
END_MESSAGE_MAP()

BOOL CDarkPane::OnEraseBkgnd(CDC* pDC)
{
	CRect rc;
	GetClientRect(&rc);
	pDC->FillSolidRect(&rc, Theme::BG);
	return TRUE;
}

void CDarkPane::OnPaint()
{
	CPaintDC dc(this);
	CRect rc;
	GetClientRect(&rc);
	dc.FillSolidRect(&rc, Theme::BG);
}
