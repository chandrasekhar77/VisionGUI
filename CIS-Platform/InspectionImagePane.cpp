#include "pch.h"
#include "framework.h"
#include "InspectionImagePane.h"
#include "HiResImageRenderer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CInspectionImagePane, CDockablePane)

BEGIN_MESSAGE_MAP(CInspectionImagePane, CDockablePane)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

int CInspectionImagePane::OnCreate(LPCREATESTRUCT lpcs)
{
    if (CDockablePane::OnCreate(lpcs) == -1) return -1;

    m_viewer.Create(CRect(0, 0, 0, 0), this, 1);

    m_font.CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

    m_lblNoImage.Create(_T("No image loaded"),
        WS_CHILD | SS_CENTER | SS_CENTERIMAGE | SS_NOTIFY,
        CRect(0, 0, 0, 0), this);
    m_lblNoImage.SetFont(&m_font);

    return 0;
}

void CInspectionImagePane::OnSize(UINT nType, int cx, int cy)
{
    CDockablePane::OnSize(nType, cx, cy);
    if (m_viewer.m_hWnd)     m_viewer.MoveWindow(0, 0, cx, cy);
    if (m_lblNoImage.m_hWnd) m_lblNoImage.MoveWindow(0, 0, cx, cy);

    // Deferred one-shot seed: the D3D viewport must be initialized against
    // the pane's real docked size, not whatever transient size Create() saw
    // before the docking manager laid it out -- otherwise ImgEngine_Initialize
    // sets up a ~1px viewport and every zoom/marker computation that follows
    // is garbage (fit-zoom rounds to 0%, effectively invisible).
    if (!m_demoSeeded && cx > 0 && cy > 0)
    {
        m_demoSeeded = true;

        // Seed with the bundled demo asset + a handful of placeholder markers —
        // no real inspection pipeline exists yet to supply these.
        std::vector<DefectInfo> demoDefects;
        const int demoCoords[][2] = {
            { 300, 220 }, { 900, 450 }, { 1500, 300 }, { 620, 900 },
            { 1200, 1050 }, { 1800, 700 }, { 150, 1200 }, { 1050, 650 },
        };
        for (const auto& c : demoCoords)
        {
            DefectInfo d{};
            d.x = c[0];
            d.y = c[1];
            demoDefects.push_back(d);
        }

        CString exeDir;
        GetModuleFileName(AfxGetInstanceHandle(), exeDir.GetBuffer(MAX_PATH), MAX_PATH);
        exeDir.ReleaseBuffer();
        exeDir = exeDir.Left(exeDir.ReverseFind(_T('\\')) + 1);
        CString demoPath = exeDir + _T("res\\DemoPanel.bmp");

        LoadPanelImageWithMarkers(demoPath, demoDefects);
    }
}

BOOL CInspectionImagePane::OnEraseBkgnd(CDC* pDC)
{
    CRect rc; GetClientRect(&rc);
    pDC->FillSolidRect(&rc, Theme::BG);
    return TRUE;
}

void CInspectionImagePane::LoadPanelImageWithMarkers(LPCTSTR path, const std::vector<DefectInfo>& defects)
{
    bool ok = m_viewer.LoadImageFile(path);
    m_lblNoImage.ShowWindow(ok ? SW_HIDE : SW_SHOW);
    if (!ok) return;

    // Guarantees the known fit-to-window state (zoom = fit, pan = (0,0))
    // that PlaceCircleMarker's image->screen math assumes.
    m_viewer.FitToWindow();

    for (const auto& d : defects)
        PlaceCircleMarker(d.x, d.y);

    m_viewer.SetShapeTool(IMG_SHAPE_NONE);
}

void CInspectionImagePane::PlaceCircleMarker(int imageX, int imageY, int radiusPx)
{
    ImgEngineHandle engine = m_viewer.GetEngineId();
    if (engine == IMG_INVALID_HANDLE) return;

    ImgParamMap* in  = ImgParamMap_Create();
    ImgParamMap* out = ImgParamMap_Create();
    ImgEngine_Query(engine, in, out);

    double  zoom = 1.0;
    int64_t imgW = 0, imgH = 0;
    ImgParamMap_TryGetFloat(out, IMG_KEY_ZOOM, &zoom);
    ImgParamMap_TryGetInt(out, IMG_KEY_IMAGE_WIDTH, &imgW);
    ImgParamMap_TryGetInt(out, IMG_KEY_IMAGE_HEIGHT, &imgH);
    ImgParamMap_Destroy(in);
    ImgParamMap_Destroy(out);
    if (imgW <= 0 || imgH <= 0) return;

    CRect rc;
    m_viewer.GetClientRect(&rc);

    // Same transform as D3DRenderer::ImageToScreen, specialized for the
    // pan == (0,0) state FitToWindow() guarantees: image centered in the
    // viewport, scaled by the fit zoom.
    int cx = static_cast<int>(rc.Width()  / 2.0 + zoom * (imageX - imgW / 2.0));
    int cy = static_cast<int>(rc.Height() / 2.0 + zoom * (imageY - imgH / 2.0));

    m_viewer.SetShapeTool(IMG_SHAPE_CIRCLE);

    LPARAM lpCenter = MAKELPARAM(cx, cy);
    LPARAM lpEdge   = MAKELPARAM(cx + radiusPx, cy);
    m_viewer.SendMessage(WM_LBUTTONDOWN, MK_LBUTTON, lpCenter);
    m_viewer.SendMessage(WM_MOUSEMOVE,   MK_LBUTTON, lpEdge);
    m_viewer.SendMessage(WM_LBUTTONUP,   0,          lpEdge);
}
