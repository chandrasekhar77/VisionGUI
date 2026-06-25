#include "pch.h"
#include "framework.h"
#include "VisionVisualManager.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CVisionVisualManager, CMFCVisualManagerOffice2007)

// Draw the dockable pane caption bar and return the text color.
// The MFC caller uses the returned COLORREF to draw the title string.
COLORREF CVisionVisualManager::OnDrawPaneCaption(CDC* pDC, CDockablePane* pBar,
	BOOL bActive, CRect rectCaption, CRect rectButtons)
{
	using namespace Theme;

	pDC->FillSolidRect(&rectCaption, bActive ? HOVER : TOP_BG);
	pDC->FillSolidRect(rectCaption.left, rectCaption.bottom - 1,
		rectCaption.Width(), 1, SEPARATOR);

	return bActive ? TEXT : TEXT_DIM;
}
