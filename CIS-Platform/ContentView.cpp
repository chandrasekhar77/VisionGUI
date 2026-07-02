#include "pch.h"
#include "framework.h"
#include "CIS-Platform.h"
#include "ContentView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CContentView, CWnd)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

BOOL CContentView::Create(LPCTSTR label, CWnd* pParent, UINT nID)
{
	m_label = label;

	LPCTSTR cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
		::LoadCursor(nullptr, IDC_ARROW),
		(HBRUSH)::GetStockObject(NULL_BRUSH), nullptr);

	return CWnd::Create(cls, _T(""),
		WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		CRect(0, 0, 0, 0), pParent, nID);
}

BOOL CContentView::PreCreateWindow(CREATESTRUCT& cs)
{
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.style     &= ~WS_BORDER;
	return CWnd::PreCreateWindow(cs);
}

int CContentView::OnCreate(LPCREATESTRUCT lpcs)
{
	if (CWnd::OnCreate(lpcs) == -1) return -1;

	m_font.CreateFont(20, 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));
	return 0;
}

void CContentView::OnPaint()
{
	CPaintDC dc(this);
	CRect rc;
	GetClientRect(&rc);

	dc.FillSolidRect(&rc, Theme::BG);

	if (!m_label.IsEmpty())
	{
		dc.SetTextColor(Theme::TEXT_DIM);
		dc.SetBkMode(TRANSPARENT);
		CFont* pOld = dc.SelectObject(&m_font);
		dc.DrawText(m_label, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		dc.SelectObject(pOld);
	}
}

BOOL CContentView::OnEraseBkgnd(CDC* /*pDC*/)
{
	return TRUE;
}

void CContentView::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
	ClientToScreen(&point);
	ReleaseCapture();
	GetParentFrame()->PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
}
