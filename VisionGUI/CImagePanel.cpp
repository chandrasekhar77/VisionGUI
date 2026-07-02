#include "pch.h"
#include "framework.h"
#include "CImagePanel.h"
#include "UITheme.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CImagePanel, CWnd)

BEGIN_MESSAGE_MAP(CImagePanel, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEWHEEL()
	ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL CImagePanel::Create(CWnd* pParent, UINT nID)
{
	LPCTSTR cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
		::LoadCursor(nullptr, IDC_ARROW));
	return CWnd::Create(cls, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
		CRect(0,0,0,0), pParent, nID);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool CImagePanel::LoadImage(LPCTSTR path)
{
	m_image.Destroy();
	if (FAILED(m_image.Load(path))) return false;
	m_hasImage = true;
	m_zoom = 1.0; m_panX = 0.0; m_panY = 0.0;

	// Fit to view
	CRect client; GetClientRect(&client);
	if (client.Width() > 0 && client.Height() > 0 && m_image.GetWidth() > 0)
	{
		double zx = (double)client.Width()  / m_image.GetWidth();
		double zy = (double)client.Height() / m_image.GetHeight();
		m_zoom = min(zx, zy);
	}
	Invalidate();
	return true;
}

void CImagePanel::SetRois(std::vector<ROI>* pRois)
{
	m_pRois = pRois;
	Invalidate();
}

void CImagePanel::SetSelectedRoi(int id)
{
	m_selectedId = id;
	Invalidate();
}

void CImagePanel::EnterDrawMode()
{
	m_drawMode = true;
	::SetCursor(::LoadCursor(nullptr, IDC_CROSS));
}

// ---------------------------------------------------------------------------
// Coordinate helpers
// ---------------------------------------------------------------------------

CPoint CImagePanel::ImgToScr(CPoint img) const
{
	return { (int)((img.x - m_panX) * m_zoom),
	         (int)((img.y - m_panY) * m_zoom) };
}

CPoint CImagePanel::ScrToImg(CPoint scr) const
{
	return { (int)(scr.x / m_zoom + m_panX),
	         (int)(scr.y / m_zoom + m_panY) };
}

CRect CImagePanel::ImgToScr(CRect img) const
{
	return { ImgToScr(img.TopLeft()), ImgToScr(img.BottomRight()) };
}

CRect CImagePanel::ScrToImg(CRect scr) const
{
	return { ScrToImg(scr.TopLeft()), ScrToImg(scr.BottomRight()) };
}

void CImagePanel::ClampPan()
{
	if (!m_hasImage) return;
	CRect client; GetClientRect(&client);
	double maxPanX = m_image.GetWidth()  - client.Width()  / m_zoom;
	double maxPanY = m_image.GetHeight() - client.Height() / m_zoom;
	m_panX = max(0.0, min(m_panX, max(0.0, maxPanX)));
	m_panY = max(0.0, min(m_panY, max(0.0, maxPanY)));
}

ROI* CImagePanel::HitRoi(CPoint scr)
{
	if (!m_pRois) return nullptr;
	for (auto& r : *m_pRois)
	{
		CRect scrRc = ImgToScr(r.bounds);
		scrRc.InflateRect(3, 3);
		if (scrRc.PtInRect(scr)) return &r;
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void CImagePanel::OnDraw(CDC& dc)
{
	CRect client; GetClientRect(&client);
	dc.FillSolidRect(&client, RGB(0x12, 0x12, 0x12));

	if (!m_hasImage) {
		dc.SetTextColor(Theme::TEXT_DIM);
		dc.SetBkMode(TRANSPARENT);
		dc.DrawText(_T("Load an image to begin"), &client, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		return;
	}

	// Draw image (scaled)
	int dstW = (int)(m_image.GetWidth()  * m_zoom);
	int dstH = (int)(m_image.GetHeight() * m_zoom);
	int dstX = -(int)(m_panX * m_zoom);
	int dstY = -(int)(m_panY * m_zoom);

	dc.SetStretchBltMode(HALFTONE);
	m_image.StretchBlt(dc.GetSafeHdc(), dstX, dstY, dstW, dstH,
		0, 0, m_image.GetWidth(), m_image.GetHeight(), SRCCOPY);

	if (!m_pRois) return;

	// Draw ROIs
	for (const ROI& r : *m_pRois)
	{
		CRect scrRc = ImgToScr(r.bounds);
		bool  sel   = (r.id == m_selectedId);

		CPen pen(PS_SOLID, sel ? 2 : 1, sel ? Theme::ACCENT : RGB(0xFF, 0xC0, 0x00));
		CPen*   pOldPen = dc.SelectObject(&pen);
		CBrush* pOldBr  = (CBrush*)dc.SelectStockObject(NULL_BRUSH);
		dc.Rectangle(&scrRc);
		dc.SelectObject(pOldPen);
		dc.SelectObject(pOldBr);

		// Name label above ROI
		CRect lblRc(scrRc.left, scrRc.top - 16, scrRc.right, scrRc.top);
		dc.SetTextColor(RGB(0xFF, 0xC0, 0x00));
		dc.SetBkMode(TRANSPARENT);
		dc.DrawText(r.name, &lblRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
	}

	// Rubber band while drawing
	if (m_drawing)
	{
		CPen pen(PS_DOT, 1, Theme::ACCENT);
		CPen*   pOldPen = dc.SelectObject(&pen);
		CBrush* pOldBr  = (CBrush*)dc.SelectStockObject(NULL_BRUSH);
		dc.Rectangle(&m_rubberBand);
		dc.SelectObject(pOldPen);
		dc.SelectObject(pOldBr);
	}

	// Zoom label (top-right corner)
	CString zoomStr; zoomStr.Format(_T("%.0f%%"), m_zoom * 100.0);
	CRect   zoomRc(client.right - 60, client.top + 4, client.right - 4, client.top + 20);
	dc.SetTextColor(Theme::TEXT_DIM);
	dc.SetBkMode(OPAQUE);
	dc.SetBkColor(RGB(0x12, 0x12, 0x12));
	dc.DrawText(zoomStr, &zoomRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

BOOL CImagePanel::OnEraseBkgnd(CDC*) { return TRUE; }

void CImagePanel::OnPaint()
{
	CPaintDC dc(this);
	CRect client; GetClientRect(&client);

	CDC     mem; CBitmap bmp;
	mem.CreateCompatibleDC(&dc);
	bmp.CreateCompatibleBitmap(&dc, client.Width(), client.Height());
	CBitmap* pOld = mem.SelectObject(&bmp);

	// Carry over font from DC
	OnDraw(mem);

	dc.BitBlt(0, 0, client.Width(), client.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(pOld);
}

// ---------------------------------------------------------------------------
// Mouse — left button (draw / select / drag)
// ---------------------------------------------------------------------------

void CImagePanel::OnLButtonDown(UINT, CPoint point)
{
	SetFocus();
	if (m_drawMode)
	{
		m_drawing   = true;
		m_dragStart = point;
		m_rubberBand = CRect(point, point);
		SetCapture();
		return;
	}

	ROI* pHit = HitRoi(point);
	int prevSel = m_selectedId;
	m_selectedId = pHit ? pHit->id : -1;
	if (pHit)
	{
		m_draggingRoi    = true;
		m_roiDragMouse   = point;
		m_roiOrigBounds  = pHit->bounds;
		SetCapture();
	}
	if (m_selectedId != prevSel)
	{
		Invalidate();
		GetParent()->PostMessage(Theme::WM_ROI_SELECTED, (WPARAM)m_selectedId, 0);
	}
}

void CImagePanel::OnLButtonUp(UINT, CPoint point)
{
	if (m_drawing)
	{
		m_drawing = false;
		ReleaseCapture();

		CRect imgRc = ScrToImg(m_rubberBand);
		imgRc.NormalizeRect();

		if (imgRc.Width() > 4 && imgRc.Height() > 4)
		{
			// Clamp to image
			CRect imgBounds(0, 0, m_image.GetWidth(), m_image.GetHeight());
			imgRc.IntersectRect(&imgRc, &imgBounds);
			m_pendingBounds = imgRc;
			GetParent()->PostMessage(Theme::WM_ROI_ADDED, 0, 0);
		}
		m_rubberBand = CRect(0,0,0,0);
		m_drawMode   = false;
		::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
		Invalidate();
		return;
	}

	if (m_draggingRoi)
	{
		m_draggingRoi = false;
		ReleaseCapture();
	}
}

void CImagePanel::OnMouseMove(UINT, CPoint point)
{
	if (m_drawing)
	{
		m_rubberBand = CRect(m_dragStart, point);
		m_rubberBand.NormalizeRect();
		Invalidate();
		return;
	}

	if (m_panning)
	{
		double dx = (point.x - m_panMouseStart.x) / m_zoom;
		double dy = (point.y - m_panMouseStart.y) / m_zoom;
		m_panX = m_panXStart - dx;
		m_panY = m_panYStart - dy;
		ClampPan();
		Invalidate();
		return;
	}

	if (m_draggingRoi && m_pRois)
	{
		int dx = (int)((point.x - m_roiDragMouse.x) / m_zoom);
		int dy = (int)((point.y - m_roiDragMouse.y) / m_zoom);
		ROI* pRoi = nullptr;
		for (auto& r : *m_pRois) if (r.id == m_selectedId) { pRoi = &r; break; }
		if (pRoi)
		{
			CSize sz = m_roiOrigBounds.Size();
			pRoi->bounds.left   = m_roiOrigBounds.left + dx;
			pRoi->bounds.top    = m_roiOrigBounds.top  + dy;
			pRoi->bounds.right  = pRoi->bounds.left + sz.cx;
			pRoi->bounds.bottom = pRoi->bounds.top  + sz.cy;
			Invalidate();
		}
	}

	if (m_drawMode)
		::SetCursor(::LoadCursor(nullptr, IDC_CROSS));
}

// ---------------------------------------------------------------------------
// Mouse — right button (pan)
// ---------------------------------------------------------------------------

void CImagePanel::OnRButtonDown(UINT, CPoint point)
{
	m_panning      = true;
	m_panMouseStart = point;
	m_panXStart    = m_panX;
	m_panYStart    = m_panY;
	SetCapture();
	::SetCursor(::LoadCursor(nullptr, IDC_SIZEALL));
}

void CImagePanel::OnRButtonUp(UINT, CPoint)
{
	m_panning = false;
	ReleaseCapture();
	::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
}

// ---------------------------------------------------------------------------
// Mouse wheel — zoom anchored to cursor
// ---------------------------------------------------------------------------

BOOL CImagePanel::OnMouseWheel(UINT, short delta, CPoint screenPt)
{
	if (!m_hasImage) return TRUE;
	ScreenToClient(&screenPt);

	CPoint imgPt = ScrToImg(screenPt);

	double factor = (delta > 0) ? 1.2 : 1.0 / 1.2;
	m_zoom = max(0.05, min(20.0, m_zoom * factor));

	m_panX = imgPt.x - screenPt.x / m_zoom;
	m_panY = imgPt.y - screenPt.y / m_zoom;
	ClampPan();
	Invalidate();
	return TRUE;
}

void CImagePanel::OnSize(UINT, int, int) { ClampPan(); Invalidate(); }
