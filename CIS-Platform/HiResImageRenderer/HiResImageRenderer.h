// HiResImageRenderer.h
// ---------------------------------------------------------------------------
// Stable C ABI for the high-resolution image load-and-render engine
// (currently implemented on Direct3D 11 / WIC; see D3DRenderer.h).
//
// DESIGN RULE (read before adding anything): once shipped, an exported
// function's SIGNATURE NEVER CHANGES, and an exported function's BEHAVIOR
// for inputs it already accepted never changes either.
//   - Need a new parameter for an existing operation? Add a new key to the
//     ImgParamMap passed into that function. Old callers that don't set the
//     key get today's default behavior; nothing about the export table or
//     calling convention changes.
//   - Need a genuinely new operation? Export a brand-new ImgEngine_* (or
//     ImgParamMap_*) function. Never repurpose or change the meaning of an
//     existing one.
// This is what makes an old EXE keep working against a newer DLL, and a
// newer EXE keep working against an older DLL (modulo ImgEngine_HasCapability
// checks for brand-new functions/keys) -- indefinitely, not just today.
//
// Why a plain C ABI (extern "C", opaque handles, no STL/MFC types in any
// signature): a C++ class exported via __declspec(dllexport) silently breaks
// under vtable layout drift or STL ABI drift, even within "the same"
// compiler/toolset across updates, or under mixed Debug/Release CRT linkage.
// A pure C boundary sidesteps all of that permanently -- which is the only
// way the "signature never changes" promise above actually holds over time.
//
// Threading: calls on a given handle should come from one logical "owner"
// at a time (e.g. one UI control) -- concurrently calling ImgEngine_Destroy
// and some other operation on the SAME handle from two different threads is
// not supported, same as before. Within that rule, two DIFFERENT handles are
// fully independent: each instance runs its entire pipeline (Initialize,
// LoadImage, Resize, ZoomAt, Pan, ResetView, Render) on its own dedicated
// worker thread inside the DLL, so two instances' GPU work can genuinely
// overlap in wall-clock time. Every ImgEngine_* call still blocks its caller
// until that specific operation finishes -- this is an internal threading
// change, not a change to the synchronous calling convention.
//
// Exceptions never cross this boundary: every exported function catches
// everything internally and reports failure via the ImgStatus return value.
// ---------------------------------------------------------------------------
#pragma once

#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef IMGENGINE_EXPORTS
#define IMGENGINE_API __declspec(dllexport)
#else
#define IMGENGINE_API __declspec(dllimport)
#endif

// ---------------------------------------------------------------------------
// Status codes. Additive only -- never renumber or remove an existing value.
// ---------------------------------------------------------------------------
typedef int32_t ImgStatus;
#define IMG_OK                      0
#define IMG_ERR_INVALID_HANDLE      1
#define IMG_ERR_INVALID_ARG         2
#define IMG_ERR_NOT_INITIALIZED     3
#define IMG_ERR_DEVICE_INIT_FAILED  4
#define IMG_ERR_IMAGE_TOO_LARGE     5   // no longer returned since the tiled-streaming path (Phase 2)
                                         // was added -- kept defined per the additive-only rule above.
#define IMG_ERR_DECODE_FAILED       6
#define IMG_ERR_UNKNOWN             7
#define IMG_ERR_OUT_OF_MEMORY       8
#define IMG_ERR_GPU_UPLOAD_FAILED   9

// Maximum number of engine instances the DLL will manage concurrently.
// ImgEngine_Create() returns IMG_INVALID_HANDLE once this many are already
// live. Safe to raise in a future version (callers should not assume this
// is the upper bound forever, only that it's at least this many today).
#define IMG_MAX_INSTANCES 5

// ---------------------------------------------------------------------------
// Opaque parameter map: a generic, extensible key/value bag used as the
// in/out argument of every parameterized ImgEngine_* call. Its internal
// representation lives entirely inside the DLL and may change freely --
// callers only ever hold an opaque pointer and go through these accessors.
//
// Key-naming contract: key names are snake_case and are part of the
// compatibility contract exactly like function names -- once shipped, a
// key's name+type pairing is never repurposed, only added to.
//
// List convention: any function that needs to return a variable-length list
// encodes it in an out map as "<prefix>_count" (int) plus, for i in
// [0, count), "<prefix>_<i>_<field>" per field. This is the one shared
// convention beyond the raw map primitive; reuse it for any future
// list-returning function instead of inventing a new scheme.
// ---------------------------------------------------------------------------
typedef struct ImgParamMap ImgParamMap;

IMGENGINE_API ImgParamMap* ImgParamMap_Create(void);
IMGENGINE_API void    ImgParamMap_Destroy(ImgParamMap* map);
IMGENGINE_API void    ImgParamMap_Clear(ImgParamMap* map);

IMGENGINE_API void    ImgParamMap_SetInt(ImgParamMap* map, const char* key, int64_t value);
IMGENGINE_API void    ImgParamMap_SetFloat(ImgParamMap* map, const char* key, double value);
IMGENGINE_API void    ImgParamMap_SetString(ImgParamMap* map, const char* key, const wchar_t* value);
IMGENGINE_API void    ImgParamMap_SetPtr(ImgParamMap* map, const char* key, void* value);

// Each TryGet returns 1 and writes *out if `key` exists with the matching
// type, 0 (and leaves *out untouched) if the key is missing or has a
// different type.
//
// IMPORTANT: there is no separate "unknown/misspelled key" error. A typo'd
// key (e.g. "widht" instead of "width") and a key that was simply never set
// are indistinguishable to the map -- both return 0 (or a negative value
// from TryGetString), because the map only knows what keys ARE present, not
// what keys a given call SHOULD have received. Always check the return
// value; never assume *out was written just because you called TryGet*.
// Use the IMG_KEY_* constants below (instead of hand-typed string literals)
// to turn most key typos into a compile error instead of a silent miss.
IMGENGINE_API int32_t ImgParamMap_TryGetInt(const ImgParamMap* map, const char* key, int64_t* out);
IMGENGINE_API int32_t ImgParamMap_TryGetFloat(const ImgParamMap* map, const char* key, double* out);
IMGENGINE_API int32_t ImgParamMap_TryGetPtr(const ImgParamMap* map, const char* key, void** out);

// GetWindowText-style: returns the required buffer size in wchar_t units
// INCLUDING the null terminator on success (truncating into buf/bufChars if
// bufChars is smaller), or a negative value if the key is missing or has a
// different type. Pass buf=NULL/bufChars=0 to just query the required size.
IMGENGINE_API int32_t ImgParamMap_TryGetString(const ImgParamMap* map, const char* key,
                                                wchar_t* buf, int32_t bufChars);

// ---------------------------------------------------------------------------
// Well-known key names, as compile-time-checked constants instead of
// hand-typed string literals. These are pure convenience: the value crossing
// the DLL boundary is still the plain string, so nothing about the ABI
// changes if you use a literal instead -- but a misspelled IMG_KEY_WIDHT
// fails to compile, where a misspelled "widht" would just silently miss at
// runtime (see the TryGet note above). Additive only, exactly like
// everything else in this header: a name is never repurposed, only added to.
// ---------------------------------------------------------------------------
#define IMG_KEY_HWND                     "hwnd"                   // ptr,    Initialize in
#define IMG_KEY_WIDTH                    "width"                  // int,    Initialize/Resize in, LoadImage/Query out
#define IMG_KEY_HEIGHT                   "height"                 // int,    Initialize/Resize in, LoadImage/Query out
#define IMG_KEY_ADAPTER_IS_WARP          "adapter_is_warp"        // int,    Initialize(optional)/SwitchAdapter in, Query out
#define IMG_KEY_ADAPTER_INDEX            "adapter_index"          // int,    Initialize(optional)/SwitchAdapter in
#define IMG_KEY_ADAPTER_LIST_INDEX       "adapter_list_index"     // int,    Initialize(optional)/SwitchAdapter in
#define IMG_KEY_ERROR_MESSAGE            "error_message"          // string, out (any verb, on failure)
#define IMG_KEY_PATH                     "path"                   // string, LoadImage in (alternative to buffer_ptr/buffer_size)
#define IMG_KEY_BUFFER_PTR               "buffer_ptr"             // ptr,    LoadImage in (alternative to path; takes precedence if both given)
#define IMG_KEY_BUFFER_SIZE              "buffer_size"            // int,    LoadImage in (required with buffer_ptr, bytes)
#define IMG_KEY_LOAD_MS                  "load_ms"                // float,  LoadImage out
#define IMG_KEY_FILE_SIZE_BYTES          "file_size_bytes"        // int,    LoadImage out
#define IMG_KEY_FACTOR                   "factor"                 // float,  ZoomAt in
#define IMG_KEY_X                        "x"                      // int,    ZoomAt in
#define IMG_KEY_Y                        "y"                      // int,    ZoomAt in
#define IMG_KEY_DX                       "dx"                     // float,  Pan in
#define IMG_KEY_DY                       "dy"                     // float,  Pan in
#define IMG_KEY_ZOOM                     "zoom"                   // float,  Query out
#define IMG_KEY_HAS_IMAGE                "has_image"              // int,    Query out
#define IMG_KEY_IMAGE_WIDTH              "image_width"            // int,    Query out
#define IMG_KEY_IMAGE_HEIGHT             "image_height"           // int,    Query out
#define IMG_KEY_IS_HARDWARE              "is_hardware"            // int,    Query out
#define IMG_KEY_DEVICE_DESCRIPTION       "device_description"     // string, Query out
#define IMG_KEY_LAST_LOAD_MS             "last_load_ms"           // float,  Query out
#define IMG_KEY_LAST_FILE_SIZE_BYTES     "last_file_size_bytes"   // int,    Query out
#define IMG_KEY_CURRENT_ADAPTER_IS_WARP  "current_adapter_is_warp"// int,    Query out
#define IMG_KEY_CURRENT_ADAPTER_INDEX    "current_adapter_index"  // int,    Query out
#define IMG_KEY_RELOADED_IMAGE           "reloaded_image"         // int,    SwitchAdapter out
#define IMG_KEY_IS_TILED                 "is_tiled"               // int,    Query out (Phase 2 tiled-streaming diagnostics)
#define IMG_KEY_TILE_COUNT               "tile_count"             // int,    Query out (0 if not tiled)
#define IMG_KEY_RESIDENT_TILE_BYTES      "resident_tile_bytes"    // int,    Query out (0 if not tiled)
#define IMG_KEY_ADAPTER_COUNT            "adapter_count"          // int,    EnumerateAdapters out

// EnumerateAdapters' per-entry keys are indexed (see the List convention
// above), so they are format strings rather than fixed names -- format with
// the 0-based adapter index in place of %d, e.g.
// sprintf_s(key, IMG_KEY_ADAPTER_NAME_FMT, (int)i).
#define IMG_KEY_ADAPTER_NAME_FMT         "adapter_%d_name"        // string
#define IMG_KEY_ADAPTER_IS_WARP_FMT      "adapter_%d_is_warp"     // int
#define IMG_KEY_ADAPTER_INDEX_FMT        "adapter_%d_index"       // int

// Shape-annotation keys (see the Shape_* verbs near the bottom of this file).
#define IMG_KEY_SHAPE_TYPE               "shape_type"             // int,    Shape_SetTool in (IMG_SHAPE_* value)
#define IMG_KEY_HANDLED                  "handled"                // int,    Shape_OnMouseDown out (1 = consumed; caller should skip its own pan-start)
#define IMG_KEY_CHANGED                  "changed"                // int,    Shape_OnMouseMove/OnMouseUp/OnKeyDown/DeleteSelected out (1 = re-render needed)
#define IMG_KEY_SHAPE_ID                 "shape_id"               // int,    Shape_OnMouseDown out (-1 if none)
#define IMG_KEY_CURRENT_TOOL             "current_tool"           // int,    Shape_OnMouseDown/OnMouseUp out (tool can auto-revert to IMG_SHAPE_NONE)
#define IMG_KEY_VK_CODE                  "vk_code"                // int,    Shape_OnKeyDown in (standard VK_* value)
#define IMG_KEY_SHAPE_COUNT              "shape_count"            // int,    Shape_GetAll out
#define IMG_KEY_SELECTED_SHAPE_ID        "selected_shape_id"      // int,    Shape_GetAll out (-1 if none selected)

// Shape_GetAll's per-shape, per-point fields (see the List convention above).
// Point coordinates are in IMAGE-PIXEL space, unlike x/y above (screen
// pixels), so they stay meaningful across zoom/pan and across a save/reload.
#define IMG_KEY_SHAPE_ID_FMT             "shape_%d_id"            // int
#define IMG_KEY_SHAPE_TYPE_FMT           "shape_%d_type"          // int (IMG_SHAPE_*)
#define IMG_KEY_SHAPE_SELECTED_FMT       "shape_%d_selected"      // int (0/1)
#define IMG_KEY_SHAPE_POINT_COUNT_FMT    "shape_%d_point_count"   // int
#define IMG_KEY_SHAPE_POINT_X_FMT        "shape_%d_point_%d_x"    // float, image-pixel space
#define IMG_KEY_SHAPE_POINT_Y_FMT        "shape_%d_point_%d_y"    // float, image-pixel space

// in/out keys for the coordinate-origin switch and mouse-hover/label
// queries (see the Shape_SetCoordinateOrigin/Shape_GetLabels verbs and the
// hover_x/hover_y additions to Shape_OnMouseMove's out, near the bottom).
#define IMG_KEY_COORD_ORIGIN             "coord_origin"           // int,    Shape_SetCoordinateOrigin in (IMG_COORD_ORIGIN_*)
#define IMG_KEY_HOVER_X                  "hover_x"                // float,  Shape_OnMouseMove out (display space; honors coord_origin)
#define IMG_KEY_HOVER_Y                  "hover_y"                // float,  Shape_OnMouseMove out
#define IMG_KEY_LABEL_COUNT              "label_count"            // int,    Shape_GetLabels out
#define IMG_KEY_LABEL_X_FMT              "label_%d_x"             // int,    Shape_GetLabels out (SCREEN pixels of this viewer)
#define IMG_KEY_LABEL_Y_FMT              "label_%d_y"             // int,    Shape_GetLabels out
#define IMG_KEY_LABEL_TEXT_FMT           "label_%d_text"          // string, Shape_GetLabels out

// int, Shape_OnMouseMove out (IMG_SHAPE_* or IMG_SHAPE_NONE) -- the type of
// shape a custom "selected shape" cursor should represent right now (the
// selected shape's type, while hovering/dragging it or one of its resize
// handles), or IMG_SHAPE_NONE for the default cursor. See ImgEngine_Shape_OnMouseMove.
#define IMG_KEY_CURSOR_SHAPE_TYPE        "cursor_shape_type"      // int (IMG_SHAPE_*/IMG_SHAPE_NONE)

// int 0/1, Shape_OnMouseMove out -- 1 means the caller should show a
// generic "move" cursor (e.g. a 4-way arrow) INSTEAD of the shape-specific
// one named by cursor_shape_type: the selected shape is filled (see
// Shape_SetFilled) and the hover/drag is over its body, not a resize
// handle, so its whole interior is a draggable area, not just a thin
// outline. Always 0 for a Line (never fillable) and whenever
// cursor_shape_type is IMG_SHAPE_NONE.
#define IMG_KEY_CURSOR_IS_MOVE           "cursor_is_move"         // int (0/1)

// int 0/1, Shape_SetFilled in -- whether closed shapes (Rectangle, Circle, a
// finished Polygon) also render a translucent fill in addition to their
// outline. A Line (and a not-yet-closed Polygon) has no enclosed area and is
// never filled regardless of this setting. Default is 0 (outline only).
#define IMG_KEY_SHAPE_FILLED             "shape_filled"           // int (0/1)

// ---------------------------------------------------------------------------
// Shape annotation tool/type identifiers, for Shape_SetTool's in and
// Shape_GetAll's per-entry shape_<i>_type out. Additive only.
// IMG_SHAPE_NONE means "selection mode" (no draw tool armed) when passed to
// Shape_SetTool; it never appears as a shape's own type in Shape_GetAll.
// ---------------------------------------------------------------------------
#define IMG_SHAPE_NONE       0
#define IMG_SHAPE_RECTANGLE  1
#define IMG_SHAPE_CIRCLE     2
#define IMG_SHAPE_LINE       3
#define IMG_SHAPE_POLYGON    4

// ---------------------------------------------------------------------------
// Coordinate-origin convention for HUMAN-FACING DISPLAY values only: the
// text from Shape_GetLabels and the hover_x/hover_y out values of
// Shape_OnMouseMove. Has NO effect on the geometry Shape_GetAll reports,
// which is always the image's native top-left-origin pixel space, so saved/
// reloaded shape data never depends on a display preference. Default is
// IMG_COORD_ORIGIN_TOP_LEFT; persists across Shape_Clear() and image loads
// (it is a display preference, not part of the shape list being cleared).
// ---------------------------------------------------------------------------
#define IMG_COORD_ORIGIN_TOP_LEFT     0
#define IMG_COORD_ORIGIN_BOTTOM_LEFT  1

// ---------------------------------------------------------------------------
// Engine handle + ABI/capability probing.
//
// ImgEngineHandle is a small unique integer ID (Matrox-Imaging-Library-style
// "MIL_ID" handle), NOT a pointer -- the DLL owns a fixed table of at most
// IMG_MAX_INSTANCES live instances internally and maps each ID to one. This
// is deliberately the only thing that ever changes about a handle's
// representation; every ImgEngine_* signature below is untouched.
//
// IMG_INVALID_HANDLE (0) is returned by ImgEngine_Create() if the table is
// full, and is always rejected by every other call as IMG_ERR_INVALID_HANDLE.
// An ID also encodes a generation counter for the slot it came from, so a
// stale ID from an already-destroyed-and-reused slot is rejected as
// IMG_ERR_INVALID_HANDLE too, rather than silently operating on whatever
// newer instance happens to occupy that slot now.
// ---------------------------------------------------------------------------
typedef int32_t ImgEngineHandle;
#define IMG_INVALID_HANDLE ((ImgEngineHandle)0)

// Coarse "are we even compatible" gate (bump only on a truly unavoidable
// breaking change -- which the design above is meant to make unnecessary).
IMGENGINE_API uint32_t ImgEngine_GetAbiVersion(void);

// Per-feature probe for additive discovery without a version bump, e.g.
// ImgEngine_HasCapability("SwitchAdapter") or ("key.adapter_is_warp").
// Returns 1 if known/supported, 0 otherwise.
IMGENGINE_API int32_t ImgEngine_HasCapability(const char* name);

// `in` is reserved for future engine-level construction options; NULL or an
// empty map is fine today and yields today's defaults. Returns
// IMG_INVALID_HANDLE if IMG_MAX_INSTANCES instances are already live, or if
// the new instance's worker thread/state could not be created.
IMGENGINE_API ImgEngineHandle ImgEngine_Create(const ImgParamMap* in);

// No plausible future parameters for a destructor-shaped call -- deliberate,
// documented exception to the (handle, in, out) shape used below. A
// missing/invalid/already-destroyed handle is a safe no-op, not an error.
IMGENGINE_API void ImgEngine_Destroy(ImgEngineHandle engine);

// ---------------------------------------------------------------------------
// Parameterized verbs. ALL share the exact same shape:
//     ImgStatus ImgEngine_<Verb>(ImgEngineHandle, const ImgParamMap* in, ImgParamMap* out);
// `out` may be NULL if the caller doesn't need any results/diagnostics.
// ---------------------------------------------------------------------------

// in:  hwnd (ptr), width (int), height (int),
//      [optional] adapter_is_warp (int 0/1), adapter_index (int)
//      -- adapter_* absent => auto-detect HARDWARE then WARP fallback;
//         adapter_* present => initialize on that specific adapter.
// out: error_message (string) on failure.
IMGENGINE_API ImgStatus ImgEngine_Initialize(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// in:  EITHER path (string) OR buffer_ptr (ptr) + buffer_size (int) --
//      buffer_ptr/buffer_size take precedence if both are present. The
//      buffer only needs to stay valid for the duration of this call; the
//      engine does not retain it. Consequently an image loaded from a
//      buffer is NOT automatically reloaded after ImgEngine_SwitchAdapter
//      the way a path-loaded one is -- has_image will read 0 after a
//      hardware switch instead. A bad/dangling buffer_ptr is handled
//      without crashing (reported as IMG_ERR_DECODE_FAILED), but is still a
//      caller bug -- the pointer must reference at least buffer_size valid,
//      readable bytes.
// out: width, height (int), load_ms (float), file_size_bytes (int, the
//      source byte count whether from a file or a buffer),
//      error_message (string) on failure.
IMGENGINE_API ImgStatus ImgEngine_LoadImage(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// in: width, height (int)
IMGENGINE_API ImgStatus ImgEngine_Resize(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// in: factor (float), x, y (int) -- zoom around a point, screen coordinates.
IMGENGINE_API ImgStatus ImgEngine_ZoomAt(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// in: dx, dy (float) -- pan in screen pixels.
IMGENGINE_API ImgStatus ImgEngine_Pan(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// in: unused today; kept map-shaped since this is a stateful camera op.
IMGENGINE_API ImgStatus ImgEngine_ResetView(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// in:  unused today.
// out: zoom (float), has_image (int 0/1), image_width, image_height (int),
//      is_hardware (int 0/1), device_description (string),
//      last_load_ms (float), last_file_size_bytes (int),
//      current_adapter_is_warp (int 0/1), current_adapter_index (int).
IMGENGINE_API ImgStatus ImgEngine_Query(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// Re-creates the device on a different adapter and, if an image was
// previously loaded, reloads it. On failure, rolls back to the previously
// active adapter (and reloads the image again) on a best-effort basis so the
// engine is left usable.
// in:  adapter_is_warp (int 0/1), adapter_index (int, ignored if is_warp)
// out: error_message (string) on failure, reloaded_image (int 0/1).
IMGENGINE_API ImgStatus ImgEngine_SwitchAdapter(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// No handle: mirrors a static query, does not require an initialized engine.
// in:  unused today.
// out: adapter_count (int), and for i in [0, adapter_count):
//      adapter_<i>_name (string), adapter_<i>_is_warp (int 0/1),
//      adapter_<i>_index (int, meaningless when is_warp).
IMGENGINE_API ImgStatus ImgEngine_EnumerateAdapters(const ImgParamMap* in, ImgParamMap* out);

// ---------------------------------------------------------------------------
// Shape annotation verbs. Each engine instance owns its own independent set
// of shapes -- storage, selection, hit-testing, resize-handle math, and
// polygon-closing detection all live inside the DLL. The caller (the EXE/UI)
// only forwards raw mouse/keyboard events in and reads back what changed; it
// never stores or computes any shape geometry itself, and never needs to
// re-derive it -- ImgEngine_Render() draws whatever the engine currently
// holds. x/y in these verbs are SCREEN pixels (same convention as ZoomAt),
// converted internally to image-pixel space so shapes stay anchored to image
// content across zoom/pan; Shape_GetAll's point list is the one place shape
// geometry crosses the boundary, and that is in IMAGE-PIXEL space (see
// IMG_KEY_SHAPE_POINT_X_FMT/_Y_FMT) so it stays meaningful independent of the
// current view. Every point is clamped into [0, image_width] x
// [0, image_height] before it's ever stored, so no shape (and no part of a
// shape -- a Circle's radius is clamped too, not just its defining points)
// can ever extend past the loaded image's bounds.
// ---------------------------------------------------------------------------

// in: shape_type (int, IMG_SHAPE_* -- IMG_SHAPE_NONE switches to selection
//     mode). Arming a draw tool discards any in-progress, not-yet-finished
//     shape (e.g. switching tools mid-polygon abandons that polygon).
IMGENGINE_API ImgStatus ImgEngine_Shape_SetTool(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// in:  x, y (int, screen pixels)
// out: handled (int 0/1 -- 1 means the press was consumed by shape selection,
//      grabbing a resize handle, starting/continuing/finishing a draw, or
//      starting a whole-shape move-drag; the caller should skip its own
//      image-pan-start logic when this is 1), shape_id (int, -1 if none),
//      current_tool (int, IMG_SHAPE_*). In selection mode, pressing on any
//      finished shape both selects it AND immediately arms a move-drag
//      (continuing to drag before releasing translates the whole shape,
//      clamped to the image bounds so it stops at the edge rather than
//      letting any part of it move past it) -- releasing without moving
//      just leaves it selected.
IMGENGINE_API ImgStatus ImgEngine_Shape_OnMouseDown(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// Safe to call on every mouse move regardless of button state -- needed for
// the live rubber-band preview while hovering between line/polygon clicks,
// and to drive a live mouse-position readout in the UI.
// in:  x, y (int, screen pixels)
// out: changed (int 0/1 -- 1 means a re-render is needed), hover_x, hover_y
//      (float, the same point clamped into [0,image_width]x[0,image_height]
//      and reported in DISPLAY coordinate space -- see IMG_COORD_ORIGIN_*/
//      Shape_SetCoordinateOrigin -- always set whenever a point conversion
//      was possible, regardless of `changed`), cursor_shape_type (int,
//      IMG_SHAPE_*/IMG_SHAPE_NONE -- which custom "selected shape" cursor,
//      if any, the caller should show right now), cursor_is_move (int 0/1
//      -- show a generic move cursor instead of the shape-specific one;
//      see IMG_KEY_CURSOR_IS_MOVE).
IMGENGINE_API ImgStatus ImgEngine_Shape_OnMouseMove(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// in:  x, y (int, screen pixels)
// out: changed (int 0/1), current_tool (int, IMG_SHAPE_* -- a draw tool
//      auto-reverts to IMG_SHAPE_NONE once its shape is finished).
IMGENGINE_API ImgStatus ImgEngine_Shape_OnMouseUp(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// in:  vk_code (int, standard VK_* value -- currently only VK_DELETE does
//      anything, equivalent to calling Shape_DeleteSelected).
// out: changed (int 0/1).
IMGENGINE_API ImgStatus ImgEngine_Shape_OnKeyDown(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// Explicit verb for "delete the selected shape", reachable without
// synthesizing a keydown.
// out: changed (int 0/1 -- 0 if nothing was selected).
IMGENGINE_API ImgStatus ImgEngine_Shape_DeleteSelected(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// Removes all shapes. Callers should call this after loading a new image
// into this instance, since existing shape coordinates would otherwise be
// meaningless against different image content.
IMGENGINE_API ImgStatus ImgEngine_Shape_Clear(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// in:  unused today.
// out: shape_count (int), selected_shape_id (int, -1 if none), and for i in
//      [0, shape_count): shape_<i>_id, shape_<i>_type (IMG_SHAPE_*),
//      shape_<i>_selected (int 0/1), shape_<i>_point_count (int), and for j
//      in [0, point_count): shape_<i>_point_<j>_x, shape_<i>_point_<j>_y
//      (float, image-pixel space).
IMGENGINE_API ImgStatus ImgEngine_Shape_GetAll(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// Switches the coordinate-origin convention used for DISPLAY values only
// (Shape_GetLabels text, Shape_OnMouseMove's hover_x/hover_y) -- see the
// IMG_COORD_ORIGIN_* comment above. Does not affect Shape_GetAll's geometry.
// in: coord_origin (int, IMG_COORD_ORIGIN_*).
IMGENGINE_API ImgStatus ImgEngine_Shape_SetCoordinateOrigin(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// Text labels: one per POINT of every shape (its coordinate, positioned at
// the point itself -- this is what shows a Line's start/end, a Rectangle's
// corners, a Circle's center+edge, a Polygon's vertices), plus one summary
// label per shape type that has dedicated text (Rectangle: width/height;
// Circle: diameter; Line: length; Polygon: none -- its per-point labels
// already cover it). Every shape that's still being drawn gets labels too
// (e.g. live width/height while dragging a rectangle), not just finished
// ones. Positions are already converted to this viewer's current SCREEN
// pixels (and nudged by a small constant screen-pixel offset so the text
// sits just outside the shape) -- the caller only needs to draw the text at
// the given (x,y), no further transform needed.
// in:  unused today.
// out: label_count (int), and for i in [0, label_count): label_<i>_x,
//      label_<i>_y (int, screen pixels of this viewer), label_<i>_text
//      (string, already formatted and honoring the current coord_origin).
IMGENGINE_API ImgStatus ImgEngine_Shape_GetLabels(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// Whether closed shapes (Rectangle, Circle, a finished Polygon) also render
// a translucent fill in addition to their outline -- see IMG_KEY_SHAPE_FILLED.
// in: shape_filled (int 0/1).
IMGENGINE_API ImgStatus ImgEngine_Shape_SetFilled(ImgEngineHandle engine, const ImgParamMap* in, ImgParamMap* out);

// ---------------------------------------------------------------------------
// Hot-path render. Deliberately EXEMPT from the (in, out) map shape above:
// it takes zero parameters today and is expected to always take zero --
// there is nothing about "draw the current state" that needs to vary per
// call, and forcing every WM_PAINT-driven Present() through a map
// indirection would add avoidable per-frame cost for no benefit. If a real
// need for render-time parameters ever appears, that is a NEW function, not
// a change to this one.
// ---------------------------------------------------------------------------
IMGENGINE_API ImgStatus ImgEngine_Render(ImgEngineHandle engine);

#ifdef __cplusplus
}
#endif
