// HiResImageRenderer.cpp
// ---------------------------------------------------------------------------
// Implements the stable C ABI declared in HiResImageRenderer.h as a thin
// shell around the existing, already-correct D3DRenderer C++ engine. Every
// exported function catches everything internally -- a C++ exception must
// never cross a DLL boundary.
// ---------------------------------------------------------------------------
#define IMGENGINE_EXPORTS
#include "HiResImageRenderer.h"
#include "D3DRenderer.h"
#include "ShapeManager.h"

#include <array>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <objbase.h>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// ImgParamMap: opaque generic key/value bag.
// ---------------------------------------------------------------------------
namespace
{
    struct Variant
    {
        enum class Type { Int, Float, String, Ptr } type = Type::Int;
        int64_t      i = 0;
        double       f = 0.0;
        std::wstring s;
        void*        p = nullptr;
    };
}

struct ImgParamMap
{
    std::unordered_map<std::string, Variant> values;
};

ImgParamMap* ImgParamMap_Create(void)
{
    try { return new ImgParamMap(); } catch (...) { return nullptr; }
}

void ImgParamMap_Destroy(ImgParamMap* map)
{
    try { delete map; } catch (...) {}
}

void ImgParamMap_Clear(ImgParamMap* map)
{
    try { if (map) map->values.clear(); } catch (...) {}
}

void ImgParamMap_SetInt(ImgParamMap* map, const char* key, int64_t value)
{
    try
    {
        if (!map || !key) return;
        Variant v; v.type = Variant::Type::Int; v.i = value;
        map->values[key] = std::move(v);
    }
    catch (...) {}
}

void ImgParamMap_SetFloat(ImgParamMap* map, const char* key, double value)
{
    try
    {
        if (!map || !key) return;
        Variant v; v.type = Variant::Type::Float; v.f = value;
        map->values[key] = std::move(v);
    }
    catch (...) {}
}

void ImgParamMap_SetString(ImgParamMap* map, const char* key, const wchar_t* value)
{
    try
    {
        if (!map || !key) return;
        Variant v; v.type = Variant::Type::String; v.s = value ? value : L"";
        map->values[key] = std::move(v);
    }
    catch (...) {}
}

void ImgParamMap_SetPtr(ImgParamMap* map, const char* key, void* value)
{
    try
    {
        if (!map || !key) return;
        Variant v; v.type = Variant::Type::Ptr; v.p = value;
        map->values[key] = std::move(v);
    }
    catch (...) {}
}

int32_t ImgParamMap_TryGetInt(const ImgParamMap* map, const char* key, int64_t* out)
{
    try
    {
        if (!map || !key || !out) return 0;
        auto it = map->values.find(key);
        if (it == map->values.end() || it->second.type != Variant::Type::Int) return 0;
        *out = it->second.i;
        return 1;
    }
    catch (...) { return 0; }
}

int32_t ImgParamMap_TryGetFloat(const ImgParamMap* map, const char* key, double* out)
{
    try
    {
        if (!map || !key || !out) return 0;
        auto it = map->values.find(key);
        if (it == map->values.end() || it->second.type != Variant::Type::Float) return 0;
        *out = it->second.f;
        return 1;
    }
    catch (...) { return 0; }
}

int32_t ImgParamMap_TryGetPtr(const ImgParamMap* map, const char* key, void** out)
{
    try
    {
        if (!map || !key || !out) return 0;
        auto it = map->values.find(key);
        if (it == map->values.end() || it->second.type != Variant::Type::Ptr) return 0;
        *out = it->second.p;
        return 1;
    }
    catch (...) { return 0; }
}

int32_t ImgParamMap_TryGetString(const ImgParamMap* map, const char* key, wchar_t* buf, int32_t bufChars)
{
    try
    {
        if (!map || !key) return -1;
        auto it = map->values.find(key);
        if (it == map->values.end() || it->second.type != Variant::Type::String) return -1;

        const std::wstring& s = it->second.s;
        const int32_t needed = static_cast<int32_t>(s.size()) + 1;
        if (buf && bufChars > 0)
        {
            const int32_t toCopy = (bufChars - 1 < static_cast<int32_t>(s.size()))
                ? (bufChars - 1) : static_cast<int32_t>(s.size());
            if (toCopy > 0) std::memcpy(buf, s.data(), static_cast<size_t>(toCopy) * sizeof(wchar_t));
            buf[toCopy] = L'\0';
        }
        return needed;
    }
    catch (...) { return -1; }
}

namespace
{
    // Convenience: every verb that can fail reports a human-readable reason
    // this way. Safe to call with out == nullptr (SetString is null-safe).
    void SetError(ImgParamMap* out, const wchar_t* message)
    {
        ImgParamMap_SetString(out, IMG_KEY_ERROR_MESSAGE, message);
    }

    // Pulls a string-valued key out of `in` into a std::wstring, using the
    // two-call (size-then-fetch) pattern with a std::vector<wchar_t> scratch
    // buffer (writing into std::wstring::data()'s own terminator slot is
    // undefined behavior in C++17, so a plain buffer is used instead).
    bool TryGetWString(const ImgParamMap* map, const char* key, std::wstring& out)
    {
        const int32_t needed = ImgParamMap_TryGetString(map, key, nullptr, 0);
        if (needed <= 0) return false;
        std::vector<wchar_t> buf(static_cast<size_t>(needed));
        ImgParamMap_TryGetString(map, key, buf.data(), needed);
        out.assign(buf.data());
        return true;
    }
}

namespace
{
    // Each ImgEngine instance owns one of these for its entire lifetime: a
    // single dedicated OS thread that every operation on that instance runs
    // on, via RunSync(). This is what makes two different instances' whole
    // pipelines (Initialize/LoadImage/Resize/ZoomAt/Pan/ResetView/Render)
    // genuinely run in parallel, while each individual ImgEngine_* call
    // still blocks ITS caller until that one operation finishes.
    class WorkerThread
    {
    public:
        WorkerThread() { m_thread = std::thread([this] { Run(); }); }
        ~WorkerThread() { Stop(); }

        WorkerThread(const WorkerThread&) = delete;
        WorkerThread& operator=(const WorkerThread&) = delete;

        // Runs `fn` on the worker thread and blocks the calling thread until
        // it completes, returning fn()'s result. `fn` must catch everything
        // itself: an exception that escapes a thread without being caught on
        // that same thread calls std::terminate -- a try/catch wrapped
        // around the call to RunSync on the CALLING thread would NOT catch
        // an exception thrown inside fn on the worker thread.
        ImgStatus RunSync(std::function<ImgStatus()> fn)
        {
            auto task = std::make_shared<std::packaged_task<ImgStatus()>>(std::move(fn));
            std::future<ImgStatus> future = task->get_future();
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_jobs.push([task] { (*task)(); });
            }
            m_cv.notify_one();
            return future.get();
        }

        // Signals the thread to exit once its job queue drains, then joins
        // it. Safe to call more than once (the destructor also calls this).
        void Stop()
        {
            if (!m_thread.joinable()) return;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stopping = true;
            }
            m_cv.notify_all();
            m_thread.join();
        }

    private:
        void Run()
        {
            // WIC (used by LoadImage) needs COM on this thread -- that work
            // now happens here instead of on whatever thread called into the
            // DLL, so this thread must initialize COM for itself.
            const bool comInit = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

            for (;;)
            {
                std::function<void()> job;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait(lock, [this] { return m_stopping || !m_jobs.empty(); });
                    if (m_jobs.empty() && m_stopping) break;
                    job = std::move(m_jobs.front());
                    m_jobs.pop();
                }
                job();
            }

            if (comInit) CoUninitialize();
        }

        std::thread m_thread;
        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::queue<std::function<void()>> m_jobs;
        bool m_stopping = false;
    };
}

// ---------------------------------------------------------------------------
// ImgEngine: what an ImgEngineHandle ID refers to (see the slot table below).
// ---------------------------------------------------------------------------
struct ImgEngine
{
    WorkerThread   worker;        // every operation on this instance runs here
    D3DRenderer    renderer;
    ShapeManager   shapes;        // this instance's vector-shape annotations (image-pixel space)
    HWND           hwnd = nullptr;
    UINT           width = 1;
    UINT           height = 1;
    bool           initialized = false;
    D3DAdapterInfo currentAdapter;
    int            currentAdapterListIndex = -1; // position in EnumerateAdapters(), for UI pre-selection
    std::wstring   lastImagePath;
};

namespace
{
    // ImgEngineHandle packs a slot index (low kSlotBits bits) and a
    // generation counter (remaining bits) for that slot. The generation is
    // bumped every time a slot is destroyed, so a handle captured before a
    // Destroy is detected as stale if that slot gets reused for a new
    // instance, instead of silently operating on the wrong one.
    constexpr int32_t kSlotBits = 8; // up to 256 slots representable; IMG_MAX_INSTANCES=5 today
    constexpr int32_t kSlotMask = (1 << kSlotBits) - 1;

    struct Slot
    {
        std::unique_ptr<ImgEngine> engine;
        uint32_t generation = 1; // starts at 1 so handle 0 (IMG_INVALID_HANDLE) is never a live instance
        bool occupied = false;
    };

    std::mutex g_tableMutex;
    std::array<Slot, IMG_MAX_INSTANCES> g_slots;

    ImgEngineHandle EncodeHandle(int slotIndex, uint32_t generation)
    {
        return static_cast<ImgEngineHandle>(
            (static_cast<int32_t>(generation) << kSlotBits) | (slotIndex & kSlotMask));
    }
    int DecodeSlotIndex(ImgEngineHandle handle) { return static_cast<int32_t>(handle) & kSlotMask; }
    uint32_t DecodeGeneration(ImgEngineHandle handle)
    {
        return static_cast<uint32_t>(static_cast<int32_t>(handle)) >> kSlotBits;
    }

    // Returns the engine for `handle` if it refers to a currently-live
    // instance, or nullptr if the handle is out of range, refers to an empty
    // slot, or is stale (generation mismatch). Only the table lookup itself
    // is locked -- not held while the returned engine is actually used, so
    // other instances' Create/Destroy/Lookup calls are never blocked by one
    // instance's work.
    ImgEngine* LookupEngine(ImgEngineHandle handle)
    {
        if (handle == IMG_INVALID_HANDLE) return nullptr;
        std::lock_guard<std::mutex> lock(g_tableMutex);
        const int slotIndex = DecodeSlotIndex(handle);
        if (slotIndex < 0 || slotIndex >= IMG_MAX_INSTANCES) return nullptr;
        Slot& slot = g_slots[slotIndex];
        if (!slot.occupied || slot.generation != DecodeGeneration(handle)) return nullptr;
        return slot.engine.get();
    }
}

uint32_t ImgEngine_GetAbiVersion(void)
{
    return 1;
}

int32_t ImgEngine_HasCapability(const char* name)
{
    try
    {
        if (!name) return 0;
        static const char* const kKnown[] =
        {
            "Initialize", "LoadImage", "Resize", "ZoomAt", "Pan", "ResetView",
            "Query", "SwitchAdapter", "EnumerateAdapters", "Render",
            "Shape_SetTool", "Shape_OnMouseDown", "Shape_OnMouseMove", "Shape_OnMouseUp",
            "Shape_OnKeyDown", "Shape_DeleteSelected", "Shape_Clear", "Shape_GetAll",
            "Shape_SetCoordinateOrigin", "Shape_GetLabels", "Shape_SetFilled",
            "key.adapter_is_warp", "key.adapter_index", "key.adapter_list_index",
            "key.buffer_ptr", "key.buffer_size", "key.shape_type", "key.vk_code",
            "key.coord_origin", "key.hover_x", "key.hover_y", "key.cursor_shape_type",
            "key.shape_filled", "key.cursor_is_move",
            "key.is_tiled", "key.tile_count", "key.resident_tile_bytes",
        };
        for (const char* k : kKnown)
            if (std::strcmp(name, k) == 0) return 1;
        return 0;
    }
    catch (...) { return 0; }
}

ImgEngineHandle ImgEngine_Create(const ImgParamMap* /*in*/)
{
    try
    {
        std::lock_guard<std::mutex> lock(g_tableMutex);
        for (int i = 0; i < IMG_MAX_INSTANCES; ++i)
        {
            Slot& slot = g_slots[i];
            if (slot.occupied) continue;
            slot.engine = std::make_unique<ImgEngine>();
            slot.occupied = true;
            return EncodeHandle(i, slot.generation);
        }
        return IMG_INVALID_HANDLE; // IMG_MAX_INSTANCES already live
    }
    catch (...) { return IMG_INVALID_HANDLE; }
}

void ImgEngine_Destroy(ImgEngineHandle handle)
{
    try
    {
        if (handle == IMG_INVALID_HANDLE) return;

        std::unique_ptr<ImgEngine> owned;
        {
            std::lock_guard<std::mutex> lock(g_tableMutex);
            const int slotIndex = DecodeSlotIndex(handle);
            if (slotIndex < 0 || slotIndex >= IMG_MAX_INSTANCES) return;
            Slot& slot = g_slots[slotIndex];
            if (!slot.occupied || slot.generation != DecodeGeneration(handle)) return; // invalid/stale: no-op
            owned = std::move(slot.engine);
            slot.occupied = false;
            ++slot.generation; // any other copies of this handle are now stale
        }

        // Tear down the D3D device on the thread that created it, then stop
        // that thread -- both outside the table lock, so other instances'
        // Create/Destroy/Lookup calls are never blocked by this.
        if (owned)
        {
            ImgEngine* rawEngine = owned.get();
            owned->worker.RunSync([rawEngine]() -> ImgStatus
            {
                try { rawEngine->renderer.Shutdown(); } catch (...) {}
                return IMG_OK;
            });
            owned->worker.Stop();
        }
        // `owned` (and its now-stopped worker thread) is freed here.
    }
    catch (...) {}
}

ImgStatus ImgEngine_Initialize(ImgEngineHandle handle, const ImgParamMap* in, ImgParamMap* out)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, in, out]() -> ImgStatus
    {
        try
        {
            if (!in) { SetError(out, L"Initialize requires hwnd, width, height."); return IMG_ERR_INVALID_ARG; }

            void* hwndPtr = nullptr;
            int64_t w = 0, h = 0;
            if (!ImgParamMap_TryGetPtr(in, IMG_KEY_HWND, &hwndPtr) ||
                !ImgParamMap_TryGetInt(in, IMG_KEY_WIDTH, &w) ||
                !ImgParamMap_TryGetInt(in, IMG_KEY_HEIGHT, &h))
            {
                SetError(out, L"Initialize requires hwnd, width, height.");
                return IMG_ERR_INVALID_ARG;
            }

            engine->hwnd   = static_cast<HWND>(hwndPtr);
            engine->width  = w > 0 ? static_cast<UINT>(w) : 1;
            engine->height = h > 0 ? static_cast<UINT>(h) : 1;

            int64_t isWarp = 0, adapterIndex = 0;
            const bool hasWarp  = ImgParamMap_TryGetInt(in, IMG_KEY_ADAPTER_IS_WARP, &isWarp) != 0;
            const bool hasIndex = ImgParamMap_TryGetInt(in, IMG_KEY_ADAPTER_INDEX, &adapterIndex) != 0;

            bool ok;
            if (hasWarp || hasIndex)
            {
                D3DAdapterInfo requested;
                requested.isWarp = isWarp != 0;
                requested.adapterIndex = static_cast<UINT>(adapterIndex);

                ok = engine->renderer.InitializeWithAdapter(engine->hwnd, engine->width, engine->height, requested);
                if (ok)
                {
                    engine->currentAdapter = requested;
                    int64_t listIndex = -1;
                    engine->currentAdapterListIndex = ImgParamMap_TryGetInt(in, IMG_KEY_ADAPTER_LIST_INDEX, &listIndex)
                        ? static_cast<int>(listIndex) : -1;
                }
            }
            else
            {
                ok = engine->renderer.Initialize(engine->hwnd, engine->width, engine->height);
                if (ok)
                {
                    // D3D11CreateDeviceAndSwapChain(pAdapter=nullptr, HARDWARE, ...) always
                    // resolves to DXGI's default adapter, which EnumAdapters1 enumerates at
                    // index 0; the WARP fallback is the synthetic last entry.
                    const auto adapters = D3DRenderer::EnumerateAdapters();
                    engine->currentAdapterListIndex = engine->renderer.IsHardwareDevice()
                        ? 0 : static_cast<int>(adapters.size()) - 1;
                    if (engine->currentAdapterListIndex >= 0 &&
                        engine->currentAdapterListIndex < static_cast<int>(adapters.size()))
                        engine->currentAdapter = adapters[engine->currentAdapterListIndex];
                }
            }

            engine->initialized = ok;
            if (!ok)
            {
                SetError(out, L"Failed to initialize Direct3D 11 (no HARDWARE or WARP device).");
                return IMG_ERR_DEVICE_INIT_FAILED;
            }
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

namespace
{
    ImgStatus MapLoadResultToStatus(D3DRenderer::D3DLoadResult result, ImgParamMap* out)
    {
        switch (result)
        {
        case D3DRenderer::D3DLoadResult::Ok:
            return IMG_OK;
        case D3DRenderer::D3DLoadResult::NoDevice:
            SetError(out, L"Engine not initialized.");
            return IMG_ERR_NOT_INITIALIZED;
        case D3DRenderer::D3DLoadResult::InvalidArgument:
            SetError(out, L"Invalid image source (null/empty path or buffer).");
            return IMG_ERR_INVALID_ARG;
        case D3DRenderer::D3DLoadResult::ImageTooLarge:
            SetError(out, L"Image exceeds the 16384 px single-texture limit.");
            return IMG_ERR_IMAGE_TOO_LARGE;
        case D3DRenderer::D3DLoadResult::OutOfMemory:
            SetError(out, L"Out of host memory while decoding the image.");
            return IMG_ERR_OUT_OF_MEMORY;
        case D3DRenderer::D3DLoadResult::GpuUploadFailed:
            SetError(out, L"GPU rejected the decoded image (insufficient VRAM?).");
            return IMG_ERR_GPU_UPLOAD_FAILED;
        case D3DRenderer::D3DLoadResult::DecodeFailed:
        default:
            SetError(out, L"Could not load image (unsupported format or decode error).");
            return IMG_ERR_DECODE_FAILED;
        }
    }
}

namespace
{
    // Screen-pixel hit-test/closing tolerance, converted to image-pixel space
    // (which shrinks as you zoom in) before being handed to ShapeManager --
    // ShapeManager itself works purely in image-pixel space and has no
    // notion of zoom.
    constexpr float kShapeHitToleranceScreenPx = 6.0f;

    float ShapeHitToleranceImagePx(ImgEngine* engine)
    {
        const float zoom = engine->renderer.GetZoom();
        return zoom > 1e-6f ? (kShapeHitToleranceScreenPx / zoom) : kShapeHitToleranceScreenPx;
    }

    // Pushes the current shape list to the renderer so the next Render()
    // picks up whatever just changed. Called at the end of every Shape_*
    // verb that can mutate shape state.
    void SyncShapeRenderData(ImgEngine* engine)
    {
        engine->renderer.SetShapeRenderData(engine->shapes.BuildRenderData());
    }
}

ImgStatus ImgEngine_LoadImage(ImgEngineHandle handle, const ImgParamMap* in, ImgParamMap* out)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, in, out]() -> ImgStatus
    {
        try
        {
            if (!engine->initialized) { SetError(out, L"Engine not initialized."); return IMG_ERR_NOT_INITIALIZED; }
            if (!in)
            {
                SetError(out, L"LoadImage requires either 'path' or 'buffer_ptr'+'buffer_size'.");
                return IMG_ERR_INVALID_ARG;
            }

            void* bufferPtr = nullptr;
            int64_t bufferSize = 0;
            const bool hasBuffer = ImgParamMap_TryGetPtr(in, IMG_KEY_BUFFER_PTR, &bufferPtr) &&
                                    ImgParamMap_TryGetInt(in, IMG_KEY_BUFFER_SIZE, &bufferSize) &&
                                    bufferPtr != nullptr && bufferSize > 0;

            D3DRenderer::D3DLoadResult result;
            if (hasBuffer)
            {
                result = engine->renderer.LoadImageFromMemory(bufferPtr, static_cast<size_t>(bufferSize));
                // A buffer isn't retained, so it can't be replayed after a
                // hardware switch -- forget any earlier path-based source so
                // SwitchAdapter doesn't reload the WRONG (stale) image.
                engine->lastImagePath.clear();
            }
            else
            {
                std::wstring path;
                if (!TryGetWString(in, IMG_KEY_PATH, path))
                {
                    SetError(out, L"LoadImage requires either 'path' or 'buffer_ptr'+'buffer_size'.");
                    return IMG_ERR_INVALID_ARG;
                }
                result = engine->renderer.LoadImageFromFile(path.c_str());
                if (result == D3DRenderer::D3DLoadResult::Ok) engine->lastImagePath = path;
            }

            if (result == D3DRenderer::D3DLoadResult::Ok)
            {
                ImgParamMap_SetInt(out, IMG_KEY_WIDTH, engine->renderer.GetImageWidth());
                ImgParamMap_SetInt(out, IMG_KEY_HEIGHT, engine->renderer.GetImageHeight());
                ImgParamMap_SetFloat(out, IMG_KEY_LOAD_MS, engine->renderer.GetLastLoadTimeMs());
                ImgParamMap_SetInt(out, IMG_KEY_FILE_SIZE_BYTES, static_cast<int64_t>(engine->renderer.GetLastFileSizeBytes()));

                // Existing shape coordinates are meaningless against whatever
                // new image content just replaced the old one.
                engine->shapes.Clear();
                engine->shapes.SetImageBounds(static_cast<float>(engine->renderer.GetImageWidth()),
                                               static_cast<float>(engine->renderer.GetImageHeight()));
                SyncShapeRenderData(engine);
            }
            return MapLoadResultToStatus(result, out);
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Resize(ImgEngineHandle handle, const ImgParamMap* in, ImgParamMap* /*out*/)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, in]() -> ImgStatus
    {
        try
        {
            if (!engine->initialized) return IMG_ERR_NOT_INITIALIZED;

            int64_t w = 0, h = 0;
            if (!in || !ImgParamMap_TryGetInt(in, IMG_KEY_WIDTH, &w) || !ImgParamMap_TryGetInt(in, IMG_KEY_HEIGHT, &h))
                return IMG_ERR_INVALID_ARG;

            engine->width  = w > 0 ? static_cast<UINT>(w) : 1;
            engine->height = h > 0 ? static_cast<UINT>(h) : 1;
            engine->renderer.Resize(engine->width, engine->height);
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_ZoomAt(ImgEngineHandle handle, const ImgParamMap* in, ImgParamMap* /*out*/)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, in]() -> ImgStatus
    {
        try
        {
            if (!engine->initialized) return IMG_ERR_NOT_INITIALIZED;

            double factor = 1.0;
            int64_t x = 0, y = 0;
            if (!in ||
                !ImgParamMap_TryGetFloat(in, IMG_KEY_FACTOR, &factor) ||
                !ImgParamMap_TryGetInt(in, IMG_KEY_X, &x) ||
                !ImgParamMap_TryGetInt(in, IMG_KEY_Y, &y))
                return IMG_ERR_INVALID_ARG;

            engine->renderer.ZoomAt(static_cast<float>(factor), static_cast<int>(x), static_cast<int>(y));
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Pan(ImgEngineHandle handle, const ImgParamMap* in, ImgParamMap* /*out*/)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, in]() -> ImgStatus
    {
        try
        {
            if (!engine->initialized) return IMG_ERR_NOT_INITIALIZED;

            double dx = 0, dy = 0;
            if (!in || !ImgParamMap_TryGetFloat(in, IMG_KEY_DX, &dx) || !ImgParamMap_TryGetFloat(in, IMG_KEY_DY, &dy))
                return IMG_ERR_INVALID_ARG;

            engine->renderer.Pan(static_cast<float>(dx), static_cast<float>(dy));
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_ResetView(ImgEngineHandle handle, const ImgParamMap* /*in*/, ImgParamMap* /*out*/)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine]() -> ImgStatus
    {
        try
        {
            if (!engine->initialized) return IMG_ERR_NOT_INITIALIZED;
            engine->renderer.ResetView();
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Query(ImgEngineHandle handle, const ImgParamMap* /*in*/, ImgParamMap* out)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, out]() -> ImgStatus
    {
        try
        {
            if (!out) return IMG_OK;

            if (!engine->initialized)
            {
                ImgParamMap_SetInt(out, IMG_KEY_HAS_IMAGE, 0);
                return IMG_ERR_NOT_INITIALIZED;
            }

            ImgParamMap_SetFloat(out, IMG_KEY_ZOOM, engine->renderer.GetZoom());
            ImgParamMap_SetInt(out, IMG_KEY_HAS_IMAGE, engine->renderer.HasImage() ? 1 : 0);
            ImgParamMap_SetInt(out, IMG_KEY_IMAGE_WIDTH, engine->renderer.GetImageWidth());
            ImgParamMap_SetInt(out, IMG_KEY_IMAGE_HEIGHT, engine->renderer.GetImageHeight());
            ImgParamMap_SetInt(out, IMG_KEY_IS_HARDWARE, engine->renderer.IsHardwareDevice() ? 1 : 0);
            ImgParamMap_SetString(out, IMG_KEY_DEVICE_DESCRIPTION, engine->renderer.GetDeviceDescription());
            ImgParamMap_SetFloat(out, IMG_KEY_LAST_LOAD_MS, engine->renderer.GetLastLoadTimeMs());
            ImgParamMap_SetInt(out, IMG_KEY_LAST_FILE_SIZE_BYTES, static_cast<int64_t>(engine->renderer.GetLastFileSizeBytes()));
            ImgParamMap_SetInt(out, IMG_KEY_CURRENT_ADAPTER_IS_WARP, engine->currentAdapter.isWarp ? 1 : 0);
            ImgParamMap_SetInt(out, IMG_KEY_CURRENT_ADAPTER_INDEX, engine->currentAdapterListIndex);
            ImgParamMap_SetInt(out, IMG_KEY_IS_TILED, engine->renderer.IsTiled() ? 1 : 0);
            ImgParamMap_SetInt(out, IMG_KEY_TILE_COUNT, static_cast<int64_t>(engine->renderer.GetTileCount()));
            ImgParamMap_SetInt(out, IMG_KEY_RESIDENT_TILE_BYTES, static_cast<int64_t>(engine->renderer.GetResidentTileBytes()));
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_SwitchAdapter(ImgEngineHandle handle, const ImgParamMap* in, ImgParamMap* out)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, in, out]() -> ImgStatus
    {
        try
        {
            if (!engine->hwnd) { SetError(out, L"Engine not initialized."); return IMG_ERR_NOT_INITIALIZED; }
            if (!in) return IMG_ERR_INVALID_ARG;

            int64_t isWarp = 0, adapterIndex = 0, listIndex = -1;
            ImgParamMap_TryGetInt(in, IMG_KEY_ADAPTER_IS_WARP, &isWarp);
            ImgParamMap_TryGetInt(in, IMG_KEY_ADAPTER_INDEX, &adapterIndex);
            ImgParamMap_TryGetInt(in, IMG_KEY_ADAPTER_LIST_INDEX, &listIndex);

            D3DAdapterInfo requested;
            requested.isWarp = isWarp != 0;
            requested.adapterIndex = static_cast<UINT>(adapterIndex);

            const D3DAdapterInfo previousAdapter   = engine->currentAdapter;
            const int            previousListIndex = engine->currentAdapterListIndex;

            engine->renderer.Shutdown();
            engine->initialized = false;

            if (engine->renderer.InitializeWithAdapter(engine->hwnd, engine->width, engine->height, requested))
            {
                engine->initialized = true;
                engine->currentAdapter = requested;
                engine->currentAdapterListIndex = static_cast<int>(listIndex);

                bool reloaded = false;
                if (!engine->lastImagePath.empty())
                    reloaded = engine->renderer.LoadImageFromFile(engine->lastImagePath.c_str())
                        == D3DRenderer::D3DLoadResult::Ok;

                ImgParamMap_SetInt(out, IMG_KEY_RELOADED_IMAGE, reloaded ? 1 : 0);
                return IMG_OK;
            }

            // Requested adapter failed -- restore the previous one (best effort)
            // so the engine is left usable rather than permanently broken.
            SetError(out, L"Failed to initialize Direct3D 11 on the selected hardware.");
            if (engine->renderer.InitializeWithAdapter(engine->hwnd, engine->width, engine->height, previousAdapter))
            {
                engine->initialized = true;
                engine->currentAdapter = previousAdapter;
                engine->currentAdapterListIndex = previousListIndex;
                if (!engine->lastImagePath.empty())
                    engine->renderer.LoadImageFromFile(engine->lastImagePath.c_str());
            }
            ImgParamMap_SetInt(out, IMG_KEY_RELOADED_IMAGE, 0);
            return IMG_ERR_DEVICE_INIT_FAILED;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_EnumerateAdapters(const ImgParamMap* /*in*/, ImgParamMap* out)
{
    try
    {
        if (!out) return IMG_OK;

        const auto adapters = D3DRenderer::EnumerateAdapters();
        ImgParamMap_SetInt(out, IMG_KEY_ADAPTER_COUNT, static_cast<int64_t>(adapters.size()));

        char key[64];
        for (size_t i = 0; i < adapters.size(); ++i)
        {
            sprintf_s(key, IMG_KEY_ADAPTER_NAME_FMT, static_cast<int>(i));
            ImgParamMap_SetString(out, key, adapters[i].description.c_str());
            sprintf_s(key, IMG_KEY_ADAPTER_IS_WARP_FMT, static_cast<int>(i));
            ImgParamMap_SetInt(out, key, adapters[i].isWarp ? 1 : 0);
            sprintf_s(key, IMG_KEY_ADAPTER_INDEX_FMT, static_cast<int>(i));
            ImgParamMap_SetInt(out, key, static_cast<int64_t>(adapters[i].adapterIndex));
        }
        return IMG_OK;
    }
    catch (...) { return IMG_ERR_UNKNOWN; }
}

ImgStatus ImgEngine_Shape_SetTool(ImgEngineHandle handle, const ImgParamMap* in, ImgParamMap* /*out*/)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, in]() -> ImgStatus
    {
        try
        {
            int64_t shapeType = IMG_SHAPE_NONE;
            if (!in || !ImgParamMap_TryGetInt(in, IMG_KEY_SHAPE_TYPE, &shapeType))
                return IMG_ERR_INVALID_ARG;

            engine->shapes.SetTool(static_cast<int>(shapeType));
            SyncShapeRenderData(engine);
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Shape_OnMouseDown(ImgEngineHandle handle, const ImgParamMap* in, ImgParamMap* out)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, in, out]() -> ImgStatus
    {
        try
        {
            if (!engine->initialized) return IMG_ERR_NOT_INITIALIZED;

            int64_t x = 0, y = 0;
            if (!in || !ImgParamMap_TryGetInt(in, IMG_KEY_X, &x) || !ImgParamMap_TryGetInt(in, IMG_KEY_Y, &y))
                return IMG_ERR_INVALID_ARG;

            const PointF imgPt = engine->renderer.ScreenToImage(static_cast<int>(x), static_cast<int>(y));
            int shapeId = -1;
            const bool handled = engine->shapes.OnMouseDown(imgPt.x, imgPt.y, ShapeHitToleranceImagePx(engine), &shapeId);
            SyncShapeRenderData(engine);

            if (out)
            {
                ImgParamMap_SetInt(out, IMG_KEY_HANDLED, handled ? 1 : 0);
                ImgParamMap_SetInt(out, IMG_KEY_SHAPE_ID, shapeId);
                ImgParamMap_SetInt(out, IMG_KEY_CURRENT_TOOL, engine->shapes.GetTool());
            }
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Shape_OnMouseMove(ImgEngineHandle handle, const ImgParamMap* in, ImgParamMap* out)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, in, out]() -> ImgStatus
    {
        try
        {
            if (!engine->initialized) return IMG_ERR_NOT_INITIALIZED;

            int64_t x = 0, y = 0;
            if (!in || !ImgParamMap_TryGetInt(in, IMG_KEY_X, &x) || !ImgParamMap_TryGetInt(in, IMG_KEY_Y, &y))
                return IMG_ERR_INVALID_ARG;

            const PointF imgPt = engine->renderer.ScreenToImage(static_cast<int>(x), static_cast<int>(y));
            bool changed = false;
            engine->shapes.OnMouseMove(imgPt.x, imgPt.y, &changed);
            if (changed) SyncShapeRenderData(engine);

            if (out)
            {
                ImgParamMap_SetInt(out, IMG_KEY_CHANGED, changed ? 1 : 0);

                // Clamp the readout to the image bounds too -- same idea as
                // shape geometry: a hover position past the image edge
                // (e.g. over the letterboxed margin) is more useful pinned
                // to the nearest valid pixel than showing a raw extrapolated
                // value that can be far outside [0,width]x[0,height].
                PointF clampedPt = imgPt;
                const float imgW = static_cast<float>(engine->renderer.GetImageWidth());
                const float imgH = static_cast<float>(engine->renderer.GetImageHeight());
                if (imgW > 0.0f) clampedPt.x = clampedPt.x < 0.0f ? 0.0f : (clampedPt.x > imgW ? imgW : clampedPt.x);
                if (imgH > 0.0f) clampedPt.y = clampedPt.y < 0.0f ? 0.0f : (clampedPt.y > imgH ? imgH : clampedPt.y);

                const PointF displayPt = engine->shapes.ToDisplayCoords(clampedPt);
                ImgParamMap_SetFloat(out, IMG_KEY_HOVER_X, displayPt.x);
                ImgParamMap_SetFloat(out, IMG_KEY_HOVER_Y, displayPt.y);

                bool cursorIsMove = false;
                const int cursorShapeType = engine->shapes.GetCursorShapeType(imgPt.x, imgPt.y, ShapeHitToleranceImagePx(engine), &cursorIsMove);
                ImgParamMap_SetInt(out, IMG_KEY_CURSOR_SHAPE_TYPE, cursorShapeType);
                ImgParamMap_SetInt(out, IMG_KEY_CURSOR_IS_MOVE, cursorIsMove ? 1 : 0);
            }
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Shape_OnMouseUp(ImgEngineHandle handle, const ImgParamMap* in, ImgParamMap* out)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, in, out]() -> ImgStatus
    {
        try
        {
            if (!engine->initialized) return IMG_ERR_NOT_INITIALIZED;

            int64_t x = 0, y = 0;
            if (!in || !ImgParamMap_TryGetInt(in, IMG_KEY_X, &x) || !ImgParamMap_TryGetInt(in, IMG_KEY_Y, &y))
                return IMG_ERR_INVALID_ARG;

            const PointF imgPt = engine->renderer.ScreenToImage(static_cast<int>(x), static_cast<int>(y));
            bool changed = false;
            engine->shapes.OnMouseUp(imgPt.x, imgPt.y, &changed);
            if (changed) SyncShapeRenderData(engine);

            if (out)
            {
                ImgParamMap_SetInt(out, IMG_KEY_CHANGED, changed ? 1 : 0);
                ImgParamMap_SetInt(out, IMG_KEY_CURRENT_TOOL, engine->shapes.GetTool());
            }
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Shape_OnKeyDown(ImgEngineHandle handle, const ImgParamMap* in, ImgParamMap* out)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, in, out]() -> ImgStatus
    {
        try
        {
            int64_t vk = 0;
            if (!in || !ImgParamMap_TryGetInt(in, IMG_KEY_VK_CODE, &vk))
                return IMG_ERR_INVALID_ARG;

            const bool changed = engine->shapes.OnKeyDown(static_cast<int>(vk));
            if (changed) SyncShapeRenderData(engine);

            if (out) ImgParamMap_SetInt(out, IMG_KEY_CHANGED, changed ? 1 : 0);
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Shape_DeleteSelected(ImgEngineHandle handle, const ImgParamMap* /*in*/, ImgParamMap* out)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, out]() -> ImgStatus
    {
        try
        {
            const bool changed = engine->shapes.DeleteSelected();
            if (changed) SyncShapeRenderData(engine);

            if (out) ImgParamMap_SetInt(out, IMG_KEY_CHANGED, changed ? 1 : 0);
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Shape_Clear(ImgEngineHandle handle, const ImgParamMap* /*in*/, ImgParamMap* /*out*/)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine]() -> ImgStatus
    {
        try
        {
            engine->shapes.Clear();
            SyncShapeRenderData(engine);
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Shape_GetAll(ImgEngineHandle handle, const ImgParamMap* /*in*/, ImgParamMap* out)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, out]() -> ImgStatus
    {
        try
        {
            if (!out) return IMG_OK;

            const auto& shapesList = engine->shapes.GetShapes();
            ImgParamMap_SetInt(out, IMG_KEY_SHAPE_COUNT, static_cast<int64_t>(shapesList.size()));
            ImgParamMap_SetInt(out, IMG_KEY_SELECTED_SHAPE_ID, engine->shapes.GetSelectedId());

            char key[64];
            for (size_t i = 0; i < shapesList.size(); ++i)
            {
                const Shape& s = shapesList[i];
                sprintf_s(key, IMG_KEY_SHAPE_ID_FMT, static_cast<int>(i));
                ImgParamMap_SetInt(out, key, s.id);
                sprintf_s(key, IMG_KEY_SHAPE_TYPE_FMT, static_cast<int>(i));
                ImgParamMap_SetInt(out, key, static_cast<int64_t>(s.type));
                sprintf_s(key, IMG_KEY_SHAPE_SELECTED_FMT, static_cast<int>(i));
                ImgParamMap_SetInt(out, key, s.selected ? 1 : 0);
                sprintf_s(key, IMG_KEY_SHAPE_POINT_COUNT_FMT, static_cast<int>(i));
                ImgParamMap_SetInt(out, key, static_cast<int64_t>(s.points.size()));

                for (size_t j = 0; j < s.points.size(); ++j)
                {
                    sprintf_s(key, IMG_KEY_SHAPE_POINT_X_FMT, static_cast<int>(i), static_cast<int>(j));
                    ImgParamMap_SetFloat(out, key, s.points[j].x);
                    sprintf_s(key, IMG_KEY_SHAPE_POINT_Y_FMT, static_cast<int>(i), static_cast<int>(j));
                    ImgParamMap_SetFloat(out, key, s.points[j].y);
                }
            }
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Shape_SetCoordinateOrigin(ImgEngineHandle handle, const ImgParamMap* in, ImgParamMap* /*out*/)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, in]() -> ImgStatus
    {
        try
        {
            int64_t origin = IMG_COORD_ORIGIN_TOP_LEFT;
            if (!in || !ImgParamMap_TryGetInt(in, IMG_KEY_COORD_ORIGIN, &origin))
                return IMG_ERR_INVALID_ARG;

            engine->shapes.SetCoordinateOrigin(static_cast<int>(origin));
            SyncShapeRenderData(engine); // labels' cached text (if any) depends on the origin too
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Shape_GetLabels(ImgEngineHandle handle, const ImgParamMap* /*in*/, ImgParamMap* out)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, out]() -> ImgStatus
    {
        try
        {
            if (!out) return IMG_OK;

            const auto labels = engine->shapes.BuildLabels();
            ImgParamMap_SetInt(out, IMG_KEY_LABEL_COUNT, static_cast<int64_t>(labels.size()));

            // Nudge each label a few screen pixels above its image-space
            // anchor so it doesn't overlap the shape outline itself; done
            // here (after ImageToScreen) rather than in image-space so the
            // offset stays a constant SCREEN distance regardless of zoom.
            constexpr float kLabelScreenNudgeUpPx = 14.0f;

            char key[64];
            for (size_t i = 0; i < labels.size(); ++i)
            {
                PointF screenPt = engine->renderer.ImageToScreen(labels[i].anchor.x, labels[i].anchor.y);
                screenPt.y -= kLabelScreenNudgeUpPx;

                sprintf_s(key, IMG_KEY_LABEL_X_FMT, static_cast<int>(i));
                ImgParamMap_SetInt(out, key, static_cast<int64_t>(screenPt.x));
                sprintf_s(key, IMG_KEY_LABEL_Y_FMT, static_cast<int>(i));
                ImgParamMap_SetInt(out, key, static_cast<int64_t>(screenPt.y));
                sprintf_s(key, IMG_KEY_LABEL_TEXT_FMT, static_cast<int>(i));
                ImgParamMap_SetString(out, key, labels[i].text.c_str());
            }
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Shape_SetFilled(ImgEngineHandle handle, const ImgParamMap* in, ImgParamMap* /*out*/)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine, in]() -> ImgStatus
    {
        try
        {
            int64_t filled = 0;
            if (!in || !ImgParamMap_TryGetInt(in, IMG_KEY_SHAPE_FILLED, &filled))
                return IMG_ERR_INVALID_ARG;

            engine->shapes.SetFilled(filled != 0);
            SyncShapeRenderData(engine);
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}

ImgStatus ImgEngine_Render(ImgEngineHandle handle)
{
    ImgEngine* engine = LookupEngine(handle);
    if (!engine) return IMG_ERR_INVALID_HANDLE;

    return engine->worker.RunSync([engine]() -> ImgStatus
    {
        try
        {
            if (!engine->initialized) return IMG_ERR_NOT_INITIALIZED;
            engine->renderer.Render();
            return IMG_OK;
        }
        catch (...) { return IMG_ERR_UNKNOWN; }
    });
}
