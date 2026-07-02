#pragma once
#include "UITheme.h"
#include "D3DImageViewerCtrl.h"
#include "Contracts/DefectInfo.h"
#include <vector>

// Leftmost dockpane — hosts the vendored Direct3D 11 high-resolution image
// viewer control (see D3DImageViewerCtrl.h / HiResImageRenderer.dll) showing
// the current panel image with defect markers overlaid via the control's
// existing vector-shape system (no DLL/ABI changes needed).
class CInspectionImagePane : public CDockablePane
{
    DECLARE_DYNAMIC(CInspectionImagePane)

public:
    CInspectionImagePane() = default;

    // Loads the image and drops a small circle marker at each defect's
    // image-pixel coordinate. Markers stay anchored to the image content
    // across future zoom/pan (the DLL stores shape geometry in image space).
    void LoadPanelImageWithMarkers(LPCTSTR path, const std::vector<DefectInfo>& defects);

protected:
    CD3DImageViewerCtrl m_viewer;
    CStatic             m_lblNoImage;
    CFont               m_font;
    bool                m_demoSeeded = false; // one-shot: seeded on the first real (nonzero) OnSize

    // Converts an image-pixel coordinate to the viewer's current screen-space
    // coordinate (valid only in the fit-to-window state -- zoom from
    // ImgEngine_Query, pan == 0 -- which LoadPanelImageWithMarkers guarantees
    // by calling FitToWindow() right before placing markers) and synthesizes
    // the drag-to-draw mouse sequence the control's Shape system expects.
    void PlaceCircleMarker(int imageX, int imageY, int radiusPx = 10);

    afx_msg int  OnCreate(LPCREATESTRUCT lpcs);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()
};
