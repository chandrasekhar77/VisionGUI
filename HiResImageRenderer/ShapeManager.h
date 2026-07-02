// ShapeManager.h
// ---------------------------------------------------------------------------
// Owns all vector-shape annotation state for one engine instance: the shape
// list, the current drawing tool, the in-progress draw/resize state machine,
// hit-testing, and per-shape-type handle dragging. Pure geometry/state, no
// MFC, no D3D, no awareness of screen pixels at all -- every coordinate here
// is in IMAGE-PIXEL space. The engine glue (HiResImageRenderer.cpp) converts
// incoming screen coordinates to image space (via D3DRenderer::ScreenToImage)
// before calling in here, and converts a hit-test tolerance from a constant
// screen-pixel value to the equivalent image-pixel value (which shrinks as
// you zoom in) before calling in here too.
//
// Resize handles are deliberately NOT one generic "scale from a bounding-box
// corner" algorithm: a Line's bounding box can be degenerate (zero width or
// height), which makes uniform corner-scaling ill-defined. Instead every
// handle maps directly onto one or two specific stored point components for
// that shape (see ApplyHandleDrag) -- simpler and avoids that edge case.
// ---------------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>

// Mirrors the IMG_SHAPE_* constants in HiResImageRenderer.h (kept as a plain
// int at this layer so this header doesn't need to depend on that one).
enum class ShapeType { Rectangle = 1, Circle = 2, Line = 3, Polygon = 4 };

struct PointF
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Shape
{
    int                  id = 0;
    ShapeType            type = ShapeType::Rectangle;
    std::vector<PointF>  points;        // image-pixel space; semantics depend on type (see .cpp)
    bool                 selected = false;
    bool                 finished = false; // false while still being drawn
};

// One drawable entity ready for D3DRenderer: an outline polyline (already
// tessellated for circles) in image-pixel space, plus optional handle marker
// CENTERS (also image-pixel space; D3DRenderer expands each into a small,
// constant-SCREEN-size square) for the selected shape only.
struct ShapeRenderEntry
{
    std::vector<PointF> outline;
    bool                 closed = false;
    bool                 selected = false;     // selects the highlight color
    bool                 inProgress = false;    // selects the "being drawn" preview color
    bool                 wantFill = false;      // true if D3DRenderer should also draw a translucent fill (closed shapes only)
    std::vector<PointF>  handles;               // empty unless this is the selected, finished shape
    bool                 hasCenterMarker = false; // Circle only: draw a small filled red dot at centerMarker
    PointF               centerMarker;
};

// One text label to draw over the image: an anchor point in IMAGE-PIXEL
// space (the engine glue converts to screen space via D3DRenderer::
// ImageToScreen, then nudges it by a constant SCREEN-pixel offset so it
// doesn't shrink/grow with zoom) plus the already-formatted text.
struct ShapeLabel
{
    PointF       anchor;
    std::wstring text;
};

class ShapeManager
{
public:
    // IMG_SHAPE_NONE(0) = selection mode. Arming a draw tool clears any
    // in-progress shape that was never finished (e.g. switching tools
    // mid-polygon discards the unfinished polygon).
    void SetTool(int shapeType);
    int  GetTool() const { return m_tool; }

    // Returns true if the press was consumed by shape interaction (select,
    // grab a resize handle, start/continue/finish a draw) -- the caller
    // should skip its own image-pan-start logic when this is true.
    // outShapeId receives the relevant shape's id, or -1.
    bool OnMouseDown(float imgX, float imgY, float hitToleranceImg, int* outShapeId);

    // Always safe to call on every mouse move regardless of button state --
    // updates live drag/resize geometry or the rubber-band preview point for
    // an in-progress line/polygon. outChanged is true if a re-render is needed.
    void OnMouseMove(float imgX, float imgY, bool* outChanged);

    // outChanged is true if a re-render is needed (drag/resize finalized).
    void OnMouseUp(float imgX, float imgY, bool* outChanged);

    // vkCode is a standard VK_* value. Currently only VK_DELETE does
    // anything (delegates to DeleteSelected). Returns true if changed.
    bool OnKeyDown(int vkCode);

    // Returns true if a shape was actually deleted.
    bool DeleteSelected();

    void Clear();

    int GetSelectedId() const { return m_selectedShapeId; }

    std::vector<ShapeRenderEntry> BuildRenderData() const;

    const std::vector<Shape>& GetShapes() const { return m_shapes; }

    // Sets the current image's pixel dimensions, so every point this class
    // accepts (draw, drag, resize) gets clamped into [0,width]x[0,height] --
    // shapes (and circle radii) can never extend past the image. Width/height
    // <= 0 means "unbounded" (no clamping), which is also the default before
    // the first SetImageBounds call. Not affected by Clear() -- this is
    // environment state, not part of the shape list being cleared.
    void SetImageBounds(float width, float height);

    // Coordinate-origin convention used ONLY for human-facing DISPLAY values
    // (BuildLabels' text, and the engine glue's hover_x/hover_y out values).
    // Has NO effect on the raw geometry in GetShapes()/Shape_GetAll, which is
    // always reported in the image's native top-left-origin pixel space.
    // `origin` is IMG_COORD_ORIGIN_TOP_LEFT(0, default) or _BOTTOM_LEFT(1).
    void SetCoordinateOrigin(int origin);
    int  GetCoordinateOrigin() const { return static_cast<int>(m_coordOrigin); }

    // Applies the current coordinate-origin convention to one image-pixel
    // point, for display purposes (e.g. the engine glue's mouse-hover
    // readout). X is unchanged; Y flips around the image height when the
    // origin is bottom-left.
    PointF ToDisplayCoords(PointF imagePoint) const;

    // Whether closed shapes (Rectangle, Circle, a finished Polygon) should
    // also render a translucent fill in addition to their outline. An open
    // shape (Line, or a Polygon not yet closed) has no enclosed area and is
    // never filled regardless of this setting. Not affected by Clear() --
    // a display preference, not part of the shape list being cleared.
    void SetFilled(bool filled) { m_filled = filled; }
    bool GetFilled() const { return m_filled; }

    // One label per point of every shape (its coordinate, positioned at the
    // point itself) plus one aggregate label per shape that has dedicated
    // summary text (Rectangle: width/height, positioned at its own center;
    // Circle: diameter; Line: "L" length; Polygon: none today -- its
    // per-point labels already cover it). Anchor points are in image-pixel
    // space; text already honors the current coordinate-origin convention.
    std::vector<ShapeLabel> BuildLabels() const;

    // The type (IMG_SHAPE_*) of the shape a custom cursor should represent
    // right now, or IMG_SHAPE_NONE if the default cursor should show
    // instead: the selected shape's type while actively moving/resizing it,
    // or while merely hovering its outline/a handle; IMG_SHAPE_NONE
    // otherwise (nothing selected, or hovering elsewhere). Called from
    // OnMouseMove's engine-glue caller on every move, hover included.
    // *outIsGenericMove is set to true instead when the shape is filled (see
    // SetFilled) and the hover/drag is over its body (not a resize handle)
    // -- callers should show a generic "move" cursor (e.g. a 4-way arrow)
    // rather than this returned shape-specific one in that case, since a
    // filled shape's whole interior is now a draggable body, not just a
    // thin outline.
    int GetCursorShapeType(float imgX, float imgY, float hitToleranceImg, bool* outIsGenericMove) const;

private:
    enum class DragMode { None, DrawingRectOrCircle, DrawingPolyOrLine, ResizingHandle, MovingShape };
    enum class CoordOrigin { TopLeft = 0, BottomLeft = 1 };

    Shape*       FindShape(int id);
    const Shape* FindShape(int id) const;
    void         DeselectAll();
    // -1 if none. Outline proximity (within tol) takes priority; when
    // m_filled is set, a finished closed shape (Rectangle/Circle/Polygon,
    // never a Line) containing the point is also a hit even far from its
    // outline -- a filled shape's whole body is clickable, not just its edge.
    int          HitTestShapes(float x, float y, float tol) const;
    int          HitTestHandles(const Shape& s, float x, float y, float tol) const; // handle index, -1 if none
    void         ApplyHandleDrag(Shape& s, int handleIndex, float x, float y);
    void         GetHandleCenters(const Shape& s, std::vector<PointF>& out) const;
    static float DistancePointToSegment(PointF p, PointF a, PointF b);

    // Distance from (x,y) to shape s's own outline (same per-type geometry
    // HitTestShapes uses, factored out so GetCursorShapeType can test a
    // single known shape without scanning the whole list). -1 if undefined.
    float DistanceToShapeOutline(const Shape& s, float x, float y) const;

    // True if (x,y) is inside shape s's enclosed area. Only meaningful for
    // closed shapes (Rectangle/Circle/Polygon); always false for a Line (no
    // interior). Polygon uses the standard even-odd ray-casting test, so it
    // works for concave (and gives a reasonable, if not rule-perfect, result
    // for self-intersecting) polygons too.
    bool IsPointInsideShape(const Shape& s, float x, float y) const;

    // Axis-aligned bounding box of shape s -- for Rectangle/Line/Polygon,
    // the min/max of its points; for Circle, center +/- radius (its stored
    // points alone don't bound the curve). Used to clamp a whole-shape move
    // so it stops exactly at the image edge instead of letting one point
    // hit the boundary while others keep going (which would distort the
    // shape instead of just stopping its movement).
    void ComputeShapeBounds(const Shape& s, float& left, float& top, float& right, float& bottom) const;

    // Clamps a candidate point into the current image bounds (no-op if
    // bounds are unset). Called once at the top of every public mouse-input
    // method, so every downstream use of x/y -- shape creation, drag update,
    // handle drag -- is already in-bounds with no further clamping needed.
    PointF ClampToImage(float x, float y) const;

    // A Rectangle/Line/Polygon is automatically fully in-bounds once every
    // one of its vertices is (the image bounds rectangle is convex, so any
    // segment between two in-bounds points stays in-bounds). A Circle is
    // the one shape whose curve can bulge outside that even when its two
    // defining points (center, edge) are individually in-bounds, so it gets
    // this dedicated radius clamp: shrink the edge point along the
    // center->edge direction so the radius never exceeds the center's
    // distance to the nearest image edge.
    void ConstrainCircleRadius(Shape& s) const;

    std::vector<Shape> m_shapes;
    int  m_tool = 0; // IMG_SHAPE_NONE
    int  m_nextId = 1;
    int  m_selectedShapeId = -1;

    DragMode m_dragMode = DragMode::None;
    int      m_inProgressShapeId = -1; // shape currently being drawn (rect/circle drag, or line/poly clicks)
    bool     m_hasLivePreview = false; // true while hovering between line/polygon clicks
    PointF   m_livePreviewPoint;

    int      m_resizeHandleIndex = -1; // which handle of m_selectedShapeId is being dragged
    PointF   m_moveLastPoint;          // last mouse position during a MovingShape drag, for incremental deltas

    float       m_imageWidth = 0.0f;  // <= 0 means unbounded/unset
    float       m_imageHeight = 0.0f;
    CoordOrigin m_coordOrigin = CoordOrigin::TopLeft;
    bool        m_filled = false; // see SetFilled
};
