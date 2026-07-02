# HiResImageRenderer — Release Notes

`HiResImageRenderer.dll` is a standalone, high-resolution image load-and-render
engine exposed through a stable, pure-C ABI (see `HiResImageRenderer.h`). It is
built as its own project (sibling to, not nested inside, the `ImageViewer` EXE
project) so it can be built, versioned, and redistributed independently of any
one host application.

## Version 1.1.0.0

**Phase 2: on-demand tiled image streaming.** Images exceeding the Direct3D
Feature Level 11 single-texture limit (16384 × 16384 px) previously failed to
load (`IMG_ERR_IMAGE_TOO_LARGE`). They now load via a new tiled-streaming
path: the full image is still decoded once to a host RAM buffer (WIC doesn't
reliably support partial/region decode that saves time across codecs, so this
one-time cost is unavoidable), but it's then split into a grid of GPU tiles
(4096×4096 px each) that are uploaded on demand — only tiles intersecting the
current viewport are GPU-resident at any time, with LRU eviction (with
hysteresis to avoid thrashing at tile boundaries) once a 1.5 GiB VRAM budget
is exceeded. This means VRAM usage for a tiled image stays bounded regardless
of source size, while panning/zooming still works exactly as before (shapes,
zoom-at-point, fit-to-window, hardware/WARP switching all work unchanged on a
tiled image — they're agnostic to whether the image is one texture or many).

New diagnostics, additive to `ImgEngine_Query`'s output: `IMG_KEY_IS_TILED`,
`IMG_KEY_TILE_COUNT`, `IMG_KEY_RESIDENT_TILE_BYTES`. `IMG_ERR_IMAGE_TOO_LARGE`
remains defined (status codes are never removed) but is no longer returned in
practice.

## Version 1.0.0.0 (initial versioned release)

This was the first release to carry a formal file/product version. Everything
below reflects the feature set as of that version.

---

### Stable C ABI

- Pure `extern "C"` boundary — opaque handles, no STL/MFC types in any
  exported signature, so it survives compiler/CRT drift between the DLL and
  whatever EXE links against it.
- Generic key/value parameter maps (`ImgParamMap`) carry every verb's
  in/out arguments, so new parameters are added as new keys rather than by
  changing a function signature. Compile-time-checked `IMG_KEY_*` /
  `IMG_SHAPE_*` / `IMG_COORD_ORIGIN_*` constants are provided as a
  convenience layer over the underlying strings.
- `ImgEngine_GetAbiVersion()` / `ImgEngine_HasCapability(name)` let a caller
  probe compatibility instead of assuming it.
- Status codes are additive-only (never renumbered/removed); specific codes
  exist for invalid handle/argument, not-initialized, device-init-failed,
  image-too-large, decode-failed, out-of-memory, and GPU-upload-failed.
- Exceptions never cross the DLL boundary — every exported function catches
  internally and reports failure via its `ImgStatus` return value.

### Multi-instance engine

- `ImgEngine_Create` / `ImgEngine_Destroy` hand out small unique integer IDs
  (Matrox-MIL-style), up to `IMG_MAX_INSTANCES` (5) concurrent instances,
  managed entirely inside the DLL via a slot table with generation counters
  (a stale ID from an already-destroyed slot is rejected, never silently
  misapplied to a newer instance reusing that slot).
- Each instance's entire pipeline (Initialize, LoadImage, Resize, ZoomAt,
  Pan, ResetView, Render, every `Shape_*` verb) runs on that instance's own
  dedicated worker thread, so two instances' GPU work genuinely overlaps in
  wall-clock time — while every individual call still blocks its caller
  synchronously until that one operation finishes.

### Hardware selection

- Auto-detects a HARDWARE (GPU) device, falling back to WARP (CPU software
  rasterizer) if none is available.
- `ImgEngine_EnumerateAdapters` / `ImgEngine_SwitchAdapter` let a caller pick
  a specific GPU or WARP at runtime; a failed switch automatically rolls
  back to the previous adapter (and reloads the image) on a best-effort
  basis, so the engine is never left in a broken state.

### Image loading

- Loads from a file path **or** from an in-memory buffer.
- The buffer-load path is SEH-guarded around the WIC decode step, so a
  bad/dangling caller-supplied pointer is reported as a decode failure
  instead of crashing the process.
- Reports decode+upload time (ms) and source byte size.
- Images within the Direct3D Feature Level 11 single-texture limit
  (16384×16384 px) load as one GPU texture (the original Phase 1 path).
  Larger images use the Phase 2 on-demand tiled streaming path instead (see
  "Version 1.1.0.0" above) — the remaining practical limit is available host
  RAM to hold one full decode, not a pixel-dimension cap.

### Rendering

- Direct3D 11, runtime-compiled HLSL (no offline shader build step).
- Image quad with pan and zoom-at-point (the point under the cursor stays
  fixed while zooming); fit-to-window reset.

### Vector shape annotations

All shape state, geometry, hit-testing, and rendering data are owned
entirely inside the DLL — a host application only forwards raw mouse/
keyboard input and draws whatever the DLL reports back.

- **4 shape types**: Rectangle, Circle, Line, Polygon.
- **Drawing**: drag-to-draw for Rectangle/Circle; click-by-click for Line
  (2 clicks) and Polygon (3+ clicks, closed by clicking back near the first
  vertex).
- **Selection**: click to select, shown in a distinct highlight color.
- **Resize**: per-shape-type drag handles (direct point-mapping rather than
  one generic bounding-box-corner algorithm, which avoids a degenerate case
  for a Line's zero-width/height bounding box).
- **Move**: dragging a selected shape's body translates the whole shape; the
  move delta is clamped against the shape's own bounding box so it stops
  exactly at the image edge instead of distorting the shape.
- **Delete**: via the Delete key or an explicit `Shape_DeleteSelected` verb.
- **Image-bounds clamping**: no shape, or part of one, can ever be drawn,
  moved, or resized outside the loaded image. A Circle's radius is clamped
  relative to its center's distance to the nearest image edge (not just by
  clamping its defining points), so it can't bulge past a corner.
- **Text labels**: a coordinate label at every point of every shape (a
  Line's start/end, a Rectangle's corners, a Circle's center+edge, a
  Polygon's vertices), plus one summary label per type that has one
  (Rectangle: width/height, centered on the shape; Circle: diameter; Line:
  length, as `L:##`). Host-controllable independently of the DLL via a
  "show labels" UI toggle, since label drawing is the one piece of shape
  presentation left to the host (GDI text has no D3D11 equivalent).
- **Circle center marker**: a small, constant-screen-size red dot always
  rendered at a circle's center, correctly tracking pan/zoom/resize.
- **Optional translucent fill** (`Shape_SetFilled`) for closed shapes
  (Rectangle, Circle, a finished Polygon — a Line has no enclosed area and
  is never filled): alpha-blended so the image underneath stays visible,
  fan-triangulated from the shape's own outline.
- **Point-in-shape hit-testing**: once filled, clicking anywhere inside a
  shape's interior (not just near its outline) selects/moves it.
- **Custom cursors**: a per-shape-type cursor (built at runtime via GDI, no
  `.cur` resources needed) while hovering or dragging a selected shape or
  one of its resize handles; a generic "move" cursor instead when the shape
  is filled and the cursor is over its body.
- **Coordinate-origin switch** (`Shape_SetCoordinateOrigin`): top-left
  (default) or bottom-left, affecting only human-facing DISPLAY values
  (label text, hover readout) — the geometry returned by `Shape_GetAll` is
  always reported in the image's native top-left-origin pixel space,
  independent of this display preference.
- **Mouse-hover coordinate readout**: live image-pixel position under the
  cursor, clamped to the image's bounds, honoring the coordinate-origin
  setting.

### Multi-viewer support (host UI feature, built on the above)

- Two independent, side-by-side viewers per host window, each its own
  engine instance — independent image, shapes, hardware adapter, zoom/pan,
  and every shape-display preference (fill/labels/origin).

### Diagnostics

- Image resolution, source file size, decoded texture memory size, load
  time, and zoom % via `ImgEngine_Query`.
- Live per-process CPU%/GPU% usage available to the host via the existing
  Windows PDH "GPU Engine" counters (host-side; not part of this DLL's ABI).

---

## Known limitations (by design)

- The tiled-streaming path (v1.1.0.0) still decodes the full source image to
  a host RAM buffer once at load time — there is no true disk-level partial
  decode, since WIC doesn't reliably support region decode that saves
  time/CPU across codecs (only uncompressed formats like BMP could benefit,
  and not consistently). This is a one-time cost proportional to source size,
  not a hard cap, but a truly multi-GB-decoded image could still be slow to
  load or exhaust host RAM. No LOD/mipmap pyramid exists yet for fast
  zoomed-out viewing of gigapixel images — zooming out still uploads full-
  resolution tiles for whatever's visible.
- Polygon fill uses a simple fan triangulation from the polygon's own
  outline: exact for convex polygons (and for Rectangle/Circle, which are
  always convex); a concave polygon's fill can show minor double-blended
  overlap in the fan's triangles outside its true interior. The outline,
  hit-testing, and all other shape behavior are unaffected and correct for
  concave polygons.
