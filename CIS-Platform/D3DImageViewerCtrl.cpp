// D3DImageViewerCtrl.cpp
// ---------------------------------------------------------------------------
// NOTE: this project uses "pch.h" as the precompiled header. If you reuse this
// file in an older project that uses "stdafx.h", change the include below.
//
// All Direct3D/WIC work happens in HiResImageRenderer.dll; every method below
// just builds an ImgParamMap, calls across that boundary, and reads the
// result back into MFC types (CString, etc.).
// ---------------------------------------------------------------------------
#include "pch.h"
#include "D3DImageViewerCtrl.h"
#include <objbase.h>
#include <atlconv.h>   // CStringW conversion helpers
#include <cmath>       // std::cos/std::sin for DrawSpinnerOverlay

// Private to this .cpp: posted by LoadImageFileAsync's background thread to
// this control's OWN hwnd when the load finishes, so the finishing touches
// (m_loading, m_hasImage, Invalidate, notifying the parent) happen back on
// the UI thread. Distinct from the public WM_D3DVIEWER_LOADCOMPLETE (control
// -> parent) in the header -- that one is sent only after this handler has
// already brought the control's own state up to date.
#define WM_D3DVIEWER_INTERNAL_LOADDONE (WM_APP + 0x190)

namespace
{
    CString FormatBytes(UINT64 bytes)
    {
        const double kb = 1024.0, mb = kb * 1024.0;
        CString s;
        if (bytes >= static_cast<UINT64>(mb))
            s.Format(_T("%.2f MB"), bytes / mb);
        else if (bytes >= static_cast<UINT64>(kb))
            s.Format(_T("%.0f KB"), bytes / kb);
        else
            s.Format(_T("%llu B"), bytes);
        return s;
    }

    // Two-call (size-then-fetch) read of a string-valued key, using a plain
    // buffer rather than std::wstring::data() (writing to a wstring's own
    // terminator slot is undefined behavior in C++17).
    CString ReadStringOr(const ImgParamMap* map, const char* key, LPCTSTR fallback)
    {
        const int32_t needed = ImgParamMap_TryGetString(map, key, nullptr, 0);
        if (needed <= 0) return fallback;
        std::vector<wchar_t> buf(static_cast<size_t>(needed));
        ImgParamMap_TryGetString(map, key, buf.data(), needed);
        return CString(buf.data());
    }

    // fmt is one of the IMG_KEY_ADAPTER_*_FMT constants (each takes one %d).
    CStringA AdapterKey(int64_t i, const char* fmt)
    {
        CStringA s;
        s.Format(fmt, static_cast<int>(i));
        return s;
    }

    // Builds a small custom cursor at runtime via GDI -- no .cur resource
    // files needed, consistent with this app's "generate at startup"
    // approach elsewhere (e.g. HLSL shaders compiled via D3DCompile rather
    // than precompiled .cso files). The cursor is the standard arrow (tip at
    // the hotspot, 0,0) plus a small outline icon of the given shape type
    // just below-right of it, so the user can tell at a glance which shape
    // they're about to move/resize.
    HCURSOR CreateShapeCursor(int shapeType)
    {
        constexpr int kSize = 32;
        const COLORREF kSentinel = RGB(1, 254, 1); // background; never drawn over, recolored to transparent below

        HDC screenDc = ::GetDC(nullptr);
        HDC memDc = ::CreateCompatibleDC(screenDc);
        ::ReleaseDC(nullptr, screenDc);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = kSize;
        bmi.bmiHeader.biHeight = -kSize; // negative = top-down DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP colorBmp = ::CreateDIBSection(memDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!colorBmp) { ::DeleteDC(memDc); return nullptr; }
        HBITMAP oldBmp = static_cast<HBITMAP>(::SelectObject(memDc, colorBmp));

        HBRUSH sentinelBrush = ::CreateSolidBrush(kSentinel);
        RECT full = { 0, 0, kSize, kSize };
        ::FillRect(memDc, &full, sentinelBrush);
        ::DeleteObject(sentinelBrush);

        HPEN blackPen = ::CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        HBRUSH whiteBrush = ::CreateSolidBrush(RGB(255, 255, 255));
        HPEN oldPen = static_cast<HPEN>(::SelectObject(memDc, blackPen));
        HBRUSH oldBrush = static_cast<HBRUSH>(::SelectObject(memDc, whiteBrush));

        // The arrow tip, anchored at the hotspot (0,0).
        const POINT arrow[] = { {0,0}, {0,16}, {4,12}, {7,19}, {10,18}, {7,11}, {12,11} };
        ::Polygon(memDc, arrow, ARRAYSIZE(arrow));

        // A small icon for the given shape, offset to the lower-right.
        const int ox = 15, oy = 15, sz = 14;
        switch (shapeType)
        {
        case IMG_SHAPE_RECTANGLE:
            ::Rectangle(memDc, ox, oy, ox + sz, oy + sz);
            break;
        case IMG_SHAPE_CIRCLE:
            ::Ellipse(memDc, ox, oy, ox + sz, oy + sz);
            break;
        case IMG_SHAPE_LINE:
            ::MoveToEx(memDc, ox, oy + sz, nullptr);
            ::LineTo(memDc, ox + sz, oy);
            break;
        case IMG_SHAPE_POLYGON:
        {
            const POINT tri[] = { { ox, oy + sz }, { ox + sz / 2, oy }, { ox + sz, oy + sz } };
            ::Polygon(memDc, tri, ARRAYSIZE(tri));
            break;
        }
        default:
            break;
        }

        ::SelectObject(memDc, oldPen);
        ::SelectObject(memDc, oldBrush);
        ::DeleteObject(blackPen);
        ::DeleteObject(whiteBrush);
        ::SelectObject(memDc, oldBmp);
        ::DeleteDC(memDc);

        // Recolor the sentinel background to fully transparent (alpha=0);
        // everything GDI actually drew (black outline / white fill) becomes
        // fully opaque. A 32bpp color bitmap with real per-pixel alpha is
        // honored directly by CreateIconIndirect on all modern Windows
        // versions, so the AND-mask below can stay all-zero.
        BYTE* px = static_cast<BYTE*>(bits); // B,G,R,A per pixel
        for (int i = 0; i < kSize * kSize; ++i, px += 4)
        {
            const bool isSentinel = px[0] == GetBValue(kSentinel) && px[1] == GetGValue(kSentinel) && px[2] == GetRValue(kSentinel);
            px[3] = isSentinel ? 0 : 255;
        }

        std::vector<BYTE> maskBits((kSize / 8) * kSize, 0); // all-zero AND mask, 1bpp
        HBITMAP maskBmp = ::CreateBitmap(kSize, kSize, 1, 1, maskBits.data());

        ICONINFO info = {};
        info.fIcon = FALSE;
        info.xHotspot = 0;
        info.yHotspot = 0;
        info.hbmMask = maskBmp;
        info.hbmColor = colorBmp;
        HCURSOR cursor = static_cast<HCURSOR>(::CreateIconIndirect(&info));

        ::DeleteObject(maskBmp);
        ::DeleteObject(colorBmp);
        return cursor;
    }

    // Lazily-created, process-lifetime cache (4 tiny GDI cursors -- never
    // explicitly freed, same lifetime policy as the registered window class
    // in Create() below).
    HCURSOR GetShapeCursor(int shapeType)
    {
        static HCURSOR cursors[5] = {}; // indexed by IMG_SHAPE_RECTANGLE..POLYGON (1..4); [0] unused
        if (shapeType < 1 || shapeType > 4) return nullptr;
        if (!cursors[shapeType]) cursors[shapeType] = CreateShapeCursor(shapeType);
        return cursors[shapeType];
    }
}

BEGIN_MESSAGE_MAP(CD3DImageViewerCtrl, CWnd)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_MOUSEWHEEL()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_RBUTTONDOWN()
    ON_WM_KEYDOWN()
    ON_WM_SETCURSOR()
    ON_WM_TIMER()
    ON_MESSAGE(WM_D3DVIEWER_INTERNAL_LOADDONE, &CD3DImageViewerCtrl::OnLoadThreadDone)
END_MESSAGE_MAP()

CD3DImageViewerCtrl::~CD3DImageViewerCtrl()
{
    // Normally already joined by OnDestroy() (which runs first and needs
    // the load finished before it calls ImgEngine_Destroy below) -- this is
    // just a defensive fallback for that same reason, in case this object is
    // ever destroyed without its window having gone through WM_DESTROY.
    if (m_loadThread.joinable()) m_loadThread.join();

    ImgEngine_Destroy(m_engine);   // safe no-op if already destroyed in OnDestroy
    m_engine = IMG_INVALID_HANDLE;
    ImgParamMap_Destroy(m_inMap);
    ImgParamMap_Destroy(m_outMap);
    m_inMap = m_outMap = nullptr;
    if (m_comInit) { CoUninitialize(); m_comInit = false; }
}

BOOL CD3DImageViewerCtrl::Create(const RECT& rect, CWnd* pParent, UINT nID)
{
    // Register a window class once: redraw on resize, no background brush
    // (NULL brush -> no GDI erase -> no flicker; D3D owns every pixel).
    static CString s_class;
    if (s_class.IsEmpty())
    {
        s_class = AfxRegisterWndClass(
            CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
            ::LoadCursor(nullptr, IDC_ARROW),
            reinterpret_cast<HBRUSH>(::GetStockObject(NULL_BRUSH)),
            nullptr);
    }

    return CreateEx(0, s_class, _T(""),
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        rect, pParent, nID);
}

int CD3DImageViewerCtrl::OnCreate(LPCREATESTRUCT lpcs)
{
    if (CWnd::OnCreate(lpcs) == -1) return -1;

    // WIC needs COM on this thread. CoInitializeEx is reference-counted; if the
    // app already called AfxOleInit() this simply succeeds again (S_FALSE).
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) m_comInit = true;     // RPC_E_CHANGED_MODE -> leave as-is

    m_engine = ImgEngine_Create(nullptr);
    m_inMap  = ImgParamMap_Create();
    m_outMap = ImgParamMap_Create();

    EnsureInitialized();
    return 0;   // keep the window even if D3D failed (we draw an error message)
}

void CD3DImageViewerCtrl::OnDestroy()
{
    // A background LoadImageFileAsync thread (if one is still running) calls
    // into m_engine via the DLL's stable ABI; ImgEngine_Destroy tears down
    // that same engine's worker thread, so it MUST NOT run concurrently with
    // it. Block here and let the load finish first -- this is the one place
    // closing the app while a load is in flight can pay for that with a
    // short wait, in exchange for never touching a half-destroyed engine.
    if (m_loadThread.joinable()) m_loadThread.join();

    // Release the device (and the engine instance entirely) while the HWND
    // it's bound to is still valid; ~CD3DImageViewerCtrl() runs later and
    // re-destroying an already-invalid handle is a safe no-op.
    ImgEngine_Destroy(m_engine);
    m_engine = IMG_INVALID_HANDLE;
    m_ready = false;
    CWnd::OnDestroy();
}

void CD3DImageViewerCtrl::RefreshStateFromEngine()
{
    if (!m_engine) { m_ready = false; return; }

    ImgParamMap_Clear(m_inMap);
    ImgParamMap_Clear(m_outMap);
    const ImgStatus status = ImgEngine_Query(m_engine, m_inMap, m_outMap);
    m_ready = (status == IMG_OK);
    if (!m_ready) return;

    int64_t idx = -1, hasImage = 0;
    ImgParamMap_TryGetInt(m_outMap, IMG_KEY_CURRENT_ADAPTER_INDEX, &idx);
    ImgParamMap_TryGetInt(m_outMap, IMG_KEY_HAS_IMAGE, &hasImage);
    m_currentAdapterIndex = static_cast<int>(idx);
    m_hasImage = hasImage != 0;
}

bool CD3DImageViewerCtrl::EnsureInitialized()
{
    if (m_ready) return true;
    if (GetSafeHwnd() == nullptr || !m_engine) return false;

    CRect rc;
    GetClientRect(&rc);
    const UINT w = rc.Width()  > 0 ? static_cast<UINT>(rc.Width())  : 1;
    const UINT h = rc.Height() > 0 ? static_cast<UINT>(rc.Height()) : 1;

    ImgParamMap_Clear(m_inMap);
    ImgParamMap_SetPtr(m_inMap, IMG_KEY_HWND, GetSafeHwnd());
    ImgParamMap_SetInt(m_inMap, IMG_KEY_WIDTH, w);
    ImgParamMap_SetInt(m_inMap, IMG_KEY_HEIGHT, h);
    ImgParamMap_Clear(m_outMap);

    if (ImgEngine_Initialize(m_engine, m_inMap, m_outMap) != IMG_OK)
    {
        m_lastError = ReadStringOr(m_outMap, IMG_KEY_ERROR_MESSAGE,
            _T("Failed to initialize Direct3D 11 (no HARDWARE or WARP device)."));
        return false;
    }

    RefreshStateFromEngine();
    return m_ready;
}

bool CD3DImageViewerCtrl::LoadImageFile(LPCTSTR path)
{
    if (m_loading) return false;
    if (!EnsureInitialized()) return false;

    CStringW wpath(path);                       // works in both ANSI and Unicode builds
    ImgParamMap_Clear(m_inMap);
    ImgParamMap_SetString(m_inMap, IMG_KEY_PATH, wpath);
    ImgParamMap_Clear(m_outMap);

    const bool ok = ImgEngine_LoadImage(m_engine, m_inMap, m_outMap) == IMG_OK;
    if (ok)
    {
        m_hasImage = true;
        m_currentShapeTool = IMG_SHAPE_NONE; // the engine clears shapes/tool on a successful load too
        Invalidate(FALSE);
    }
    else
        m_lastError = ReadStringOr(m_outMap, IMG_KEY_ERROR_MESSAGE,
            _T("Could not load image (unsupported format, decode error, ")
            _T("or insufficient memory/VRAM)."));
    return ok;
}

void CD3DImageViewerCtrl::LoadImageFileAsync(LPCTSTR path)
{
    if (m_loading) return; // ignore re-entrant calls while one is already running
    if (m_loadThread.joinable()) m_loadThread.join(); // release a previous (already-finished) thread handle

    if (!EnsureInitialized())
    {
        // Device creation itself failed -- nothing to run in the background.
        // Notify the parent the same way a real async load would, so its
        // enable/disable-controls logic doesn't need a separate case for this.
        Invalidate(FALSE);
        if (CWnd* parent = GetParent())
            parent->SendMessage(WM_D3DVIEWER_LOADCOMPLETE, static_cast<WPARAM>(GetDlgCtrlID()), FALSE);
        return;
    }

    m_loading = true;
    m_spinnerAngle = 0.0f;
    m_loadStartTickMs = ::GetTickCount64();
    SetTimer(kSpinnerTimerId, 40, nullptr);
    Invalidate(FALSE);

    // The thread body touches nothing belonging to `this` except the plain
    // HWND/engine-handle values captured below (both trivially copyable, no
    // MFC object lifetime involved) -- m_inMap/m_outMap stay UI-thread-only,
    // so this uses its own ImgParamMaps instead.
    const std::wstring pathCopy(path);
    HWND hwndSelf = GetSafeHwnd();
    ImgEngineHandle engine = m_engine;

    m_loadThread = std::thread([hwndSelf, engine, pathCopy]()
    {
        ImgParamMap* in = ImgParamMap_Create();
        ImgParamMap* out = ImgParamMap_Create();
        ImgParamMap_SetString(in, IMG_KEY_PATH, pathCopy.c_str());
        const bool ok = ImgEngine_LoadImage(engine, in, out) == IMG_OK;
        if (ok)
        {
            // Force the on-demand GPU tile upload a large image needs (see
            // RenderTiles in the DLL) to happen here too, off the UI thread,
            // so the next OnPaint back on it is fast instead of doing it itself.
            ImgEngine_Render(engine);
        }
        ImgParamMap_Destroy(in);
        ImgParamMap_Destroy(out);

        ::PostMessage(hwndSelf, WM_D3DVIEWER_INTERNAL_LOADDONE, 0, ok ? TRUE : FALSE);
    });
}

LRESULT CD3DImageViewerCtrl::OnLoadThreadDone(WPARAM /*wParam*/, LPARAM lParam)
{
    KillTimer(kSpinnerTimerId);
    m_loading = false;
    const bool ok = lParam != 0;
    if (ok)
    {
        m_hasImage = true;
        m_currentShapeTool = IMG_SHAPE_NONE; // the engine clears shapes/tool on a successful load too
    }
    else
        m_lastError = _T("Could not load image (unsupported format, decode error, ")
                      _T("or insufficient memory/VRAM).");
    Invalidate(FALSE);

    if (CWnd* parent = GetParent())
        parent->SendMessage(WM_D3DVIEWER_LOADCOMPLETE, static_cast<WPARAM>(GetDlgCtrlID()), ok ? TRUE : FALSE);
    return 0;
}

bool CD3DImageViewerCtrl::SwitchAdapter(const D3DAdapterInfo& adapter, int adapterListIndex)
{
    if (GetSafeHwnd() == nullptr || !m_engine || m_loading) return false;

    ImgParamMap_Clear(m_inMap);
    ImgParamMap_SetInt(m_inMap, IMG_KEY_ADAPTER_IS_WARP, adapter.isWarp ? 1 : 0);
    ImgParamMap_SetInt(m_inMap, IMG_KEY_ADAPTER_INDEX, static_cast<int64_t>(adapter.adapterIndex));
    ImgParamMap_SetInt(m_inMap, IMG_KEY_ADAPTER_LIST_INDEX, adapterListIndex);
    ImgParamMap_Clear(m_outMap);

    const ImgStatus status = ImgEngine_SwitchAdapter(m_engine, m_inMap, m_outMap);
    if (status != IMG_OK)
        m_lastError = ReadStringOr(m_outMap, IMG_KEY_ERROR_MESSAGE,
            _T("Failed to initialize Direct3D 11 on the selected hardware."));

    // Picks up m_ready / m_currentAdapterIndex / m_hasImage as they actually
    // ended up -- including the engine's best-effort rollback outcome.
    RefreshStateFromEngine();

    Invalidate(FALSE);
    NotifyViewChanged();
    return status == IMG_OK;
}

void CD3DImageViewerCtrl::FitToWindow()
{
    if (!m_ready || m_loading) return;
    ImgParamMap_Clear(m_inMap);
    ImgParamMap_Clear(m_outMap);
    ImgEngine_ResetView(m_engine, m_inMap, m_outMap);
    Invalidate(FALSE);
    NotifyViewChanged();
}

void CD3DImageViewerCtrl::OnPaint()
{
    {
        CPaintDC dc(this);   // BeginPaint/EndPaint validates the update region
        if (!m_ready && !m_loading) EnsureInitialized();

        if (m_loading)
        {
            // Don't touch the engine while a background load owns its
            // worker-thread queue -- calling Render() here would block this
            // (UI) thread waiting for that same queue, defeating the whole
            // point of loading in the background. Leave whatever's already
            // on screen as-is; the spinner overlay below covers it.
        }
        else if (m_ready)
            ImgEngine_Render(m_engine);   // D3D Present draws directly to this HWND
        else
            PaintError(dc);
    } // EndPaint (dc's dtor) runs here, same as the dialog's green-border technique requires

    if (m_loading)
        DrawSpinnerOverlay();   // fresh CClientDC, like DrawShapeLabels below
    else if (m_ready && m_showLabels)
        DrawShapeLabels();   // GDI text on top, via a fresh CClientDC -- see its own comment
}

void CD3DImageViewerCtrl::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == kSpinnerTimerId)
    {
        m_spinnerAngle += 18.0f;
        if (m_spinnerAngle >= 360.0f) m_spinnerAngle -= 360.0f;
        InvalidateRect(nullptr, FALSE);
        return;
    }
    CWnd::OnTimer(nIDEvent);
}

void CD3DImageViewerCtrl::DrawSpinnerOverlay()
{
    CClientDC dc(this);   // NOT a CPaintDC -- same post-paint-cycle technique as DrawShapeLabels
    CRect rc;
    GetClientRect(&rc);
    const int cx = rc.Width() / 2;
    const int cy = rc.Height() / 2;
    const int r = kSpinnerRadius;

    // OnEraseBkgnd() suppresses the normal background erase (needed so the
    // D3D-rendered image doesn't flicker), which means nothing else clears
    // this region between timer ticks -- without explicitly erasing it
    // ourselves first, each new frame's arc/text draws directly on top of
    // the previous one instead of replacing it (this is what caused the
    // elapsed-time digits to look like overlapping/smeared multiple writes).
    const int fillHalf = r + kSpinnerPenWidth;
    CRect fillRect(cx - fillHalf, cy - fillHalf, cx + fillHalf, cy + fillHalf);
    dc.FillSolidRect(&fillRect, RGB(30, 30, 34));

    CBrush* oldBrush = static_cast<CBrush*>(dc.SelectStockObject(NULL_BRUSH));

    CPen trackPen(PS_SOLID, kSpinnerPenWidth, RGB(90, 90, 90));
    CPen* oldPen = dc.SelectObject(&trackPen);
    dc.Ellipse(cx - r, cy - r, cx + r, cy + r);

    // A ~100-degree arc that rotates every timer tick -- a classic
    // indeterminate "still working" spinner, drawn with plain GDI (no extra
    // dependency), consistent with how this app already builds cursors and
    // shape labels at runtime instead of using resource files.
    CPen arcPen(PS_SOLID, kSpinnerPenWidth, RGB(40, 200, 40));
    dc.SelectObject(&arcPen);
    constexpr double kPi = 3.14159265358979323846;
    const double startRad = m_spinnerAngle * kPi / 180.0;
    const double endRad   = startRad + (100.0 * kPi / 180.0);
    const int x1 = cx + static_cast<int>(r * std::cos(startRad));
    const int y1 = cy - static_cast<int>(r * std::sin(startRad));
    const int x2 = cx + static_cast<int>(r * std::cos(endRad));
    const int y2 = cy - static_cast<int>(r * std::sin(endRad));
    dc.Arc(cx - r, cy - r, cx + r, cy + r, x1, y1, x2, y2);

    dc.SelectObject(oldPen);
    dc.SelectObject(oldBrush);

    // Elapsed time since this load started, centered inside the ring -- uses
    // the same CDC::TextOutW + dark-shadow-then-colored-text technique as
    // DrawShapeLabels (proven to render correctly over arbitrary content in
    // this exact control), rather than DrawText, so centering is computed
    // explicitly via GetTextExtent instead of relying on a bounding CRect.
    CFont font;
    font.CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));
    CFont* oldFont = dc.SelectObject(&font);
    const int oldBkMode = dc.SetBkMode(TRANSPARENT);

    const ULONGLONG elapsedMs = ::GetTickCount64() - m_loadStartTickMs;
    CString text;
    text.Format(_T("%llu ms"), elapsedMs);
    const CSize textSize = dc.GetTextExtent(text);
    const int tx = cx - textSize.cx / 2;
    const int ty = cy - textSize.cy / 2;

    dc.SetTextColor(RGB(0, 0, 0));
    dc.TextOutW(tx + 1, ty + 1, text);
    dc.SetTextColor(RGB(255, 40, 40));
    dc.TextOutW(tx, ty, text);

    dc.SetBkMode(oldBkMode);
    dc.SelectObject(oldFont);
}

void CD3DImageViewerCtrl::DrawShapeLabels()
{
    CClientDC dc(this);   // NOT the CPaintDC above -- see OnPaint's comment
    ImgParamMap_Clear(m_inMap);
    ImgParamMap_Clear(m_outMap);
    if (ImgEngine_Shape_GetLabels(m_engine, m_inMap, m_outMap) != IMG_OK) return;

    int64_t count = 0;
    ImgParamMap_TryGetInt(m_outMap, IMG_KEY_LABEL_COUNT, &count);
    if (count <= 0) return;

    const int oldBkMode = dc.SetBkMode(TRANSPARENT);
    for (int64_t i = 0; i < count; ++i)
    {
        int64_t x = 0, y = 0;
        ImgParamMap_TryGetInt(m_outMap, AdapterKey(i, IMG_KEY_LABEL_X_FMT), &x);
        ImgParamMap_TryGetInt(m_outMap, AdapterKey(i, IMG_KEY_LABEL_Y_FMT), &y);
        const CString text = ReadStringOr(m_outMap, AdapterKey(i, IMG_KEY_LABEL_TEXT_FMT), _T(""));
        if (text.IsEmpty()) continue;

        // A 1px dark "shadow" behind light text keeps it legible over the
        // arbitrary, often busy, image content underneath.
        dc.SetTextColor(RGB(0, 0, 0));
        dc.TextOutW(static_cast<int>(x) + 1, static_cast<int>(y) + 1, text);
        dc.SetTextColor(RGB(255, 255, 120));
        dc.TextOutW(static_cast<int>(x), static_cast<int>(y), text);
    }
    dc.SetBkMode(oldBkMode);
}

void CD3DImageViewerCtrl::PaintError(CDC& dc)
{
    CRect rc;
    GetClientRect(&rc);
    dc.FillSolidRect(&rc, RGB(30, 30, 34));
    dc.SetTextColor(RGB(220, 90, 90));
    dc.SetBkMode(TRANSPARENT);
    CString msg = L"Direct3D not initialized";
    if (!m_lastError.IsEmpty()) {
		msg = m_lastError;
    }
    dc.DrawText(msg, &rc, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
}

BOOL CD3DImageViewerCtrl::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;   // suppress GDI erase (prevents flicker)
}

void CD3DImageViewerCtrl::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    if (m_ready && !m_loading && cx > 0 && cy > 0)
    {
        ImgParamMap_Clear(m_inMap);
        ImgParamMap_SetInt(m_inMap, IMG_KEY_WIDTH, cx);
        ImgParamMap_SetInt(m_inMap, IMG_KEY_HEIGHT, cy);
        ImgParamMap_Clear(m_outMap);
        ImgEngine_Resize(m_engine, m_inMap, m_outMap);
        Invalidate(FALSE);
    }
}

BOOL CD3DImageViewerCtrl::OnMouseWheel(UINT /*nFlags*/, short zDelta, CPoint pt)
{
    if (m_ready && m_hasImage && !m_loading)
    {
        CPoint cpt = pt;
        ScreenToClient(&cpt);                       // wheel pt is in screen coords
        const float factor = (zDelta > 0) ? 1.10f : (1.0f / 1.10f);

        ImgParamMap_Clear(m_inMap);
        ImgParamMap_SetFloat(m_inMap, IMG_KEY_FACTOR, factor);
        ImgParamMap_SetInt(m_inMap, IMG_KEY_X, cpt.x);
        ImgParamMap_SetInt(m_inMap, IMG_KEY_Y, cpt.y);
        ImgParamMap_Clear(m_outMap);
        ImgEngine_ZoomAt(m_engine, m_inMap, m_outMap);

        Invalidate(FALSE);
        NotifyViewChanged();
    }
    return TRUE;
}

void CD3DImageViewerCtrl::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
    SetFocus();                 // so subsequent wheel/keyboard messages come to us

    bool handled = false;
    if (m_ready && m_hasImage && !m_loading)
    {
        ImgParamMap_Clear(m_inMap);
        ImgParamMap_SetInt(m_inMap, IMG_KEY_X, point.x);
        ImgParamMap_SetInt(m_inMap, IMG_KEY_Y, point.y);
        ImgParamMap_Clear(m_outMap);
        ImgEngine_Shape_OnMouseDown(m_engine, m_inMap, m_outMap);

        int64_t handledInt = 0, tool = m_currentShapeTool;
        ImgParamMap_TryGetInt(m_outMap, IMG_KEY_HANDLED, &handledInt);
        ImgParamMap_TryGetInt(m_outMap, IMG_KEY_CURRENT_TOOL, &tool);
        handled = handledInt != 0;
        m_currentShapeTool = static_cast<int>(tool);

        Invalidate(FALSE);
        NotifyViewChanged();
    }

    // A click the shape system didn't consume (no tool armed, nothing under
    // the cursor) falls through to the original image-pan behavior.
    if (!handled)
    {
        m_dragging  = true;
        m_lastMouse = point;
        SetCapture();
    }
}

void CD3DImageViewerCtrl::OnLButtonUp(UINT /*nFlags*/, CPoint point)
{
    if (m_dragging)
    {
        m_dragging = false;
        ReleaseCapture();
    }

    if (m_ready && m_hasImage && !m_loading)
    {
        ImgParamMap_Clear(m_inMap);
        ImgParamMap_SetInt(m_inMap, IMG_KEY_X, point.x);
        ImgParamMap_SetInt(m_inMap, IMG_KEY_Y, point.y);
        ImgParamMap_Clear(m_outMap);
        ImgEngine_Shape_OnMouseUp(m_engine, m_inMap, m_outMap);

        int64_t changed = 0, tool = m_currentShapeTool;
        ImgParamMap_TryGetInt(m_outMap, IMG_KEY_CHANGED, &changed);
        ImgParamMap_TryGetInt(m_outMap, IMG_KEY_CURRENT_TOOL, &tool);
        m_currentShapeTool = static_cast<int>(tool);

        if (changed)
        {
            Invalidate(FALSE);
            NotifyViewChanged();
        }
    }
}

void CD3DImageViewerCtrl::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
    // Safe to call on every move regardless of drag state -- this drives the
    // live rubber-band preview while hovering between line/polygon clicks,
    // and live resize while a handle is being dragged.
    if (m_ready && m_hasImage && !m_loading)
    {
        ImgParamMap_Clear(m_inMap);
        ImgParamMap_SetInt(m_inMap, IMG_KEY_X, point.x);
        ImgParamMap_SetInt(m_inMap, IMG_KEY_Y, point.y);
        ImgParamMap_Clear(m_outMap);
        ImgEngine_Shape_OnMouseMove(m_engine, m_inMap, m_outMap);

        int64_t changed = 0;
        ImgParamMap_TryGetInt(m_outMap, IMG_KEY_CHANGED, &changed);
        if (changed) Invalidate(FALSE);

        double hx = 0.0, hy = 0.0;
        ImgParamMap_TryGetFloat(m_outMap, IMG_KEY_HOVER_X, &hx);
        ImgParamMap_TryGetFloat(m_outMap, IMG_KEY_HOVER_Y, &hy);
        m_hoverText.Format(_T("X: %.0f   Y: %.0f"), hx, hy);
        NotifyHoverChanged();

        int64_t cursorShapeType = IMG_SHAPE_NONE, cursorIsMove = 0;
        ImgParamMap_TryGetInt(m_outMap, IMG_KEY_CURSOR_SHAPE_TYPE, &cursorShapeType);
        ImgParamMap_TryGetInt(m_outMap, IMG_KEY_CURSOR_IS_MOVE, &cursorIsMove);
        m_cursorShapeType = static_cast<int>(cursorShapeType);
        m_cursorIsGenericMove = cursorIsMove != 0;
    }
    else
    {
        m_cursorShapeType = IMG_SHAPE_NONE;
        m_cursorIsGenericMove = false;
    }

    if (m_dragging && m_ready && m_hasImage && !m_loading)
    {
        ImgParamMap_Clear(m_inMap);
        ImgParamMap_SetFloat(m_inMap, IMG_KEY_DX, static_cast<float>(point.x - m_lastMouse.x));
        ImgParamMap_SetFloat(m_inMap, IMG_KEY_DY, static_cast<float>(point.y - m_lastMouse.y));
        ImgParamMap_Clear(m_outMap);
        ImgEngine_Pan(m_engine, m_inMap, m_outMap);

        m_lastMouse = point;
        Invalidate(FALSE);
    }
}

void CD3DImageViewerCtrl::OnRButtonDown(UINT /*nFlags*/, CPoint /*point*/)
{
    FitToWindow();              // right-click resets the view
}

BOOL CD3DImageViewerCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    // m_cursorShapeType/m_cursorIsGenericMove are refreshed on every
    // OnMouseMove from the engine's Shape_OnMouseMove report -- just apply
    // whatever they currently say here; WM_SETCURSOR fires continuously as
    // the mouse moves, so this needs no DLL round-trip of its own.
    if (m_cursorIsGenericMove)
    {
        // The selected shape is filled, and the hover/drag is over its body
        // (not a resize handle) -- its whole interior is draggable now, so
        // show the universally-recognized "move" cursor instead of the
        // shape-specific icon.
        ::SetCursor(::LoadCursor(nullptr, IDC_SIZEALL));
        return TRUE;
    }
    if (HCURSOR cursor = GetShapeCursor(m_cursorShapeType))
    {
        ::SetCursor(cursor);
        return TRUE;
    }
    return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

void CD3DImageViewerCtrl::OnKeyDown(UINT nChar, UINT /*nRepCnt*/, UINT /*nFlags*/)
{
    if (m_ready && !m_loading)
    {
        ImgParamMap_Clear(m_inMap);
        ImgParamMap_SetInt(m_inMap, IMG_KEY_VK_CODE, static_cast<int64_t>(nChar));
        ImgParamMap_Clear(m_outMap);
        ImgEngine_Shape_OnKeyDown(m_engine, m_inMap, m_outMap);

        int64_t changed = 0;
        ImgParamMap_TryGetInt(m_outMap, IMG_KEY_CHANGED, &changed);
        if (changed)
        {
            Invalidate(FALSE);
            NotifyViewChanged();
        }
    }
}

void CD3DImageViewerCtrl::SetShapeTool(int shapeType)
{
    if (!m_ready || m_loading) return;

    ImgParamMap_Clear(m_inMap);
    ImgParamMap_SetInt(m_inMap, IMG_KEY_SHAPE_TYPE, shapeType);
    ImgParamMap_Clear(m_outMap);
    ImgEngine_Shape_SetTool(m_engine, m_inMap, m_outMap);

    m_currentShapeTool = shapeType;
    Invalidate(FALSE);
}

void CD3DImageViewerCtrl::SetCoordinateOrigin(int origin)
{
    if (!m_ready || m_loading) return;

    ImgParamMap_Clear(m_inMap);
    ImgParamMap_SetInt(m_inMap, IMG_KEY_COORD_ORIGIN, origin);
    ImgParamMap_Clear(m_outMap);
    ImgEngine_Shape_SetCoordinateOrigin(m_engine, m_inMap, m_outMap);

    m_coordOrigin = origin;
    Invalidate(FALSE); // label text and the hover readout both depend on this
}

void CD3DImageViewerCtrl::SetShapeFilled(bool filled)
{
    if (!m_ready || m_loading) return;

    ImgParamMap_Clear(m_inMap);
    ImgParamMap_SetInt(m_inMap, IMG_KEY_SHAPE_FILLED, filled ? 1 : 0);
    ImgParamMap_Clear(m_outMap);
    ImgEngine_Shape_SetFilled(m_engine, m_inMap, m_outMap);

    m_shapeFilled = filled;
    Invalidate(FALSE);
}

void CD3DImageViewerCtrl::SetShowLabels(bool show)
{
    m_showLabels = show;
    Invalidate(FALSE);
}

void CD3DImageViewerCtrl::NotifyViewChanged()
{
    if (CWnd* pParent = GetParent())
        pParent->SendMessage(WM_D3DVIEWER_VIEWCHANGED, static_cast<WPARAM>(GetDlgCtrlID()), 0);
}

void CD3DImageViewerCtrl::NotifyHoverChanged()
{
    if (CWnd* pParent = GetParent())
        pParent->SendMessage(WM_D3DVIEWER_HOVERCHANGED, static_cast<WPARAM>(GetDlgCtrlID()), 0);
}

CString CD3DImageViewerCtrl::GetPerfText()
{
    m_perf.Sample();
    CString s;
    s.Format(_T("CPU %.1f%%  |  GPU %.1f%%"), m_perf.GetCpuPercent(), m_perf.GetGpuPercent());
    return s;
}

CString CD3DImageViewerCtrl::GetStatusText() const
{
    if (!m_ready || !m_engine) return _T("Direct3D not ready");

    ImgParamMap_Clear(m_inMap);
    ImgParamMap_Clear(m_outMap);
    if (ImgEngine_Query(m_engine, m_inMap, m_outMap) != IMG_OK)
        return _T("Direct3D not ready");

    int64_t hasImage = 0;
    ImgParamMap_TryGetInt(m_outMap, IMG_KEY_HAS_IMAGE, &hasImage);
    if (!hasImage) return _T("No image loaded");

    int64_t width = 0, height = 0, fileSizeBytes = 0;
    double zoom = 0.0, loadMs = 0.0;
    ImgParamMap_TryGetInt(m_outMap, IMG_KEY_IMAGE_WIDTH, &width);
    ImgParamMap_TryGetInt(m_outMap, IMG_KEY_IMAGE_HEIGHT, &height);
    ImgParamMap_TryGetInt(m_outMap, IMG_KEY_LAST_FILE_SIZE_BYTES, &fileSizeBytes);
    ImgParamMap_TryGetFloat(m_outMap, IMG_KEY_ZOOM, &zoom);
    ImgParamMap_TryGetFloat(m_outMap, IMG_KEY_LAST_LOAD_MS, &loadMs);

    const UINT64 textureBytes = static_cast<UINT64>(width) * static_cast<UINT64>(height) * 4;
    CString s;
    s.Format(_T("%u x %u px  |  File %s  |  Texture %s  |  Load %.0f ms  |  Zoom %.0f%%"),
        static_cast<UINT>(width), static_cast<UINT>(height),
        static_cast<LPCTSTR>(FormatBytes(static_cast<UINT64>(fileSizeBytes))),
        static_cast<LPCTSTR>(FormatBytes(textureBytes)),
        loadMs, zoom * 100.0);
    return s;
}

std::vector<D3DAdapterInfo> CD3DImageViewerCtrl::EnumerateAdapters()
{
    std::vector<D3DAdapterInfo> result;

    ImgParamMap* out = ImgParamMap_Create();
    if (!out) return result;

    if (ImgEngine_EnumerateAdapters(nullptr, out) == IMG_OK)
    {
        int64_t count = 0;
        ImgParamMap_TryGetInt(out, IMG_KEY_ADAPTER_COUNT, &count);
        result.reserve(static_cast<size_t>(count > 0 ? count : 0));

        for (int64_t i = 0; i < count; ++i)
        {
            D3DAdapterInfo info;

            int64_t isWarp = 0;
            ImgParamMap_TryGetInt(out, AdapterKey(i, IMG_KEY_ADAPTER_IS_WARP_FMT), &isWarp);
            info.isWarp = isWarp != 0;

            int64_t idx = 0;
            ImgParamMap_TryGetInt(out, AdapterKey(i, IMG_KEY_ADAPTER_INDEX_FMT), &idx);
            info.adapterIndex = static_cast<UINT>(idx);

            const CStringA nameKey = AdapterKey(i, IMG_KEY_ADAPTER_NAME_FMT);
            const int32_t needed = ImgParamMap_TryGetString(out, nameKey, nullptr, 0);
            if (needed > 0)
            {
                std::vector<wchar_t> buf(static_cast<size_t>(needed));
                ImgParamMap_TryGetString(out, nameKey, buf.data(), needed);
                info.description = buf.data();
            }

            result.push_back(std::move(info));
        }
    }

    ImgParamMap_Destroy(out);
    return result;
}
