// D3DRenderer.h
// ---------------------------------------------------------------------------
// Phase 1 Direct3D 11 renderer for the MFC image viewer.
//
// Responsibilities:
//   - Create a D3D11 device + swap chain bound to an HWND.
//   - Auto-select HARDWARE (GPU) driver, fall back to WARP (CPU software).
//   - Decode an image file via WIC and upload it to a GPU texture.
//   - Render the image as a single textured quad with pan / zoom.
//
// This file has NO MFC dependency on purpose. It is pure Win32 + D3D11 + WIC
// so it can be reused outside MFC (Qt, raw Win32, a test harness, etc.).
//
// NOTE: images up to the FL11 single-texture limit (16384 x 16384) load as
//   ONE D3D texture (the original Phase 1 path, unchanged). Larger images
//   use the tiled path added in Phase 2: the full image is decoded once into
//   a host RAM buffer, then sliced into on-demand GPU tiles with LRU
//   eviction -- see m_isTiled / BuildTileGrid / RenderTiles in the .cpp.
// ---------------------------------------------------------------------------
#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <string>
#include <vector>

#include "ShapeManager.h"   // PointF, ShapeRenderEntry -- shape overlay data, no D3D dependency itself

using Microsoft::WRL::ComPtr;

// Describes one selectable rendering backend for a hardware-picker UI: either
// a specific GPU (DXGI adapter) or the WARP software rasterizer.
struct D3DAdapterInfo
{
    std::wstring description;       // friendly name to show the user
    bool         isWarp = false;    // true => WARP (CPU) instead of a GPU
    UINT         adapterIndex = 0;  // DXGI adapter index; unused when isWarp
};

class D3DRenderer
{
public:
    // Specific reason a LoadImageFrom* call failed, so callers (the engine
    // DLL shell) can report something more useful than a bare bool. Ok = 0.
    enum class D3DLoadResult
    {
        Ok = 0,
        NoDevice,         // Initialize() hasn't succeeded yet
        InvalidArgument,  // null path, or null/zero-size buffer
        DecodeFailed,     // WIC couldn't parse the data (corrupt/unsupported/bad pointer)
        ImageTooLarge,    // exceeds the FL11 single-texture limit (16384 px)
        OutOfMemory,      // host allocation failure while staging decoded pixels
        GpuUploadFailed,  // CreateTexture2D/CreateShaderResourceView failed (VRAM?)
    };

    D3DRenderer() = default;
    ~D3DRenderer();

    // Enumerate the GPUs Direct3D can see plus the WARP (CPU) fallback, for a
    // hardware-selection UI. Does not require Initialize() to have run.
    static std::vector<D3DAdapterInfo> EnumerateAdapters();

    // Create device, swap chain and the fixed pipeline objects on the default
    // adapter (tries HARDWARE first, falls back to WARP).
    bool Initialize(HWND hWnd, UINT width, UINT height);

    // Same as Initialize(), but on a specific adapter from EnumerateAdapters()
    // rather than the auto-selected default. Used to let the user switch
    // rendering hardware at runtime.
    bool InitializeWithAdapter(HWND hWnd, UINT width, UINT height, const D3DAdapterInfo& adapter);

    // Release everything (safe to call multiple times).
    void Shutdown();

    // Resize swap-chain buffers. Call from WM_SIZE.
    bool Resize(UINT width, UINT height);

    // Decode an image file (WIC) and upload to a GPU texture.
    D3DLoadResult LoadImageFromFile(const wchar_t* path);

    // Same, but decoding from an in-memory buffer (e.g. bytes already read
    // into RAM, or received over the network) instead of a file on disk.
    // `data` only needs to remain valid for the duration of this call; it is
    // not retained. The decode step is isolated behind structured exception
    // handling, so a bad/dangling `data` pointer reports DecodeFailed instead
    // of crashing the process -- this is the one entry point in this class
    // that touches caller-supplied raw memory it didn't allocate itself.
    D3DLoadResult LoadImageFromMemory(const void* data, size_t dataSize);

    // Render one frame: clear, draw image quad if present, present.
    void Render();

    // ---- View control (zoom is the image-px -> screen-px ratio) ----
    void  ZoomAt(float factor, int screenX, int screenY); // zoom around a point
    void  Pan(float dxPixels, float dyPixels);
    void  ResetView();                                    // fit-to-window
    float GetZoom() const { return m_zoom; }

    // ---- Queries ----
    bool          HasImage()          const { return m_imageSRV != nullptr || m_isTiled; }
    UINT          GetImageWidth()     const { return m_imageWidth; }
    UINT          GetImageHeight()    const { return m_imageHeight; }
    bool          IsHardwareDevice()  const { return m_isHardware; }
    const wchar_t* GetDeviceDescription() const { return m_deviceDesc; }

    // ---- Tiled-image diagnostics (Phase 2) ----
    bool   IsTiled()              const { return m_isTiled; }
    UINT   GetTileCount()         const { return static_cast<UINT>(m_tiles.size()); }
    UINT64 GetResidentTileBytes() const { return m_residentTileBytes; }

    // Wall-clock time (decode + GPU upload) taken by the most recent
    // LoadImageFromFile() call, in milliseconds.
    float GetLastLoadTimeMs() const { return m_lastLoadMs; }

    // Size, in bytes, of the most recently loaded source: the file size on
    // disk for LoadImageFromFile, or dataSize as passed to LoadImageFromMemory.
    UINT64 GetLastFileSizeBytes() const { return m_lastFileSizeBytes; }

    // ---- Shape-annotation overlay ----
    // Converts between this control's CLIENT/SCREEN pixel space and
    // IMAGE-PIXEL space, using the exact same zoom/pan/viewport transform
    // Render() uses for the image quad (so shapes stay anchored to image
    // content across zoom/pan). Meaningless if !HasImage().
    PointF ScreenToImage(int screenX, int screenY) const;
    PointF ImageToScreen(float imageX, float imageY) const;

    // Replaces the current shape overlays drawn on top of the image. Takes
    // effect on the next Render() call; the GPU-side vertex buffers are
    // rebuilt lazily (only if this was called since the last Render()).
    void SetShapeRenderData(std::vector<ShapeRenderEntry> shapes);

private:
    bool InitializeInternal(HWND hWnd, UINT width, UINT height, const D3DAdapterInfo* explicitAdapter);
    bool CreateDeviceAndSwapChain(HWND hWnd, UINT width, UINT height, const D3DAdapterInfo* explicitAdapter);
    bool TryCreate(HWND hWnd, const struct DXGI_SWAP_CHAIN_DESC& scd, UINT flags, const D3DAdapterInfo* explicitAdapter);
    bool CreateRenderTarget();
    void ReleaseRenderTarget();
    bool CreatePipeline();
    void UpdateConstantBuffer();

    // Shared tail of both LoadImageFrom*: decode via an already-created WIC
    // decoder into a packed RGBA pixel buffer (does not touch the GPU).
    D3DLoadResult DecodeToPixels(struct IWICImagingFactory* factory, struct IWICBitmapDecoder* decoder,
                                  UINT& outWidth, UINT& outHeight, std::vector<BYTE>& outPixels);
    // SEH boundary around DecodeToPixels for the memory-sourced path only:
    // this function's own frame deliberately has no destructible C++ locals
    // (everything with RAII lives one frame down, in DecodeToPixels itself),
    // which is required for __try/__except to coexist safely with C++
    // object unwinding. Used because, unlike a file path, a caller-supplied
    // memory pointer can be wrong in ways WIC's own error handling can't
    // catch (e.g. a dangling pointer) -- this turns that into DecodeFailed
    // instead of an access-violation crash.
    D3DLoadResult SehGuardedDecodeToPixels(struct IWICImagingFactory* factory, struct IWICBitmapDecoder* decoder,
                                            UINT& outWidth, UINT& outHeight, std::vector<BYTE>& outPixels);
    // Shared tail: takes an already-decoded RGBA pixel buffer and either (a)
    // uploads it to a single GPU texture (images within the FL11 limit,
    // today's original path) or (b) moves it into m_hostPixels and builds an
    // on-demand GPU tile grid over it (images exceeding the limit -- see
    // BuildTileGrid/RenderTiles). `pixels` is taken by non-const reference so
    // case (b) can move out of it instead of copying a potentially ~1GB buffer.
    D3DLoadResult UploadPixels(UINT width, UINT height, std::vector<BYTE>& pixels);

    // Single source of truth for the image-pixel <-> NDC transform, shared by
    // UpdateConstantBuffer() (drives the image quad's shader constant) and
    // ScreenToImage/ImageToScreen/RebuildShapeVertexBuffers (so coordinate
    // conversion can never drift out of sync with what's actually rendered).
    void ComputeImageTransform(float& sx, float& sy, float& tx, float& ty) const;

    bool CreateShapePipeline();       // flat-color shader + identity constant buffer; called once from CreatePipeline()
    void RebuildShapeVertexBuffers(); // CPU->GPU upload of m_shapes, only when m_shapesDirty
    void RenderShapes();              // issues the shape/handle draw calls; called from Render()

    struct ShapeVertex { float x, y; float r, g, b, a; };
    struct ShapeDrawRange { UINT vertexOffset = 0; UINT vertexCount = 0; };
    void UploadDynamicVertexBuffer(const std::vector<ShapeVertex>& verts, ComPtr<ID3D11Buffer>& buffer, UINT& capacity);

    // ---- Tiled image path (Phase 2) ----
    // One cell of the on-demand tile grid covering the whole decoded image.
    // texture/srv are null when the tile isn't currently uploaded to the GPU.
    struct ImageTile
    {
        UINT pixelX = 0, pixelY = 0;   // top-left, in whole-image pixel space
        UINT pixelW = 0, pixelH = 0;   // actual size (last row/col is clipped)
        ComPtr<ID3D11Texture2D>          texture;
        ComPtr<ID3D11ShaderResourceView> srv;
        UINT64 lastUsedFrame = 0;
    };
    struct TileVertex { float x, y; float u, v; };

    void BuildTileGrid();                              // populates m_tiles from m_imageWidth/Height; called once per tiled load
    bool EnsureTileResident(ImageTile& tile);           // lazy CreateTexture2D+SRV from m_hostPixels
    void GetVisibleTileRange(UINT& outCol0, UINT& outCol1, UINT& outRow0, UINT& outRow1) const;
    void RenderTiles();                                 // replaces the single image Draw(6,0) when m_isTiled; called from Render()
    void EvictTilesIfOverBudget(const std::vector<UINT>& visibleIndices);

    // Core D3D objects
    ComPtr<ID3D11Device>             m_device;
    ComPtr<ID3D11DeviceContext>      m_context;
    ComPtr<IDXGISwapChain>           m_swapChain;
    ComPtr<ID3D11RenderTargetView>   m_rtv;

    // Fixed pipeline
    ComPtr<ID3D11VertexShader>       m_vs;
    ComPtr<ID3D11PixelShader>        m_ps;
    ComPtr<ID3D11InputLayout>        m_inputLayout;
    ComPtr<ID3D11Buffer>             m_vertexBuffer;
    ComPtr<ID3D11Buffer>             m_constantBuffer;
    ComPtr<ID3D11SamplerState>       m_sampler;
    ComPtr<ID3D11RasterizerState>    m_rasterState;

    // Image resources
    ComPtr<ID3D11Texture2D>          m_imageTexture;
    ComPtr<ID3D11ShaderResourceView> m_imageSRV;
    UINT m_imageWidth  = 0;
    UINT m_imageHeight = 0;

    // ---- Tiled image path (Phase 2): used when the decoded image exceeds
    // kMaxTextureDim in either dimension. m_hostPixels holds the full decoded
    // RGBA buffer for the lifetime of the loaded image (re-decoding on every
    // pan/tile-miss would cost far more than keeping this resident); GPU
    // tiles are created/evicted on demand from it inside Render().
    bool                   m_isTiled = false;
    std::vector<BYTE>      m_hostPixels;       // only populated when m_isTiled
    UINT                   m_tileGridCols = 0;
    UINT                   m_tileGridRows = 0;
    std::vector<ImageTile> m_tiles;            // row-major, size = m_tileGridCols * m_tileGridRows
    UINT64                 m_frameCounter = 0; // monotonically increasing, stamped on tiles drawn each frame
    UINT64                 m_residentTileBytes = 0;
    ComPtr<ID3D11Buffer>   m_tileVB;           // shared dynamic VB, rewritten per tile draw (6 verts, like the shape VBs)
    UINT                   m_tileVBCapacity = 0;

    // Viewport / view state
    UINT  m_viewportWidth  = 1;
    UINT  m_viewportHeight = 1;
    float m_zoom = 1.0f;
    float m_panX = 0.0f;   // screen-space pixels, relative to viewport center
    float m_panY = 0.0f;

    bool    m_isHardware = false;
    wchar_t m_deviceDesc[128] = L"";
    float   m_lastLoadMs = 0.0f;
    UINT64  m_lastFileSizeBytes = 0;

    // ---- Shape-annotation overlay pipeline ----
    ComPtr<ID3D11VertexShader>    m_shapeVS;
    ComPtr<ID3D11PixelShader>     m_shapePS;
    ComPtr<ID3D11InputLayout>     m_shapeInputLayout;
    ComPtr<ID3D11Buffer>          m_identityConstantBuffer; // {1,1,0,0}; used for already-final-NDC handle quads

    std::vector<ShapeRenderEntry> m_shapes;
    bool m_shapesDirty = false;

    ComPtr<ID3D11Buffer> m_shapeOutlineVB;
    UINT                 m_shapeOutlineVBCapacity = 0;
    std::vector<ShapeDrawRange> m_shapeOutlineRanges;

    ComPtr<ID3D11Buffer> m_handleVB;
    UINT                 m_handleVBCapacity = 0;
    std::vector<ShapeDrawRange> m_handleRanges;

    // Small filled red squares marking a Circle's center (see
    // ShapeRenderEntry::hasCenterMarker). Constant screen size like handles
    // (drawn with the identity transform), but filled (triangle list) and
    // always red, regardless of selection/fill state.
    ComPtr<ID3D11Buffer> m_centerMarkerVB;
    UINT                 m_centerMarkerVBCapacity = 0;
    std::vector<ShapeDrawRange> m_centerMarkerRanges;

    // Translucent fill triangles for closed shapes with wantFill set (see
    // ShapeRenderEntry), fan-triangulated from each shape's own outline.
    ComPtr<ID3D11Buffer> m_fillVB;
    UINT                 m_fillVBCapacity = 0;
    std::vector<ShapeDrawRange> m_fillRanges;

    // m_blendStateOpaque is bound at the start of every Render() so the
    // image quad always draws fully opaque regardless of whatever blend
    // state a previous frame's RenderShapes() left bound (D3D11 blend state
    // is sticky context state, not reset automatically between draws).
    // m_blendStateAlpha is bound for the whole shape-overlay pass: outline/
    // handle colors all have alpha=1 so blending vs. not blending looks
    // identical for them, while the fill triangles' reduced alpha actually
    // blends with whatever's underneath.
    ComPtr<ID3D11BlendState> m_blendStateOpaque;
    ComPtr<ID3D11BlendState> m_blendStateAlpha;
};
