// D3DRenderer.cpp
// ---------------------------------------------------------------------------
#include "D3DRenderer.h"

#include <d3dcompiler.h>
#include <wincodec.h>
#include <vector>
#include <cstring>
#include <cmath>
#include <chrono>
#include <algorithm>

// Auto-link the required libraries so the host project does not have to edit
// linker settings. (Requires the Windows 10 SDK.)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace
{
    struct Vertex { float x, y; float u, v; };

    // Zoom limits (image-px -> screen-px ratio).
    constexpr float kMinZoom = 0.02f;
    constexpr float kMaxZoom = 64.0f;

    // FL11 maximum 2D texture dimension. Used as both (a) the threshold above
    // which LoadImage switches to the tiled path (see UploadPixels), and
    // (b) implicitly the upper bound on a single tile's size (kTileSize
    // below is well under it).
    constexpr UINT  kMaxTextureDim = 16384;

    // Tile size for the streaming path (Phase 2). 4096x4096 RGBA = 64 MiB per
    // tile: large enough to keep tile/draw-call counts low for realistic
    // gigapixel images (a 14304x19000 image needs only a 4x5 = 20-tile grid)
    // while staying a comfortably small single GPU allocation.
    constexpr UINT  kTileSize = 4096;

    // Resident-tile VRAM budget for the tiled path. EvictTilesIfOverBudget
    // brings usage down to the low-water mark once usage exceeds the
    // high-water mark, rather than evicting back to exactly the budget every
    // frame -- the gap between the two is hysteresis so panning back and
    // forth across a tile boundary doesn't re-upload/evict the same tile
    // every frame.
    constexpr UINT64 kTileEvictHighWaterBytes = 1536ull * 1024 * 1024; // 1.5 GiB
    constexpr UINT64 kTileEvictLowWaterBytes  = 1228ull * 1024 * 1024; // ~1.2 GiB (80%)

    // Unit quad centred on the origin, in [-0.5, 0.5]. The vertex shader scales
    // it by image size / zoom / viewport into NDC, so this buffer is static.
    //   v0 top-left      v1 top-right
    //   v2 bottom-left   v3 bottom-right
    const Vertex kQuad[6] =
    {
        { -0.5f, -0.5f, 0.0f, 0.0f }, // v0
        {  0.5f, -0.5f, 1.0f, 0.0f }, // v1
        { -0.5f,  0.5f, 0.0f, 1.0f }, // v2
        { -0.5f,  0.5f, 0.0f, 1.0f }, // v2
        {  0.5f, -0.5f, 1.0f, 0.0f }, // v1
        {  0.5f,  0.5f, 1.0f, 1.0f }, // v3
    };

    // HLSL (compiled at runtime via D3DCompile so there is no shader build step).
    const char* kShaderSrc = R"(
cbuffer Transform : register(b0)
{
    float4 gScaleTrans;   // xy = scale, zw = translate (NDC)
};

struct VSIn  { float2 pos : POSITION; float2 uv : TEXCOORD0; };
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.pos = float4(i.pos * gScaleTrans.xy + gScaleTrans.zw, 0.0f, 1.0f);
    o.uv  = i.uv;
    return o;
}

Texture2D    gTex : register(t0);
SamplerState gSmp : register(s0);

float4 PSMain(VSOut i) : SV_TARGET
{
    return gTex.Sample(gSmp, i.uv);
}
)";

    // Flat per-vertex-color shader for shape outlines and resize handles --
    // no texture sampling. Reuses the same b0 "scale/translate" convention as
    // the image shader so shape outlines drawn through it line up with the
    // image; handle quads are drawn with an identity transform bound instead
    // (see D3DRenderer::RenderShapes), since their vertices are precomputed
    // CPU-side into final NDC space.
    const char* kShapeShaderSrc = R"(
cbuffer Transform : register(b0)
{
    float4 gScaleTrans;   // xy = scale, zw = translate (NDC)
};

struct VSIn  { float2 pos : POSITION; float4 color : COLOR0; };
struct VSOut { float4 pos : SV_POSITION; float4 color : COLOR0; };

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.pos = float4(i.pos * gScaleTrans.xy + gScaleTrans.zw, 0.0f, 1.0f);
    o.color = i.color;
    return o;
}

float4 PSMain(VSOut i) : SV_TARGET
{
    return i.color;
}
)";

    bool CompileShader(const char* source, const char* entry, const char* target, ID3DBlob** out)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ComPtr<ID3DBlob> errors;
        HRESULT hr = D3DCompile(source, std::strlen(source), "viewer.hlsl",
                                nullptr, nullptr, entry, target, flags, 0, out, &errors);
        return SUCCEEDED(hr);
    }

    // Outline/handle colors (RGBA, 0..1).
    constexpr float kNormalColor[4]   = { 1.0f, 1.0f, 0.0f, 1.0f }; // yellow: finished, unselected
    constexpr float kSelectedColor[4] = { 1.0f, 0.15f, 0.15f, 1.0f }; // red: selected
    constexpr float kPreviewColor[4]  = { 1.0f, 0.6f, 0.0f, 1.0f }; // orange: still being drawn
    constexpr float kHandleColor[4]   = { 1.0f, 1.0f, 1.0f, 1.0f }; // white: resize handles
    constexpr float kHandleScreenHalfSizePx = 4.0f;

    // Fill triangles reuse the outline's RGB (so a filled selected shape is
    // still tinted red, etc.) at this much lower alpha, so the image content
    // underneath stays visible through it.
    constexpr float kFillAlpha = 0.32f;
}

D3DRenderer::~D3DRenderer()
{
    Shutdown();
}

void D3DRenderer::Shutdown()
{
    if (m_context) m_context->ClearState();
    ReleaseRenderTarget();
    m_imageSRV.Reset();
    m_imageTexture.Reset();
    // Tiled-path GPU state must not outlive the device that owns it (matters
    // for SwitchAdapter, which calls Shutdown() then reinitializes on a new
    // adapter and reloads the image from scratch -- the reload fully
    // repopulates m_hostPixels/m_tiles, so nothing is lost by clearing here).
    m_isTiled = false;
    m_tiles.clear();
    m_hostPixels.clear();
    m_hostPixels.shrink_to_fit();
    m_residentTileBytes = 0;
    m_tileVB.Reset();
    m_tileVBCapacity = 0;
    m_sampler.Reset();
    m_rasterState.Reset();
    m_constantBuffer.Reset();
    m_vertexBuffer.Reset();
    m_inputLayout.Reset();
    m_ps.Reset();
    m_vs.Reset();
    m_shapeOutlineVB.Reset();
    m_shapeOutlineVBCapacity = 0;
    m_handleVB.Reset();
    m_handleVBCapacity = 0;
    m_centerMarkerVB.Reset();
    m_centerMarkerVBCapacity = 0;
    m_fillVB.Reset();
    m_fillVBCapacity = 0;
    m_blendStateOpaque.Reset();
    m_blendStateAlpha.Reset();
    m_identityConstantBuffer.Reset();
    m_shapeInputLayout.Reset();
    m_shapePS.Reset();
    m_shapeVS.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

std::vector<D3DAdapterInfo> D3DRenderer::EnumerateAdapters()
{
    std::vector<D3DAdapterInfo> result;

    ComPtr<IDXGIFactory1> factory;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
    {
        for (UINT i = 0; ; ++i)
        {
            ComPtr<IDXGIAdapter1> adapter;
            if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) break;

            DXGI_ADAPTER_DESC1 desc = {};
            if (SUCCEEDED(adapter->GetDesc1(&desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
            {
                D3DAdapterInfo info;
                info.description  = desc.Description;
                info.isWarp       = false;
                info.adapterIndex = i;
                result.push_back(std::move(info));
            }
        }
    }

    // Always offer the CPU software rasterizer as the last choice.
    D3DAdapterInfo warp;
    warp.description = L"WARP (Software / CPU)";
    warp.isWarp       = true;
    result.push_back(std::move(warp));

    return result;
}

bool D3DRenderer::Initialize(HWND hWnd, UINT width, UINT height)
{
    return InitializeInternal(hWnd, width, height, nullptr);
}

bool D3DRenderer::InitializeWithAdapter(HWND hWnd, UINT width, UINT height, const D3DAdapterInfo& adapter)
{
    return InitializeInternal(hWnd, width, height, &adapter);
}

bool D3DRenderer::InitializeInternal(HWND hWnd, UINT width, UINT height, const D3DAdapterInfo* explicitAdapter)
{
    m_viewportWidth  = width  ? width  : 1;
    m_viewportHeight = height ? height : 1;

    if (!CreateDeviceAndSwapChain(hWnd, m_viewportWidth, m_viewportHeight, explicitAdapter)) return false;
    if (!CreateRenderTarget())                                                              return false;
    if (!CreatePipeline())                                                                   return false;
    return true;
}

bool D3DRenderer::TryCreate(HWND hWnd, const DXGI_SWAP_CHAIN_DESC& scd, UINT flags, const D3DAdapterInfo* explicitAdapter)
{
    const D3D_FEATURE_LEVEL levels[] =
    {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0
    };

    // explicitAdapter == nullptr: auto-select, same as before (try the
    // default GPU, then fall back to WARP).
    if (!explicitAdapter)
    {
        const D3D_DRIVER_TYPE driverTypes[] =
        {
            D3D_DRIVER_TYPE_HARDWARE,  // real GPU (NVIDIA / AMD / Intel)
            D3D_DRIVER_TYPE_WARP       // CPU software rasterizer
        };

        for (D3D_DRIVER_TYPE dt : driverTypes)
        {
            D3D_FEATURE_LEVEL obtained = D3D_FEATURE_LEVEL_11_0;
            DXGI_SWAP_CHAIN_DESC desc = scd;     // copy; D3D may not modify, but be safe
            HRESULT hr = D3D11CreateDeviceAndSwapChain(
                nullptr, dt, nullptr, flags,
                levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                &desc, m_swapChain.ReleaseAndGetAddressOf(),
                m_device.ReleaseAndGetAddressOf(), &obtained,
                m_context.ReleaseAndGetAddressOf());

            if (SUCCEEDED(hr))
            {
                m_isHardware = (dt == D3D_DRIVER_TYPE_HARDWARE);
                return true;
            }
        }
        return false;
    }

    // explicitAdapter != nullptr: the user picked a specific GPU or WARP.
    // Per D3D11CreateDevice rules: a non-null adapter requires DriverType
    // UNKNOWN; WARP requires a null adapter.
    ComPtr<IDXGIAdapter1> namedAdapter;
    D3D_DRIVER_TYPE dt = D3D_DRIVER_TYPE_UNKNOWN;
    if (explicitAdapter->isWarp)
    {
        dt = D3D_DRIVER_TYPE_WARP;
    }
    else
    {
        ComPtr<IDXGIFactory1> factory;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;
        if (FAILED(factory->EnumAdapters1(explicitAdapter->adapterIndex, &namedAdapter))) return false;
    }

    D3D_FEATURE_LEVEL obtained = D3D_FEATURE_LEVEL_11_0;
    DXGI_SWAP_CHAIN_DESC desc = scd;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        namedAdapter.Get(), dt, nullptr, flags,
        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
        &desc, m_swapChain.ReleaseAndGetAddressOf(),
        m_device.ReleaseAndGetAddressOf(), &obtained,
        m_context.ReleaseAndGetAddressOf());

    if (FAILED(hr)) return false;
    m_isHardware = !explicitAdapter->isWarp;
    return true;
}

bool D3DRenderer::CreateDeviceAndSwapChain(HWND hWnd, UINT width, UINT height, const D3DAdapterInfo* explicitAdapter)
{
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount                        = 1;
    scd.BufferDesc.Width                   = width;
    scd.BufferDesc.Height                  = height;
    scd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator   = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow                       = hWnd;
    scd.SampleDesc.Count                   = 1;
    scd.SampleDesc.Quality                 = 0;
    scd.Windowed                           = TRUE;
    scd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Try with the requested flags first. If that fails (e.g. the D3D debug
    // layer is not installed on this machine), retry without the debug flag.
    if (TryCreate(hWnd, scd, flags, explicitAdapter)) { /* ok */ }
    else if ((flags & D3D11_CREATE_DEVICE_DEBUG) && TryCreate(hWnd, scd, flags & ~D3D11_CREATE_DEVICE_DEBUG, explicitAdapter)) { /* ok */ }
    else return false;

    // Read the adapter description for the status bar ("GPU name" / "WARP").
    ComPtr<IDXGIDevice> dxgiDevice;
    if (SUCCEEDED(m_device.As(&dxgiDevice)))
    {
        ComPtr<IDXGIAdapter> adapter;
        if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter)))
        {
            DXGI_ADAPTER_DESC ad = {};
            if (SUCCEEDED(adapter->GetDesc(&ad)))
                wcsncpy_s(m_deviceDesc, ad.Description, _TRUNCATE);
        }
    }
    return true;
}

bool D3DRenderer::CreateRenderTarget()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
    return SUCCEEDED(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_rtv));
}

void D3DRenderer::ReleaseRenderTarget()
{
    if (m_context) m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_rtv.Reset();
}

bool D3DRenderer::CreatePipeline()
{
    // --- Shaders ---
    ComPtr<ID3DBlob> vsBlob, psBlob;
    if (!CompileShader(kShaderSrc, "VSMain", "vs_5_0", &vsBlob)) return false;
    if (!CompileShader(kShaderSrc, "PSMain", "ps_5_0", &psBlob)) return false;

    if (FAILED(m_device->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, &m_vs))) return false;
    if (FAILED(m_device->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, &m_ps))) return false;

    // --- Input layout ---
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (FAILED(m_device->CreateInputLayout(layout, ARRAYSIZE(layout),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout))) return false;

    // --- Vertex buffer (static unit quad) ---
    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = sizeof(kQuad);
    vbd.Usage     = D3D11_USAGE_IMMUTABLE;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbInit = {};
    vbInit.pSysMem = kQuad;
    if (FAILED(m_device->CreateBuffer(&vbd, &vbInit, &m_vertexBuffer))) return false;

    // --- Constant buffer (transform; updated each frame) ---
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth      = sizeof(float) * 4;          // one float4, 16-byte aligned
    cbd.Usage          = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(m_device->CreateBuffer(&cbd, nullptr, &m_constantBuffer))) return false;

    // --- Sampler (linear, clamp) ---
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD         = D3D11_FLOAT32_MAX;
    if (FAILED(m_device->CreateSamplerState(&sd, &m_sampler))) return false;

    // --- Rasterizer (no culling so quad winding never matters) ---
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode        = D3D11_FILL_SOLID;
    rd.CullMode        = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(m_device->CreateRasterizerState(&rd, &m_rasterState))) return false;

    if (!CreateShapePipeline()) return false;

    return true;
}

bool D3DRenderer::CreateShapePipeline()
{
    ComPtr<ID3DBlob> vsBlob, psBlob;
    if (!CompileShader(kShapeShaderSrc, "VSMain", "vs_5_0", &vsBlob)) return false;
    if (!CompileShader(kShapeShaderSrc, "PSMain", "ps_5_0", &psBlob)) return false;

    if (FAILED(m_device->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, &m_shapeVS))) return false;
    if (FAILED(m_device->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, &m_shapePS))) return false;

    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (FAILED(m_device->CreateInputLayout(layout, ARRAYSIZE(layout),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_shapeInputLayout))) return false;

    // Identity transform, bound when drawing handle-quad vertices that are
    // already in final NDC space (their constant on-screen size is computed
    // on the CPU each rebuild, so they must bypass the real image transform).
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(float) * 4;
    cbd.Usage     = D3D11_USAGE_IMMUTABLE;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    const float identity[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = identity;
    if (FAILED(m_device->CreateBuffer(&cbd, &init, &m_identityConstantBuffer))) return false;

    // Opaque: bound at the top of every Render() so the image quad always
    // draws fully opaque, regardless of whatever blend state a previous
    // frame's RenderShapes() left bound.
    D3D11_BLEND_DESC bdOpaque = {};
    bdOpaque.RenderTarget[0].BlendEnable = FALSE;
    bdOpaque.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(m_device->CreateBlendState(&bdOpaque, &m_blendStateOpaque))) return false;

    // Standard "over" alpha blending, used for the whole shape-overlay pass
    // (fill triangles actually blend; outline/handle colors have alpha=1 so
    // they look identical blended or not).
    D3D11_BLEND_DESC bdAlpha = {};
    bdAlpha.RenderTarget[0].BlendEnable    = TRUE;
    bdAlpha.RenderTarget[0].SrcBlend       = D3D11_BLEND_SRC_ALPHA;
    bdAlpha.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
    bdAlpha.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
    bdAlpha.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
    bdAlpha.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bdAlpha.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
    bdAlpha.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(m_device->CreateBlendState(&bdAlpha, &m_blendStateAlpha))) return false;

    return true;
}

bool D3DRenderer::Resize(UINT width, UINT height)
{
    if (!m_swapChain) return false;
    if (width  == 0) width  = 1;
    if (height == 0) height = 1;

    m_viewportWidth  = width;
    m_viewportHeight = height;
    m_shapesDirty = true; // viewport size feeds ComputeImageTransform() -- see ZoomAt's comment

    ReleaseRenderTarget();
    if (FAILED(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)))
        return false;
    return CreateRenderTarget();
}

D3DRenderer::D3DLoadResult D3DRenderer::DecodeToPixels(
    IWICImagingFactory* factory, IWICBitmapDecoder* decoder,
    UINT& outWidth, UINT& outHeight, std::vector<BYTE>& outPixels)
{
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return D3DLoadResult::DecodeFailed;

    // Convert to straight 32bpp RGBA regardless of source format.
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter))) return D3DLoadResult::DecodeFailed;
    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
        return D3DLoadResult::DecodeFailed;

    UINT w = 0, h = 0;
    converter->GetSize(&w, &h);
    if (w == 0 || h == 0) return D3DLoadResult::DecodeFailed;

    // No size rejection here: images beyond the FL11 single-texture limit
    // (kMaxTextureDim) are handled by UploadPixels' tiled path instead. The
    // try/catch below already reports a clean OutOfMemory for truly
    // infeasible sizes rather than crashing.
    const UINT stride  = w * 4;
    const UINT bufSize = stride * h;
    std::vector<BYTE> pixels;
    try { pixels.resize(bufSize); }
    catch (...) { return D3DLoadResult::OutOfMemory; }
    if (FAILED(converter->CopyPixels(nullptr, stride, bufSize, pixels.data())))
        return D3DLoadResult::DecodeFailed;

    outWidth  = w;
    outHeight = h;
    outPixels = std::move(pixels);
    return D3DLoadResult::Ok;
}

D3DRenderer::D3DLoadResult D3DRenderer::SehGuardedDecodeToPixels(
    IWICImagingFactory* factory, IWICBitmapDecoder* decoder,
    UINT& outWidth, UINT& outHeight, std::vector<BYTE>& outPixels)
{
    // Deliberately no destructible C++ locals in THIS frame -- everything
    // with RAII lives in DecodeToPixels, one frame down, where it is
    // perfectly safe; __try/__except just cannot coexist with such locals
    // in the exact frame that contains the __try block itself.
    __try
    {
        return DecodeToPixels(factory, decoder, outWidth, outHeight, outPixels);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return D3DLoadResult::DecodeFailed;
    }
}

D3DRenderer::D3DLoadResult D3DRenderer::UploadPixels(UINT width, UINT height, std::vector<BYTE>& pixels)
{
    // Clear any previous load's state from BOTH paths unconditionally, so a
    // huge image loaded after a normal one (or vice versa) never leaves
    // stale tiles/single-texture state behind.
    m_imageTexture.Reset();
    m_imageSRV.Reset();
    m_isTiled = false;
    m_tiles.clear();
    m_hostPixels.clear();
    m_hostPixels.shrink_to_fit();
    m_residentTileBytes = 0;

    if (width > kMaxTextureDim || height > kMaxTextureDim)
    {
        // Tiled path: keep the decoded buffer resident; tiles are created
        // lazily from it inside RenderTiles() as they become visible.
        m_hostPixels  = std::move(pixels);
        m_imageWidth  = width;
        m_imageHeight = height;
        m_isTiled     = true;
        BuildTileGrid();
        ResetView();
        return D3DLoadResult::Ok;
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = width;
    td.Height           = height;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_IMMUTABLE;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem     = pixels.data();
    init.SysMemPitch = width * 4;

    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(m_device->CreateTexture2D(&td, &init, &tex))) return D3DLoadResult::GpuUploadFailed;
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(m_device->CreateShaderResourceView(tex.Get(), nullptr, &srv))) return D3DLoadResult::GpuUploadFailed;

    m_imageTexture = tex;
    m_imageSRV     = srv;
    m_imageWidth   = width;
    m_imageHeight  = height;
    ResetView();
    return D3DLoadResult::Ok;
}

void D3DRenderer::BuildTileGrid()
{
    m_tileGridCols = (m_imageWidth  + kTileSize - 1) / kTileSize;
    m_tileGridRows = (m_imageHeight + kTileSize - 1) / kTileSize;

    m_tiles.clear();
    m_tiles.reserve(static_cast<size_t>(m_tileGridCols) * m_tileGridRows);
    for (UINT row = 0; row < m_tileGridRows; ++row)
    {
        for (UINT col = 0; col < m_tileGridCols; ++col)
        {
            ImageTile tile;
            tile.pixelX = col * kTileSize;
            tile.pixelY = row * kTileSize;
            tile.pixelW = (std::min)(kTileSize, m_imageWidth  - tile.pixelX);
            tile.pixelH = (std::min)(kTileSize, m_imageHeight - tile.pixelY);
            m_tiles.push_back(std::move(tile));
        }
    }
    m_residentTileBytes = 0;
}

bool D3DRenderer::EnsureTileResident(ImageTile& tile)
{
    if (tile.texture) return true; // already resident

    // m_hostPixels' row stride is the WHOLE image's width * 4, not this
    // tile's, so D3D11_SUBRESOURCE_DATA (which describes one contiguous
    // buffer with one pitch for just this tile) needs its own tightly
    // packed staging buffer rather than pointing partway into m_hostPixels.
    const UINT srcStride = m_imageWidth * 4;
    const UINT dstStride = tile.pixelW * 4;
    std::vector<BYTE> staging;
    try { staging.resize(static_cast<size_t>(dstStride) * tile.pixelH); }
    catch (...) { return false; } // leave tile non-resident; caller just skips drawing it this frame
    for (UINT row = 0; row < tile.pixelH; ++row)
    {
        const BYTE* src = m_hostPixels.data() + static_cast<size_t>(tile.pixelY + row) * srcStride + static_cast<size_t>(tile.pixelX) * 4;
        BYTE* dst = staging.data() + static_cast<size_t>(row) * dstStride;
        std::memcpy(dst, src, dstStride);
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = tile.pixelW;
    td.Height           = tile.pixelH;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_IMMUTABLE;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem     = staging.data();
    init.SysMemPitch = dstStride;

    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(m_device->CreateTexture2D(&td, &init, &tex))) return false;
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(m_device->CreateShaderResourceView(tex.Get(), nullptr, &srv))) return false;

    tile.texture = tex;
    tile.srv     = srv;
    m_residentTileBytes += static_cast<UINT64>(dstStride) * tile.pixelH;
    return true;
}

void D3DRenderer::GetVisibleTileRange(UINT& outCol0, UINT& outCol1, UINT& outRow0, UINT& outRow1) const
{
    const PointF topLeft     = ScreenToImage(0, 0);
    const PointF bottomRight = ScreenToImage(static_cast<int>(m_viewportWidth), static_cast<int>(m_viewportHeight));

    float minX = (std::min)(topLeft.x, bottomRight.x);
    float maxX = (std::max)(topLeft.x, bottomRight.x);
    float minY = (std::min)(topLeft.y, bottomRight.y);
    float maxY = (std::max)(topLeft.y, bottomRight.y);

    minX = (std::max)(0.0f, minX); maxX = (std::min)(static_cast<float>(m_imageWidth),  maxX);
    minY = (std::max)(0.0f, minY); maxY = (std::min)(static_cast<float>(m_imageHeight), maxY);

    if (maxX <= minX || maxY <= minY || m_tileGridCols == 0 || m_tileGridRows == 0)
    {
        outCol0 = outCol1 = outRow0 = outRow1 = 0;
        return;
    }

    outCol0 = static_cast<UINT>(minX) / kTileSize;
    outCol1 = (std::min)(m_tileGridCols - 1, static_cast<UINT>(maxX) / kTileSize);
    outRow0 = static_cast<UINT>(minY) / kTileSize;
    outRow1 = (std::min)(m_tileGridRows - 1, static_cast<UINT>(maxY) / kTileSize);
}

void D3DRenderer::RenderTiles()
{
    ++m_frameCounter;

    UINT col0 = 0, col1 = 0, row0 = 0, row1 = 0;
    GetVisibleTileRange(col0, col1, row0, row1);

    std::vector<UINT> visibleIndices;
    if (m_tileGridCols > 0 && m_tileGridRows > 0)
    {
        visibleIndices.reserve(static_cast<size_t>(col1 - col0 + 1) * (row1 - row0 + 1));
        for (UINT row = row0; row <= row1; ++row)
            for (UINT col = col0; col <= col1; ++col)
                visibleIndices.push_back(row * m_tileGridCols + col);
    }

    UpdateConstantBuffer(); // whole-image gScaleTrans; same for every tile this frame

    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vs.Get(), nullptr, 0);
    ID3D11Buffer* cbs[] = { m_constantBuffer.Get() };
    m_context->VSSetConstantBuffers(0, 1, cbs);
    m_context->PSSetShader(m_ps.Get(), nullptr, 0);
    ID3D11SamplerState* samps[] = { m_sampler.Get() };
    m_context->PSSetSamplers(0, 1, samps);
    m_context->RSSetState(m_rasterState.Get());

    const float invW = 1.0f / static_cast<float>(m_imageWidth);
    const float invH = 1.0f / static_cast<float>(m_imageHeight);
    const UINT stride = sizeof(TileVertex);
    const UINT offset = 0;

    if (!m_tileVB)
    {
        D3D11_BUFFER_DESC vbd = {};
        vbd.ByteWidth      = sizeof(TileVertex) * 6;
        vbd.Usage          = D3D11_USAGE_DYNAMIC;
        vbd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (SUCCEEDED(m_device->CreateBuffer(&vbd, nullptr, m_tileVB.ReleaseAndGetAddressOf())))
            m_tileVBCapacity = 6;
    }

    for (UINT idx : visibleIndices)
    {
        ImageTile& tile = m_tiles[idx];
        if (!EnsureTileResident(tile)) continue; // OOM/GPU-upload failure: skip this tile this frame, retry next frame
        tile.lastUsedFrame = m_frameCounter;

        const float left   = tile.pixelX                * invW - 0.5f;
        const float right  = (tile.pixelX + tile.pixelW) * invW - 0.5f;
        const float top    = tile.pixelY                * invH - 0.5f;
        const float bottom = (tile.pixelY + tile.pixelH) * invH - 0.5f;
        const TileVertex verts[6] =
        {
            { left,  top,    0.0f, 0.0f },
            { right, top,    1.0f, 0.0f },
            { left,  bottom, 0.0f, 1.0f },
            { left,  bottom, 0.0f, 1.0f },
            { right, top,    1.0f, 0.0f },
            { right, bottom, 1.0f, 1.0f },
        };

        if (!m_tileVB) continue;
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(m_context->Map(m_tileVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) continue;
        std::memcpy(mapped.pData, verts, sizeof(verts));
        m_context->Unmap(m_tileVB.Get(), 0);

        ID3D11Buffer* vbs[] = { m_tileVB.Get() };
        m_context->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
        ID3D11ShaderResourceView* srvs[] = { tile.srv.Get() };
        m_context->PSSetShaderResources(0, 1, srvs);
        m_context->Draw(6, 0);
    }

    EvictTilesIfOverBudget(visibleIndices);
}

void D3DRenderer::EvictTilesIfOverBudget(const std::vector<UINT>& visibleIndices)
{
    if (m_residentTileBytes <= kTileEvictHighWaterBytes) return;

    std::vector<UINT> evictionCandidates; // resident, NOT in the current visible set
    evictionCandidates.reserve(m_tiles.size());
    for (UINT i = 0; i < m_tiles.size(); ++i)
    {
        if (!m_tiles[i].texture) continue;
        if (std::find(visibleIndices.begin(), visibleIndices.end(), i) != visibleIndices.end()) continue;
        evictionCandidates.push_back(i);
    }

    std::sort(evictionCandidates.begin(), evictionCandidates.end(), [this](UINT a, UINT b)
    {
        return m_tiles[a].lastUsedFrame < m_tiles[b].lastUsedFrame; // oldest first
    });

    for (UINT i : evictionCandidates)
    {
        if (m_residentTileBytes <= kTileEvictLowWaterBytes) break;
        ImageTile& tile = m_tiles[i];
        const UINT64 tileBytes = static_cast<UINT64>(tile.pixelW) * 4 * tile.pixelH;
        tile.texture.Reset();
        tile.srv.Reset();
        m_residentTileBytes -= tileBytes;
    }
}

D3DRenderer::D3DLoadResult D3DRenderer::LoadImageFromFile(const wchar_t* path)
{
    if (!m_device) return D3DLoadResult::NoDevice;
    if (!path || !*path) return D3DLoadResult::InvalidArgument;

    const auto startTime = std::chrono::steady_clock::now();
    struct LoadTimer
    {
        D3DRenderer* self;
        std::chrono::steady_clock::time_point start;
        ~LoadTimer()
        {
            const auto elapsed = std::chrono::steady_clock::now() - start;
            self->m_lastLoadMs = std::chrono::duration<float, std::milli>(elapsed).count();
        }
    } timer{ this, startTime };

    m_lastFileSizeBytes = 0;
    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (GetFileAttributesExW(path, GetFileExInfoStandard, &fad))
        m_lastFileSizeBytes = (static_cast<UINT64>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;

    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return D3DLoadResult::DecodeFailed;

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnDemand, &decoder))) return D3DLoadResult::DecodeFailed;

    UINT w = 0, h = 0;
    std::vector<BYTE> pixels;
    // File-backed decoding reads from a Win32 file handle, not an arbitrary
    // caller pointer, so this path doesn't need the SEH guard the memory
    // path does -- a malformed file fails through normal WIC HRESULTs.
    D3DLoadResult result = DecodeToPixels(factory.Get(), decoder.Get(), w, h, pixels);
    if (result != D3DLoadResult::Ok) return result;

    return UploadPixels(w, h, pixels); // UploadPixels moves out of `pixels` itself when tiling
}

D3DRenderer::D3DLoadResult D3DRenderer::LoadImageFromMemory(const void* data, size_t dataSize)
{
    if (!m_device) return D3DLoadResult::NoDevice;
    if (!data || dataSize == 0 || dataSize > MAXDWORD) return D3DLoadResult::InvalidArgument;

    const auto startTime = std::chrono::steady_clock::now();
    struct LoadTimer
    {
        D3DRenderer* self;
        std::chrono::steady_clock::time_point start;
        ~LoadTimer()
        {
            const auto elapsed = std::chrono::steady_clock::now() - start;
            self->m_lastLoadMs = std::chrono::duration<float, std::milli>(elapsed).count();
        }
    } timer{ this, startTime };

    m_lastFileSizeBytes = static_cast<UINT64>(dataSize);

    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return D3DLoadResult::DecodeFailed;

    ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream))) return D3DLoadResult::DecodeFailed;

    // InitializeFromMemory's signature wants a non-const BYTE*; WIC does not
    // modify the buffer through it. The buffer only needs to stay valid for
    // this call -- everything below is synchronous.
    if (FAILED(stream->InitializeFromMemory(
            static_cast<BYTE*>(const_cast<void*>(data)), static_cast<DWORD>(dataSize))))
        return D3DLoadResult::DecodeFailed;

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr,
        WICDecodeMetadataCacheOnDemand, &decoder)))
        return D3DLoadResult::DecodeFailed;

    UINT w = 0, h = 0;
    std::vector<BYTE> pixels;
    D3DLoadResult result = SehGuardedDecodeToPixels(factory.Get(), decoder.Get(), w, h, pixels);
    if (result != D3DLoadResult::Ok) return result;

    return UploadPixels(w, h, pixels);
}

void D3DRenderer::ResetView()
{
    if (m_imageWidth == 0 || m_imageHeight == 0) { m_zoom = 1.0f; m_panX = m_panY = 0.0f; return; }

    const float fx = static_cast<float>(m_viewportWidth)  / static_cast<float>(m_imageWidth);
    const float fy = static_cast<float>(m_viewportHeight) / static_cast<float>(m_imageHeight);
    float fit = (fx < fy) ? fx : fy;
    if (fit > 1.0f) fit = 1.0f;     // don't upscale small images past 100%

    m_zoom = fit;
    m_panX = 0.0f;
    m_panY = 0.0f;
    m_shapesDirty = true; // see ZoomAt's comment
}

void D3DRenderer::ZoomAt(float factor, int screenX, int screenY)
{
    float newZoom = m_zoom * factor;
    if (newZoom < kMinZoom) newZoom = kMinZoom;
    if (newZoom > kMaxZoom) newZoom = kMaxZoom;
    factor = newZoom / m_zoom;      // recompute actual factor after clamping

    // Keep the image point under the cursor fixed.
    const float csx = screenX - static_cast<float>(m_viewportWidth)  * 0.5f;
    const float csy = screenY - static_cast<float>(m_viewportHeight) * 0.5f;
    m_panX = csx - (csx - m_panX) * factor;
    m_panY = csy - (csy - m_panY) * factor;
    m_zoom = newZoom;

    // The outline vertices are stored in unit-quad space and re-projected
    // through the per-frame constant buffer every Render() call, so they
    // already track zoom/pan automatically. Handles and the circle center
    // marker are the exception: they're precomputed to final NDC at the
    // last rebuild and drawn with an IDENTITY transform (so their SIZE
    // stays constant on screen) -- which means their POSITION goes stale
    // the moment zoom/pan changes, until something forces a rebuild. Force
    // one now so they track the view immediately instead of snapping back
    // into place only on the next shape interaction.
    m_shapesDirty = true;
}

void D3DRenderer::Pan(float dxPixels, float dyPixels)
{
    m_panX += dxPixels;
    m_panY += dyPixels;
    m_shapesDirty = true; // see ZoomAt's comment
}

void D3DRenderer::ComputeImageTransform(float& sx, float& sy, float& tx, float& ty) const
{
    const float vpW = static_cast<float>(m_viewportWidth);
    const float vpH = static_cast<float>(m_viewportHeight);

    // Image size on screen (px) = image_px * zoom, independent of viewport size.
    sx =  static_cast<float>(m_imageWidth)  * m_zoom * 2.0f / vpW;
    sy = -static_cast<float>(m_imageHeight) * m_zoom * 2.0f / vpH; // flip Y
    tx =  2.0f * m_panX / vpW;
    ty = -2.0f * m_panY / vpH;
}

void D3DRenderer::UpdateConstantBuffer()
{
    float sx, sy, tx, ty;
    ComputeImageTransform(sx, sy, tx, ty);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        const float data[4] = { sx, sy, tx, ty };
        std::memcpy(mapped.pData, data, sizeof(data));
        m_context->Unmap(m_constantBuffer.Get(), 0);
    }
}

PointF D3DRenderer::ImageToScreen(float imageX, float imageY) const
{
    if (m_imageWidth == 0 || m_imageHeight == 0) return PointF{ 0.0f, 0.0f };

    float sx, sy, tx, ty;
    ComputeImageTransform(sx, sy, tx, ty);

    const float unitX = imageX / static_cast<float>(m_imageWidth)  - 0.5f;
    const float unitY = imageY / static_cast<float>(m_imageHeight) - 0.5f;
    const float ndcX = unitX * sx + tx;
    const float ndcY = unitY * sy + ty;

    const float vpW = static_cast<float>(m_viewportWidth);
    const float vpH = static_cast<float>(m_viewportHeight);
    PointF screen;
    screen.x = (ndcX + 1.0f) * 0.5f * vpW;
    screen.y = (1.0f - ndcY) * 0.5f * vpH;
    return screen;
}

PointF D3DRenderer::ScreenToImage(int screenX, int screenY) const
{
    if (m_imageWidth == 0 || m_imageHeight == 0) return PointF{ 0.0f, 0.0f };

    float sx, sy, tx, ty;
    ComputeImageTransform(sx, sy, tx, ty);
    if (std::fabs(sx) < 1e-9f || std::fabs(sy) < 1e-9f) return PointF{ 0.0f, 0.0f };

    const float vpW = static_cast<float>(m_viewportWidth);
    const float vpH = static_cast<float>(m_viewportHeight);
    const float ndcX = (static_cast<float>(screenX) / vpW) * 2.0f - 1.0f;
    const float ndcY = 1.0f - (static_cast<float>(screenY) / vpH) * 2.0f;

    const float unitX = (ndcX - tx) / sx;
    const float unitY = (ndcY - ty) / sy;

    PointF image;
    image.x = (unitX + 0.5f) * static_cast<float>(m_imageWidth);
    image.y = (unitY + 0.5f) * static_cast<float>(m_imageHeight);
    return image;
}

void D3DRenderer::SetShapeRenderData(std::vector<ShapeRenderEntry> shapes)
{
    m_shapes = std::move(shapes);
    m_shapesDirty = true;
}

void D3DRenderer::UploadDynamicVertexBuffer(const std::vector<ShapeVertex>& verts, ComPtr<ID3D11Buffer>& buffer, UINT& capacity)
{
    if (verts.empty()) return; // leave any existing buffer/capacity alone; nothing will be drawn from it this frame

    const UINT neededBytes = static_cast<UINT>(verts.size() * sizeof(ShapeVertex));
    if (!buffer || capacity < verts.size())
    {
        D3D11_BUFFER_DESC vbd = {};
        vbd.ByteWidth      = neededBytes;
        vbd.Usage          = D3D11_USAGE_DYNAMIC;
        vbd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_device->CreateBuffer(&vbd, nullptr, buffer.ReleaseAndGetAddressOf())))
        {
            capacity = 0;
            return;
        }
        capacity = static_cast<UINT>(verts.size());
    }

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, verts.data(), neededBytes);
        m_context->Unmap(buffer.Get(), 0);
    }
}

void D3DRenderer::RebuildShapeVertexBuffers()
{
    m_shapeOutlineRanges.clear();
    m_handleRanges.clear();
    m_centerMarkerRanges.clear();
    m_fillRanges.clear();
    std::vector<ShapeVertex> outlineVerts;
    std::vector<ShapeVertex> handleVerts;
    std::vector<ShapeVertex> centerMarkerVerts;
    std::vector<ShapeVertex> fillVerts;

    const float invW = m_imageWidth  > 0 ? 1.0f / static_cast<float>(m_imageWidth)  : 0.0f;
    const float invH = m_imageHeight > 0 ? 1.0f / static_cast<float>(m_imageHeight) : 0.0f;

    float sx, sy, tx, ty;
    ComputeImageTransform(sx, sy, tx, ty);
    const float vpW = static_cast<float>(m_viewportWidth);
    const float vpH = static_cast<float>(m_viewportHeight);
    const float halfNdcX = kHandleScreenHalfSizePx * 2.0f / vpW;
    const float halfNdcY = kHandleScreenHalfSizePx * 2.0f / vpH;

    for (const auto& shape : m_shapes)
    {
        if (shape.outline.size() < 2) continue;
        const float* color = shape.selected ? kSelectedColor : (shape.inProgress ? kPreviewColor : kNormalColor);

        // Fan-triangulated fill, reusing the outline's own points -- exact
        // for Rectangle/Circle (always convex); a concave Polygon can show
        // minor double-blended overlap where the fan's triangles cross
        // outside the true interior, but never crashes or leaves gaps.
        if (shape.wantFill && shape.outline.size() >= 3)
        {
            const UINT fillStart = static_cast<UINT>(fillVerts.size());
            auto addFillVertex = [&](const PointF& p)
            {
                ShapeVertex v;
                v.x = p.x * invW - 0.5f;
                v.y = p.y * invH - 0.5f;
                v.r = color[0]; v.g = color[1]; v.b = color[2]; v.a = kFillAlpha;
                fillVerts.push_back(v);
            };
            for (size_t i = 1; i + 1 < shape.outline.size(); ++i)
            {
                addFillVertex(shape.outline[0]);
                addFillVertex(shape.outline[i]);
                addFillVertex(shape.outline[i + 1]);
            }
            ShapeDrawRange fillRange;
            fillRange.vertexOffset = fillStart;
            fillRange.vertexCount  = static_cast<UINT>(fillVerts.size()) - fillStart;
            m_fillRanges.push_back(fillRange);
        }

        const UINT startVertex = static_cast<UINT>(outlineVerts.size());
        for (const auto& p : shape.outline)
        {
            ShapeVertex v;
            v.x = p.x * invW - 0.5f;
            v.y = p.y * invH - 0.5f;
            v.r = color[0]; v.g = color[1]; v.b = color[2]; v.a = color[3];
            outlineVerts.push_back(v);
        }
        if (shape.closed)
            outlineVerts.push_back(outlineVerts[startVertex]); // duplicate first vertex to close the line strip

        ShapeDrawRange range;
        range.vertexOffset = startVertex;
        range.vertexCount  = static_cast<UINT>(outlineVerts.size()) - startVertex;
        m_shapeOutlineRanges.push_back(range);

        // Handle squares (selected, finished shape only -- ShapeManager only
        // populates `handles` in that case).
        for (const auto& h : shape.handles)
        {
            const float unitX = h.x * invW - 0.5f;
            const float unitY = h.y * invH - 0.5f;
            const float ndcX = unitX * sx + tx;
            const float ndcY = unitY * sy + ty;

            const UINT hStart = static_cast<UINT>(handleVerts.size());
            const float corners[5][2] =
            {
                { ndcX - halfNdcX, ndcY - halfNdcY },
                { ndcX + halfNdcX, ndcY - halfNdcY },
                { ndcX + halfNdcX, ndcY + halfNdcY },
                { ndcX - halfNdcX, ndcY + halfNdcY },
                { ndcX - halfNdcX, ndcY - halfNdcY }, // close the loop
            };
            for (const auto& c : corners)
            {
                ShapeVertex v;
                v.x = c[0]; v.y = c[1];
                v.r = kHandleColor[0]; v.g = kHandleColor[1]; v.b = kHandleColor[2]; v.a = kHandleColor[3];
                handleVerts.push_back(v);
            }
            ShapeDrawRange hRange;
            hRange.vertexOffset = hStart;
            hRange.vertexCount  = 5;
            m_handleRanges.push_back(hRange);
        }

        // Circle center marker: a small filled red square, constant screen
        // size (identity transform, like handles), always shown -- not
        // gated on selection or the fill setting.
        if (shape.hasCenterMarker)
        {
            const float unitX = shape.centerMarker.x * invW - 0.5f;
            const float unitY = shape.centerMarker.y * invH - 0.5f;
            const float ndcX = unitX * sx + tx;
            const float ndcY = unitY * sy + ty;

            constexpr float kMarkerHalfSizePx = 3.0f;
            const float halfMx = kMarkerHalfSizePx * 2.0f / vpW;
            const float halfMy = kMarkerHalfSizePx * 2.0f / vpH;

            const UINT mStart = static_cast<UINT>(centerMarkerVerts.size());
            const float quad[6][2] =
            {
                { ndcX - halfMx, ndcY - halfMy }, { ndcX + halfMx, ndcY - halfMy }, { ndcX + halfMx, ndcY + halfMy },
                { ndcX - halfMx, ndcY - halfMy }, { ndcX + halfMx, ndcY + halfMy }, { ndcX - halfMx, ndcY + halfMy },
            };
            for (const auto& c : quad)
            {
                ShapeVertex v;
                v.x = c[0]; v.y = c[1];
                v.r = 1.0f; v.g = 0.0f; v.b = 0.0f; v.a = 1.0f; // opaque red
                centerMarkerVerts.push_back(v);
            }
            ShapeDrawRange mRange;
            mRange.vertexOffset = mStart;
            mRange.vertexCount  = 6;
            m_centerMarkerRanges.push_back(mRange);
        }
    }

    UploadDynamicVertexBuffer(outlineVerts, m_shapeOutlineVB, m_shapeOutlineVBCapacity);
    UploadDynamicVertexBuffer(handleVerts, m_handleVB, m_handleVBCapacity);
    UploadDynamicVertexBuffer(centerMarkerVerts, m_centerMarkerVB, m_centerMarkerVBCapacity);
    UploadDynamicVertexBuffer(fillVerts, m_fillVB, m_fillVBCapacity);

    m_shapesDirty = false;
}

void D3DRenderer::RenderShapes()
{
    if (m_shapesDirty) RebuildShapeVertexBuffers();
    if (m_shapeOutlineRanges.empty() && m_handleRanges.empty() && m_fillRanges.empty() && m_centerMarkerRanges.empty()) return;

    m_context->IASetInputLayout(m_shapeInputLayout.Get());
    m_context->VSSetShader(m_shapeVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_shapePS.Get(), nullptr, 0);
    m_context->RSSetState(m_rasterState.Get());
    // Whole shape-overlay pass blends: the fill triangles' reduced alpha
    // actually needs it, and outline/handle colors (alpha=1) look identical
    // blended or not, so there's no need to toggle blend state mid-pass.
    m_context->OMSetBlendState(m_blendStateAlpha.Get(), nullptr, 0xFFFFFFFF);

    const UINT stride = sizeof(ShapeVertex);
    const UINT offset = 0;

    if (!m_fillRanges.empty())
    {
        ID3D11Buffer* vbs[] = { m_fillVB.Get() };
        m_context->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11Buffer* cbs[] = { m_constantBuffer.Get() };
        m_context->VSSetConstantBuffers(0, 1, cbs);
        for (const auto& r : m_fillRanges)
            m_context->Draw(r.vertexCount, r.vertexOffset);
    }

    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

    if (!m_shapeOutlineRanges.empty())
    {
        ID3D11Buffer* vbs[] = { m_shapeOutlineVB.Get() };
        m_context->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
        ID3D11Buffer* cbs[] = { m_constantBuffer.Get() }; // the real, already-updated-this-frame image transform
        m_context->VSSetConstantBuffers(0, 1, cbs);
        for (const auto& r : m_shapeOutlineRanges)
            m_context->Draw(r.vertexCount, r.vertexOffset);
    }

    if (!m_handleRanges.empty())
    {
        ID3D11Buffer* vbs[] = { m_handleVB.Get() };
        m_context->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
        ID3D11Buffer* cbs[] = { m_identityConstantBuffer.Get() }; // vertices are already final NDC
        m_context->VSSetConstantBuffers(0, 1, cbs);
        for (const auto& r : m_handleRanges)
            m_context->Draw(r.vertexCount, r.vertexOffset);
    }

    if (!m_centerMarkerRanges.empty())
    {
        ID3D11Buffer* vbs[] = { m_centerMarkerVB.Get() };
        m_context->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11Buffer* cbs[] = { m_identityConstantBuffer.Get() }; // vertices are already final NDC
        m_context->VSSetConstantBuffers(0, 1, cbs);
        for (const auto& r : m_centerMarkerRanges)
            m_context->Draw(r.vertexCount, r.vertexOffset);
    }
}

void D3DRenderer::Render()
{
    if (!m_context || !m_rtv) return;

    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(m_viewportWidth);
    vp.Height   = static_cast<float>(m_viewportHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);

    ID3D11RenderTargetView* rtvs[] = { m_rtv.Get() };
    m_context->OMSetRenderTargets(1, rtvs, nullptr);

    const float clear[4] = { 0.12f, 0.12f, 0.13f, 1.0f };
    m_context->ClearRenderTargetView(m_rtv.Get(), clear);

    // RenderShapes() (below) may leave the alpha blend state bound -- D3D11
    // blend state is sticky across draw calls, so without this the image
    // quad could end up alpha-blended against last frame's leftover
    // backbuffer content instead of drawing fully opaque.
    m_context->OMSetBlendState(m_blendStateOpaque.Get(), nullptr, 0xFFFFFFFF);

    if (m_imageSRV)
    {
        UpdateConstantBuffer();

        const UINT stride = sizeof(Vertex);
        const UINT offset = 0;
        ID3D11Buffer* vbs[] = { m_vertexBuffer.Get() };
        m_context->IASetInputLayout(m_inputLayout.Get());
        m_context->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_context->VSSetShader(m_vs.Get(), nullptr, 0);
        ID3D11Buffer* cbs[] = { m_constantBuffer.Get() };
        m_context->VSSetConstantBuffers(0, 1, cbs);

        m_context->PSSetShader(m_ps.Get(), nullptr, 0);
        ID3D11ShaderResourceView* srvs[] = { m_imageSRV.Get() };
        m_context->PSSetShaderResources(0, 1, srvs);
        ID3D11SamplerState* samps[] = { m_sampler.Get() };
        m_context->PSSetSamplers(0, 1, samps);

        m_context->RSSetState(m_rasterState.Get());
        m_context->Draw(6, 0);

        RenderShapes();
    }
    else if (m_isTiled)
    {
        RenderTiles();
        RenderShapes(); // shapes are independent of imaging path (image-pixel space either way)
    }

    m_swapChain->Present(1, 0);   // vsync-capped
}
