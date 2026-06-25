#pragma once
#include "UITheme.h"

class CVisionVisualManager : public CMFCVisualManagerOffice2007
{
	DECLARE_DYNCREATE(CVisionVisualManager)
public:
	COLORREF OnDrawPaneCaption(CDC* pDC, CDockablePane* pBar, BOOL bActive,
	                           CRect rectCaption, CRect rectButtons) override;
};
