#pragma once
#include "PCBModel.h"
#include <atlimage.h>

// Custom control that displays a golden PCB image with:
//   - Mouse wheel zoom (anchored to cursor)
//   - Right-drag pan
//   - ROI overlay (selection, rubber-band draw)
//
// Posts to parent:
//   WM_ROI_ADDED    — after user draws a new ROI; call GetPendingBounds() to retrieve it
//   WM_ROI_SELECTED — wParam = selected ROI id, or -1 when deselected

class CImagePanel : public CWnd
{
	DECLARE_DYNAMIC(CImagePanel)
public:
	BOOL Create(CWnd* pParent, UINT nID);

	bool LoadImage(LPCTSTR path);
	void SetRois(std::vector<ROI>* pRois);
	void SetSelectedRoi(int id);
	void EnterDrawMode();
	bool HasImage() const { return m_hasImage; }

	CRect GetPendingBounds() const { return m_pendingBounds; }

private:
	CImage           m_image;
	bool             m_hasImage    = false;

	double           m_zoom        = 1.0;
	double           m_panX        = 0.0;   // image-space X of view top-left
	double           m_panY        = 0.0;

	std::vector<ROI>* m_pRois      = nullptr;
	int              m_selectedId  = -1;

	// Draw mode state
	bool             m_drawMode    = false;
	bool             m_drawing     = false;
	CPoint           m_dragStart;
	CRect            m_rubberBand;
	CRect            m_pendingBounds;

	// Pan state
	bool             m_panning     = false;
	CPoint           m_panMouseStart;
	double           m_panXStart   = 0.0;
	double           m_panYStart   = 0.0;

	// ROI drag state
	bool             m_draggingRoi = false;
	CPoint           m_roiDragMouse;
	CRect            m_roiOrigBounds;

	// Coordinate helpers
	CPoint  ImgToScr(CPoint img)  const;
	CPoint  ScrToImg(CPoint scr)  const;
	CRect   ImgToScr(CRect   img) const;
	CRect   ScrToImg(CRect   scr) const;
	ROI*    HitRoi  (CPoint  scr);
	void    ClampPan();

	void OnDraw(CDC& dc);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC*);
	afx_msg void OnLButtonDown(UINT, CPoint);
	afx_msg void OnLButtonUp  (UINT, CPoint);
	afx_msg void OnMouseMove  (UINT, CPoint);
	afx_msg void OnRButtonDown(UINT, CPoint);
	afx_msg void OnRButtonUp  (UINT, CPoint);
	afx_msg BOOL OnMouseWheel (UINT, short, CPoint);
	afx_msg void OnSize       (UINT, int, int);
	DECLARE_MESSAGE_MAP()
};
