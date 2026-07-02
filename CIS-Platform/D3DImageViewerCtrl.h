// D3DImageViewerCtrl.h
// ---------------------------------------------------------------------------
// MFC control that hosts the Direct3D 11 image-load/render engine, which now
// lives in HiResImageRenderer.dll (see HiResImageRenderer.h for the stable C ABI).
// This control is the thin MFC-facing wrapper: it owns an engine handle plus
// a reusable pair of parameter maps, and translates Windows messages / CString
// paths into calls across that stable boundary.
//
// It is a CWnd (not a CStatic) with its own registered window class so that it
// reliably receives ALL mouse input, including the mouse wheel (a plain static
// control does not take focus and would not get WM_MOUSEWHEEL).
//
// Typical use: place a placeholder control in the dialog editor, then create
// this viewer over it in OnInitDialog (see README.md for exact steps).
//
// NOTE: include this header AFTER your MFC precompiled header (pch.h /
// stdafx.h) so the MFC types (CWnd, CString) are already available.
// ---------------------------------------------------------------------------
#pragma once

#include "HiResImageRenderer.h"
#include "PerfMonitor.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>

// Sent to the parent window whenever the view (zoom and/or pan) changes as a
// result of input handled inside the control (mouse-wheel zoom, right-click
// reset). wParam = control ID (GetDlgCtrlID()), lParam = unused. Hosts that
// display zoom/position in a status bar should handle this to refresh it;
// it is optional, so the control still works in hosts that ignore it.
#define WM_D3DVIEWER_VIEWCHANGED (WM_APP + 0x100)

// Sent to the parent window on every mouse move over the control (regardless
// of button state), once GetHoverText() has been refreshed. wParam = control
// ID. Kept separate from WM_D3DVIEWER_VIEWCHANGED since it fires far more
// often and the parent's handler for it should stay cheap (just read the
// already-computed GetHoverText(), no DLL round-trip of its own).
#define WM_D3DVIEWER_HOVERCHANGED (WM_APP + 0x101)

// Sent to the parent window when a LoadImageFileAsync() started on this
// control finishes, success or failure. wParam = control ID, lParam = TRUE/
// FALSE for success. The parent should re-enable whatever controls it
// disabled while the load was running and refresh its status text; on
// failure it should show its own message (the control's own m_lastError
// already covers the generic case, same as the synchronous LoadImageFile).
#define WM_D3DVIEWER_LOADCOMPLETE (WM_APP + 0x102)

// Describes one selectable rendering backend for the hardware-picker combo
// box: either a specific GPU (DXGI adapter) or the WARP software rasterizer.
// This is a UI-facing convenience type owned by the control; the engine DLL
// only ever deals in primitives (see HiResImageRenderer.h's adapter_* keys).
struct D3DAdapterInfo
{
    std::wstring description;       // friendly name to show the user
    bool         isWarp = false;    // true => WARP (CPU) instead of a GPU
    UINT         adapterIndex = 0;  // DXGI adapter index; unused when isWarp
};

class CD3DImageViewerCtrl : public CWnd
{
public:
    CD3DImageViewerCtrl() = default;
    virtual ~CD3DImageViewerCtrl();

    // Create the viewer as a child window of pParent at the given rect.
    BOOL Create(const RECT& rect, CWnd* pParent, UINT nID);

    // Decode + display an image file. Returns false on failure. Blocks the
    // calling thread for the whole decode + first GPU upload -- prefer
    // LoadImageFileAsync for anything driven by user interaction.
    bool LoadImageFile(LPCTSTR path);

    // Same, but the actual ImgEngine_LoadImage + first ImgEngine_Render
    // (which performs the potentially slow on-demand GPU tile upload for a
    // large image -- see RenderTiles in the DLL) run on a background thread,
    // so the caller's UI thread is never blocked. The control shows its own
    // spinner overlay for the duration and posts WM_D3DVIEWER_LOADCOMPLETE
    // to its parent when done (see that message's comment for what the
    // parent should do). While a load is in flight, this instance ignores
    // mouse/keyboard input and further Load/Switch/Fit/SetShape* calls --
    // m_inMap/m_outMap are not safe to touch from two threads at once, so
    // the background thread uses its own, separate ImgParamMaps instead.
    void LoadImageFileAsync(LPCTSTR path);
    bool IsLoading() const { return m_loading; }

    // Reset to fit-to-window.
    void FitToWindow();

    // Image info line for a label / status bar: resolution, file size,
    // decoded texture memory, last load time, and zoom %.
    CString GetStatusText() const;

    bool IsRendererReady() const { return m_ready; }

    // Enumerate selectable rendering backends (GPUs + WARP) for a hardware
    // picker UI. Safe to call before the control is created.
    static std::vector<D3DAdapterInfo> EnumerateAdapters();

    // Index into EnumerateAdapters() for the backend currently in use, or -1
    // before the device has been created.
    int GetCurrentAdapterIndex() const { return m_currentAdapterIndex; }

    // Re-create the D3D device on a different adapter (GPU or WARP) and, if
    // an image was loaded, reload it so the view is preserved. adapterListIndex
    // is the index of `adapter` in EnumerateAdapters(), recorded so
    // GetCurrentAdapterIndex() stays accurate. Returns false if the new
    // adapter could not be initialized.
    bool SwitchAdapter(const D3DAdapterInfo& adapter, int adapterListIndex);

    // Re-samples and formats CPU%/GPU% usage for this process, for a perf
    // label. Not const: sampling updates internal state. The hardware name
    // is intentionally omitted here -- it's already shown by the adapter combo.
    CString GetPerfText();

    // The underlying engine instance ID (see HiResImageRenderer.h), for UI
    // that wants to show which concrete instance a viewer maps to. May be
    // IMG_INVALID_HANDLE before the control has been created.
    ImgEngineHandle GetEngineId() const { return m_engine; }

    // Arms a shape-drawing tool (IMG_SHAPE_RECTANGLE/CIRCLE/LINE/POLYGON from
    // HiResImageRenderer.h), or IMG_SHAPE_NONE for plain selection mode. The
    // DLL owns all shape state; this just forwards the request.
    void SetShapeTool(int shapeType);

    // Cheap cached mirror of the engine's current shape tool. Can change
    // without a SetShapeTool call (a draw tool auto-reverts to
    // IMG_SHAPE_NONE once its shape is finished), so callers syncing a
    // tool-button UI should re-read this after WM_D3DVIEWER_VIEWCHANGED
    // rather than assuming it only changes when THEY call SetShapeTool.
    int GetCurrentShapeTool() const { return m_currentShapeTool; }

    // Switches the coordinate-origin convention used for shape label text
    // and the hover-coordinate readout (IMG_COORD_ORIGIN_TOP_LEFT/_BOTTOM_LEFT
    // from HiResImageRenderer.h). Default is top-left.
    void SetCoordinateOrigin(int origin);
    int  GetCoordinateOrigin() const { return m_coordOrigin; }

    // "X: ###   Y: ###" for whatever image-pixel point the mouse was last
    // over, already formatted in the current coordinate-origin convention.
    // Updated on every OnMouseMove; empty before the first move over an
    // initialized viewer with an image loaded.
    CString GetHoverText() const { return m_hoverText; }

    // Whether closed shapes (Rectangle, Circle, a finished Polygon) also
    // render a translucent fill in addition to their outline. The DLL owns
    // this (it affects the actual GPU vertex data it generates); default is
    // false (outline only).
    void SetShapeFilled(bool filled);
    bool GetShapeFilled() const { return m_shapeFilled; }

    // Whether DrawShapeLabels() draws anything at all. Purely a local
    // EXE-side display preference -- it decides whether to bother calling
    // ImgEngine_Shape_GetLabels and drawing the result, with no bearing on
    // any DLL-owned shape state, so unlike SetShapeFilled it never crosses
    // the engine boundary. Default is true (labels shown).
    void SetShowLabels(bool show);
    bool GetShowLabels() const { return m_showLabels; }

protected:
    afx_msg int  OnCreate(LPCREATESTRUCT lpcs);
    afx_msg void OnDestroy();
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg LRESULT OnLoadThreadDone(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    bool EnsureInitialized();
    void PaintError(CDC& dc);
    void NotifyViewChanged();
    void NotifyHoverChanged();

    // Draws each shape's text label (Rectangle start/end+W/H, Circle
    // diameter, Line length) on top of whatever ImgEngine_Render() just
    // drew, via plain GDI TextOut -- D3D11 has no built-in text rendering.
    // Uses its own fresh CClientDC, called AFTER OnPaint's CPaintDC has
    // already gone out of scope (EndPaint'd) -- mirrors the dialog's green
    // active-viewer border technique exactly (CClientDC, post-paint-cycle),
    // which is the combination already proven to layer correctly on top of
    // a D3D-painted window in this app.
    void DrawShapeLabels();

    // Re-queries the engine for m_ready/m_currentAdapterIndex/m_hasImage.
    // Called after Initialize/SwitchAdapter so the control's cheap cached
    // accessors reflect the engine's true state, including rollback outcomes.
    void RefreshStateFromEngine();

    // Small spinner overlay shown while m_loading is true, drawn via plain
    // GDI (a rotating arc, plus the elapsed-time text inside it) on a fresh
    // CClientDC -- same post-paint-cycle technique as DrawShapeLabels.
    // kSpinnerTimerId advances m_spinnerAngle on a SetTimer tick and
    // invalidates to animate it; the elapsed time is just GetTickCount64()
    // minus m_loadStartTickMs, recomputed fresh on every repaint.
    void DrawSpinnerOverlay();
    static constexpr UINT_PTR kSpinnerTimerId = 100;
    static constexpr int kSpinnerRadius = 88;     // doubled twice from the original 22
    static constexpr int kSpinnerPenWidth = 20;   // doubled twice from the original 5

    // Rollback-on-failure and image-reload-after-switch logic now live inside
    // the engine (it's the only side with both device lifecycle and
    // last-image-path state); the control only caches cheap, read-only
    // mirrors of engine state for hot paths (mouse move/wheel) and accessors.
    ImgEngineHandle m_engine = IMG_INVALID_HANDLE;
    ImgParamMap*    m_inMap  = nullptr;  // reused across calls to avoid per-call allocation
    ImgParamMap*    m_outMap = nullptr;
    PerfMonitor m_perf;
    bool    m_ready    = false;
    bool    m_hasImage = false;
    bool    m_comInit  = false;
    bool    m_dragging = false;
    CPoint  m_lastMouse{ 0, 0 };
    CString m_lastError;
    int     m_currentAdapterIndex = -1; // index into EnumerateAdapters(), mirrors the engine's
    int     m_currentShapeTool = IMG_SHAPE_NONE; // mirrors the engine's current Shape_* tool
    int     m_coordOrigin = IMG_COORD_ORIGIN_TOP_LEFT; // mirrors the engine's current Shape_* coordinate origin
    CString m_hoverText;     // last mouse-hover coordinate readout, see GetHoverText()
    int     m_cursorShapeType = IMG_SHAPE_NONE; // mirrors the engine's cursor_shape_type from the last OnMouseMove
    bool    m_cursorIsGenericMove = false; // mirrors the engine's cursor_is_move from the last OnMouseMove
    bool    m_shapeFilled = false; // mirrors the engine's current Shape_* filled setting
    bool    m_showLabels  = true;  // EXE-only; see SetShowLabels

    // ---- Async load (LoadImageFileAsync) ----
    std::thread       m_loadThread;
    std::atomic<bool> m_loading{ false };
    float             m_spinnerAngle = 0.0f;
    ULONGLONG         m_loadStartTickMs = 0; // GetTickCount64() when the current load began
};
