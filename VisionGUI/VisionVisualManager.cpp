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

void CVisionVisualManager::OnDrawPaneDivider(CDC* pDC, CPaneDivider* pSlider,
	CRect rect, BOOL bAutoHideMode)
{
	using namespace Theme;

	pDC->FillSolidRect(&rect, BG);

	// Check whether the cursor is currently over this divider window
	CPoint pt;
	GetCursorPos(&pt);
	pSlider->ScreenToClient(&pt);
	BOOL bHovered = rect.PtInRect(pt);

	if (pSlider->IsHorizontal())
	{
		int y = rect.CenterPoint().y;
		pDC->FillSolidRect(rect.left, y, rect.Width(), 1,
			bHovered ? ACCENT : SEPARATOR);
	}
	else
	{
		int x = rect.CenterPoint().x;
		pDC->FillSolidRect(x, rect.top, 1, rect.Height(),
			bHovered ? ACCENT : SEPARATOR);
	}
}
