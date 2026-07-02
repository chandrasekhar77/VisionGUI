// ShapeManager.cpp
// ---------------------------------------------------------------------------
#include "ShapeManager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#ifndef NOMINMAX
#define NOMINMAX   // windows.h's min/max macros would otherwise break std::min/std::max below
#endif
#include <windows.h>   // VK_DELETE only

namespace
{
    float Distance(PointF a, PointF b)
    {
        const float dx = b.x - a.x, dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }
}

float ShapeManager::DistancePointToSegment(PointF p, PointF a, PointF b)
{
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float lenSq = dx * dx + dy * dy;
    float t = 0.0f;
    if (lenSq > 1e-9f)
    {
        t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }
    const PointF proj{ a.x + t * dx, a.y + t * dy };
    return Distance(p, proj);
}

Shape* ShapeManager::FindShape(int id)
{
    for (auto& s : m_shapes) if (s.id == id) return &s;
    return nullptr;
}

const Shape* ShapeManager::FindShape(int id) const
{
    for (auto& s : m_shapes) if (s.id == id) return &s;
    return nullptr;
}

void ShapeManager::DeselectAll()
{
    for (auto& s : m_shapes) s.selected = false;
    m_selectedShapeId = -1;
}

void ShapeManager::SetTool(int shapeType)
{
    // Switching tools abandons any unfinished in-progress shape (e.g. a
    // half-drawn polygon) rather than leaving a dangling partial shape.
    if (m_inProgressShapeId >= 0)
    {
        m_shapes.erase(std::remove_if(m_shapes.begin(), m_shapes.end(),
            [this](const Shape& s) { return s.id == m_inProgressShapeId && !s.finished; }),
            m_shapes.end());
        m_inProgressShapeId = -1;
    }
    m_dragMode = DragMode::None;
    m_hasLivePreview = false;
    m_tool = shapeType;
}

void ShapeManager::GetHandleCenters(const Shape& s, std::vector<PointF>& out) const
{
    out.clear();
    switch (s.type)
    {
    case ShapeType::Rectangle:
        // 0=points[0], 1=points[1], 2=(p0.x,p1.y), 3=(p1.x,p0.y) -- dragging
        // any one keeps the diagonally-opposite corner fixed.
        out = { s.points[0], s.points[1],
                PointF{ s.points[0].x, s.points[1].y },
                PointF{ s.points[1].x, s.points[0].y } };
        break;
    case ShapeType::Circle:
        out = { s.points[1] }; // the edge point; dragging it sets the radius directly
        break;
    case ShapeType::Line:
        out = { s.points[0], s.points[1] };
        break;
    case ShapeType::Polygon:
        out = s.points; // one handle per vertex
        break;
    }
}

int ShapeManager::HitTestHandles(const Shape& s, float x, float y, float tol) const
{
    std::vector<PointF> handles;
    GetHandleCenters(s, handles);

    int best = -1;
    float bestDist = tol;
    for (size_t i = 0; i < handles.size(); ++i)
    {
        const float d = Distance(handles[i], { x, y });
        if (d <= bestDist) { bestDist = d; best = static_cast<int>(i); }
    }
    return best;
}

void ShapeManager::ApplyHandleDrag(Shape& s, int handleIndex, float x, float y)
{
    switch (s.type)
    {
    case ShapeType::Rectangle:
        switch (handleIndex)
        {
        case 0: s.points[0] = { x, y }; break;
        case 1: s.points[1] = { x, y }; break;
        case 2: s.points[0].x = x; s.points[1].y = y; break;
        case 3: s.points[1].x = x; s.points[0].y = y; break;
        default: break;
        }
        break;
    case ShapeType::Circle:
        s.points[1] = { x, y }; // only handle 0
        ConstrainCircleRadius(s);
        break;
    case ShapeType::Line:
        if (handleIndex == 0) s.points[0] = { x, y };
        else if (handleIndex == 1) s.points[1] = { x, y };
        break;
    case ShapeType::Polygon:
        if (handleIndex >= 0 && handleIndex < static_cast<int>(s.points.size()))
            s.points[handleIndex] = { x, y };
        break;
    }
}

float ShapeManager::DistanceToShapeOutline(const Shape& s, float x, float y) const
{
    switch (s.type)
    {
    case ShapeType::Rectangle:
    {
        const float l = std::min(s.points[0].x, s.points[1].x);
        const float r = std::max(s.points[0].x, s.points[1].x);
        const float t = std::min(s.points[0].y, s.points[1].y);
        const float b = std::max(s.points[0].y, s.points[1].y);
        const PointF c0{ l, t }, c1{ r, t }, c2{ r, b }, c3{ l, b };
        return std::min({ DistancePointToSegment({ x, y }, c0, c1), DistancePointToSegment({ x, y }, c1, c2),
                           DistancePointToSegment({ x, y }, c2, c3), DistancePointToSegment({ x, y }, c3, c0) });
    }
    case ShapeType::Circle:
    {
        const float radius = Distance(s.points[0], s.points[1]);
        return std::fabs(Distance({ x, y }, s.points[0]) - radius);
    }
    case ShapeType::Line:
        return DistancePointToSegment({ x, y }, s.points[0], s.points[1]);
    case ShapeType::Polygon:
    {
        float d = -1.0f;
        for (size_t i = 0; i < s.points.size(); ++i)
        {
            const PointF& a = s.points[i];
            const PointF& b = s.points[(i + 1) % s.points.size()];
            const float segD = DistancePointToSegment({ x, y }, a, b);
            if (d < 0.0f || segD < d) d = segD;
        }
        return d;
    }
    }
    return -1.0f;
}

bool ShapeManager::IsPointInsideShape(const Shape& s, float x, float y) const
{
    switch (s.type)
    {
    case ShapeType::Rectangle:
    {
        const float l = std::min(s.points[0].x, s.points[1].x);
        const float r = std::max(s.points[0].x, s.points[1].x);
        const float t = std::min(s.points[0].y, s.points[1].y);
        const float b = std::max(s.points[0].y, s.points[1].y);
        return x >= l && x <= r && y >= t && y <= b;
    }
    case ShapeType::Circle:
        return Distance({ x, y }, s.points[0]) <= Distance(s.points[0], s.points[1]);
    case ShapeType::Polygon:
    {
        // Standard even-odd ray-casting point-in-polygon test (PNPOLY).
        bool inside = false;
        for (size_t i = 0, j = s.points.size() - 1; i < s.points.size(); j = i++)
        {
            const PointF& a = s.points[i];
            const PointF& b = s.points[j];
            if (((a.y > y) != (b.y > y)) &&
                (x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x))
            {
                inside = !inside;
            }
        }
        return inside;
    }
    case ShapeType::Line:
    default:
        return false; // no enclosed area
    }
}

int ShapeManager::HitTestShapes(float x, float y, float tol) const
{
    int best = -1;
    float bestDist = tol;
    for (const auto& s : m_shapes)
    {
        if (!s.finished) continue;
        const float d = DistanceToShapeOutline(s, x, y);
        if (d >= 0.0f && d <= bestDist)
        {
            bestDist = d;
            best = s.id;
        }
    }
    if (best >= 0) return best; // outline proximity takes priority

    // A filled shape's whole body is clickable, not just its edge.
    if (m_filled)
    {
        for (const auto& s : m_shapes)
        {
            if (s.finished && IsPointInsideShape(s, x, y))
                return s.id;
        }
    }
    return -1;
}

void ShapeManager::ComputeShapeBounds(const Shape& s, float& left, float& top, float& right, float& bottom) const
{
    if (s.type == ShapeType::Circle)
    {
        const float radius = Distance(s.points[0], s.points[1]);
        left = s.points[0].x - radius; right = s.points[0].x + radius;
        top = s.points[0].y - radius; bottom = s.points[0].y + radius;
        return;
    }
    left = top = 3.4e38f;
    right = bottom = -3.4e38f;
    for (const auto& p : s.points)
    {
        left = std::min(left, p.x);   right = std::max(right, p.x);
        top = std::min(top, p.y);     bottom = std::max(bottom, p.y);
    }
}

int ShapeManager::GetCursorShapeType(float x, float y, float tol, bool* outIsGenericMove) const
{
    if (outIsGenericMove) *outIsGenericMove = false;

    // A Line is never fillable, so it never gets the generic-move treatment
    // even when m_filled is on -- only Rectangle/Circle/Polygon can.
    auto isFillable = [](const Shape& s) { return s.type != ShapeType::Line; };

    if (m_dragMode == DragMode::ResizingHandle)
    {
        if (const Shape* s = FindShape(m_selectedShapeId))
            return static_cast<int>(s->type);
        return 0; // IMG_SHAPE_NONE
    }
    if (m_dragMode == DragMode::MovingShape)
    {
        if (const Shape* s = FindShape(m_selectedShapeId))
        {
            if (outIsGenericMove) *outIsGenericMove = m_filled && isFillable(*s);
            return static_cast<int>(s->type);
        }
        return 0;
    }

    if (m_selectedShapeId >= 0)
    {
        const Shape* sel = FindShape(m_selectedShapeId);
        if (sel && sel->finished)
        {
            if (HitTestHandles(*sel, x, y, tol) >= 0) return static_cast<int>(sel->type);

            const float d = DistanceToShapeOutline(*sel, x, y);
            const bool nearOutline = d >= 0.0f && d <= tol;
            const bool inside = m_filled && isFillable(*sel) && IsPointInsideShape(*sel, x, y);
            if (nearOutline || inside)
            {
                if (outIsGenericMove) *outIsGenericMove = m_filled && isFillable(*sel);
                return static_cast<int>(sel->type);
            }
        }
    }
    return 0; // IMG_SHAPE_NONE
}

PointF ShapeManager::ClampToImage(float x, float y) const
{
    PointF p{ x, y };
    if (m_imageWidth > 0.0f)
    {
        if (p.x < 0.0f) p.x = 0.0f;
        else if (p.x > m_imageWidth) p.x = m_imageWidth;
    }
    if (m_imageHeight > 0.0f)
    {
        if (p.y < 0.0f) p.y = 0.0f;
        else if (p.y > m_imageHeight) p.y = m_imageHeight;
    }
    return p;
}

void ShapeManager::ConstrainCircleRadius(Shape& s) const
{
    if (s.type != ShapeType::Circle || m_imageWidth <= 0.0f || m_imageHeight <= 0.0f) return;

    const PointF center = s.points[0];
    const float maxRadius = std::min({ center.x, m_imageWidth - center.x, center.y, m_imageHeight - center.y });
    if (maxRadius < 0.0f) return; // center itself out of bounds (shouldn't happen, it's clamped too) -- nothing sane to do

    const float dx = s.points[1].x - center.x, dy = s.points[1].y - center.y;
    const float radius = std::sqrt(dx * dx + dy * dy);
    if (radius > maxRadius && radius > 1e-6f)
    {
        const float scale = maxRadius / radius;
        s.points[1].x = center.x + dx * scale;
        s.points[1].y = center.y + dy * scale;
    }
}

void ShapeManager::SetImageBounds(float width, float height)
{
    m_imageWidth = width > 0.0f ? width : 0.0f;
    m_imageHeight = height > 0.0f ? height : 0.0f;
}

void ShapeManager::SetCoordinateOrigin(int origin)
{
    m_coordOrigin = (origin == static_cast<int>(CoordOrigin::BottomLeft)) ? CoordOrigin::BottomLeft : CoordOrigin::TopLeft;
}

PointF ShapeManager::ToDisplayCoords(PointF imagePoint) const
{
    if (m_coordOrigin == CoordOrigin::BottomLeft && m_imageHeight > 0.0f)
        return PointF{ imagePoint.x, m_imageHeight - imagePoint.y };
    return imagePoint;
}

std::vector<ShapeLabel> ShapeManager::BuildLabels() const
{
    std::vector<ShapeLabel> labels;
    wchar_t buf[160];

    for (const auto& s : m_shapes)
    {
        if (s.points.size() < 2) continue;

        // Every point of every shape gets its own coordinate label,
        // positioned at the point itself -- this is what shows a Line's
        // start/end point, a Rectangle's corners, a Circle's center+edge,
        // and a Polygon's vertices, uniformly.
        for (const auto& p : s.points)
        {
            const PointF disp = ToDisplayCoords(p);
            swprintf_s(buf, L"(%.0f, %.0f)", disp.x, disp.y);
            labels.push_back({ p, buf });
        }

        // Plus one aggregate summary label per shape type that has one.
        switch (s.type)
        {
        case ShapeType::Rectangle:
        {
            const float w = std::fabs(s.points[1].x - s.points[0].x);
            const float h = std::fabs(s.points[1].y - s.points[0].y);
            swprintf_s(buf, L"W:%.0f H:%.0f", w, h);
            // At the rectangle's own center, not a corner -- keeps it clear
            // of the per-point corner labels above instead of overlapping one.
            const float cx = (s.points[0].x + s.points[1].x) * 0.5f;
            const float cy = (s.points[0].y + s.points[1].y) * 0.5f;
            labels.push_back({ PointF{ cx, cy }, buf });
            break;
        }
        case ShapeType::Circle:
        {
            const float radius = Distance(s.points[0], s.points[1]);
            swprintf_s(buf, L"Diameter: %.0f", radius * 2.0f);
            labels.push_back({ PointF{ s.points[0].x, s.points[0].y - radius }, buf });
            break;
        }
        case ShapeType::Line:
        {
            const float len = Distance(s.points[0], s.points[1]);
            swprintf_s(buf, L"L:%.0f", len);
            labels.push_back({ PointF{ (s.points[0].x + s.points[1].x) * 0.5f, (s.points[0].y + s.points[1].y) * 0.5f }, buf });
            break;
        }
        case ShapeType::Polygon:
            break; // per-vertex labels above already cover it
        }
    }
    return labels;
}

bool ShapeManager::OnMouseDown(float x, float y, float tol, int* outShapeId)
{
    { const PointF c = ClampToImage(x, y); x = c.x; y = c.y; }

    if (outShapeId) *outShapeId = -1;

    // 1. A shape is already selected: check its resize handles first.
    if (m_selectedShapeId >= 0)
    {
        Shape* sel = FindShape(m_selectedShapeId);
        if (sel && sel->finished)
        {
            const int handleIdx = HitTestHandles(*sel, x, y, tol);
            if (handleIdx >= 0)
            {
                m_dragMode = DragMode::ResizingHandle;
                m_resizeHandleIndex = handleIdx;
                if (outShapeId) *outShapeId = sel->id;
                return true;
            }
        }
    }

    // 2. A draw tool is armed.
    if (m_tool == static_cast<int>(ShapeType::Rectangle) || m_tool == static_cast<int>(ShapeType::Circle))
    {
        Shape s;
        s.id = m_nextId++;
        s.type = static_cast<ShapeType>(m_tool);
        s.points = { { x, y }, { x, y } };
        DeselectAll();
        m_shapes.push_back(s);
        m_inProgressShapeId = s.id;
        m_dragMode = DragMode::DrawingRectOrCircle;
        if (outShapeId) *outShapeId = s.id;
        return true;
    }

    if (m_tool == static_cast<int>(ShapeType::Line) || m_tool == static_cast<int>(ShapeType::Polygon))
    {
        if (m_inProgressShapeId < 0)
        {
            Shape s;
            s.id = m_nextId++;
            s.type = static_cast<ShapeType>(m_tool);
            s.points = { { x, y } };
            DeselectAll();
            m_shapes.push_back(s);
            m_inProgressShapeId = s.id;
            m_dragMode = DragMode::DrawingPolyOrLine;
            m_hasLivePreview = false;
            if (outShapeId) *outShapeId = s.id;
            return true;
        }

        Shape* s = FindShape(m_inProgressShapeId);
        if (!s) { m_inProgressShapeId = -1; m_dragMode = DragMode::None; return false; }

        if (s->type == ShapeType::Line)
        {
            s->points.push_back({ x, y });
            s->finished = true;
            s->selected = true;
            m_selectedShapeId = s->id;
            m_inProgressShapeId = -1;
            m_dragMode = DragMode::None;
            m_hasLivePreview = false;
            m_tool = 0; // auto-revert to selection mode
            if (outShapeId) *outShapeId = s->id;
            return true;
        }

        // Polygon: closing click -- near the first vertex, with enough points already.
        if (s->points.size() >= 3 && Distance(s->points[0], { x, y }) <= tol)
        {
            s->finished = true;
            s->selected = true;
            m_selectedShapeId = s->id;
            m_inProgressShapeId = -1;
            m_dragMode = DragMode::None;
            m_hasLivePreview = false;
            m_tool = 0;
            if (outShapeId) *outShapeId = s->id;
            return true;
        }

        s->points.push_back({ x, y });
        if (outShapeId) *outShapeId = s->id;
        return true;
    }

    // 3. Selection mode: hit-test existing finished shapes. A hit both
    // selects the shape AND immediately arms a move-drag, so a single
    // click-and-drag selects and moves it in one motion (same as most
    // drawing apps); clicking and releasing without moving just leaves it
    // selected, same as before.
    DeselectAll();
    const int hit = HitTestShapes(x, y, tol);
    if (hit >= 0)
    {
        FindShape(hit)->selected = true;
        m_selectedShapeId = hit;
        m_dragMode = DragMode::MovingShape;
        m_moveLastPoint = PointF{ x, y };
        if (outShapeId) *outShapeId = hit;
        return true;
    }
    return false; // empty space: caller should fall through to image panning
}

void ShapeManager::OnMouseMove(float x, float y, bool* outChanged)
{
    { const PointF c = ClampToImage(x, y); x = c.x; y = c.y; }

    if (outChanged) *outChanged = false;

    if (m_dragMode == DragMode::DrawingRectOrCircle && m_inProgressShapeId >= 0)
    {
        if (Shape* s = FindShape(m_inProgressShapeId))
        {
            s->points[1] = { x, y };
            ConstrainCircleRadius(*s);
            if (outChanged) *outChanged = true;
        }
    }
    else if (m_dragMode == DragMode::DrawingPolyOrLine && m_inProgressShapeId >= 0)
    {
        m_livePreviewPoint = { x, y };
        m_hasLivePreview = true;
        if (outChanged) *outChanged = true;
    }
    else if (m_dragMode == DragMode::ResizingHandle && m_selectedShapeId >= 0)
    {
        if (Shape* s = FindShape(m_selectedShapeId))
        {
            ApplyHandleDrag(*s, m_resizeHandleIndex, x, y);
            if (outChanged) *outChanged = true;
        }
    }
    else if (m_dragMode == DragMode::MovingShape && m_selectedShapeId >= 0)
    {
        if (Shape* s = FindShape(m_selectedShapeId))
        {
            float dx = x - m_moveLastPoint.x;
            float dy = y - m_moveLastPoint.y;

            // Clamp the DELTA (not each point individually) against the
            // shape's own bounding box, so the move just stops exactly at
            // the image edge instead of distorting the shape (which would
            // happen if one point got clamped while others kept moving).
            if (m_imageWidth > 0.0f && m_imageHeight > 0.0f)
            {
                float left, top, right, bottom;
                ComputeShapeBounds(*s, left, top, right, bottom);

                const float minDx = -left, maxDx = m_imageWidth - right;
                dx = (minDx <= maxDx) ? std::min(std::max(dx, minDx), maxDx) : 0.0f;
                const float minDy = -top, maxDy = m_imageHeight - bottom;
                dy = (minDy <= maxDy) ? std::min(std::max(dy, minDy), maxDy) : 0.0f;
            }

            for (auto& p : s->points) { p.x += dx; p.y += dy; }
            m_moveLastPoint.x += dx;
            m_moveLastPoint.y += dy;
            if (outChanged) *outChanged = true;
        }
    }
}

void ShapeManager::OnMouseUp(float x, float y, bool* outChanged)
{
    { const PointF c = ClampToImage(x, y); x = c.x; y = c.y; }

    if (outChanged) *outChanged = false;

    if (m_dragMode == DragMode::DrawingRectOrCircle && m_inProgressShapeId >= 0)
    {
        if (Shape* s = FindShape(m_inProgressShapeId))
        {
            s->points[1] = { x, y };
            ConstrainCircleRadius(*s);
            const bool tooSmall = Distance(s->points[0], s->points[1]) < 2.0f; // discard accidental click-without-drag
            const int id = s->id;
            if (tooSmall)
            {
                m_shapes.erase(std::remove_if(m_shapes.begin(), m_shapes.end(),
                    [&](const Shape& sh) { return sh.id == id; }), m_shapes.end());
            }
            else
            {
                s->finished = true;
                s->selected = true;
                m_selectedShapeId = id;
            }
        }
        m_inProgressShapeId = -1;
        m_dragMode = DragMode::None;
        m_tool = 0; // auto-revert to selection mode
        if (outChanged) *outChanged = true;
    }
    else if (m_dragMode == DragMode::ResizingHandle)
    {
        m_dragMode = DragMode::None;
        m_resizeHandleIndex = -1;
        if (outChanged) *outChanged = true;
    }
    else if (m_dragMode == DragMode::MovingShape)
    {
        m_dragMode = DragMode::None;
        if (outChanged) *outChanged = true;
    }
    // Line/Polygon finalize on a click (OnMouseDown), not on mouse-up.
}

bool ShapeManager::OnKeyDown(int vkCode)
{
    if (vkCode == VK_DELETE) return DeleteSelected();
    return false;
}

bool ShapeManager::DeleteSelected()
{
    if (m_selectedShapeId < 0) return false;
    const int id = m_selectedShapeId;
    const size_t before = m_shapes.size();
    m_shapes.erase(std::remove_if(m_shapes.begin(), m_shapes.end(),
        [&](const Shape& s) { return s.id == id; }), m_shapes.end());
    const bool removed = m_shapes.size() != before;
    if (removed)
    {
        if (m_inProgressShapeId == id) { m_inProgressShapeId = -1; m_dragMode = DragMode::None; }
        m_selectedShapeId = -1;
    }
    return removed;
}

void ShapeManager::Clear()
{
    m_shapes.clear();
    m_tool = 0;
    m_nextId = 1;
    m_selectedShapeId = -1;
    m_dragMode = DragMode::None;
    m_inProgressShapeId = -1;
    m_hasLivePreview = false;
    m_resizeHandleIndex = -1;
}

std::vector<ShapeRenderEntry> ShapeManager::BuildRenderData() const
{
    std::vector<ShapeRenderEntry> result;
    result.reserve(m_shapes.size());

    for (const auto& s : m_shapes)
    {
        ShapeRenderEntry e;
        e.selected = s.selected && s.finished;
        e.inProgress = !s.finished;

        switch (s.type)
        {
        case ShapeType::Rectangle:
        {
            const float l = std::min(s.points[0].x, s.points[1].x);
            const float r = std::max(s.points[0].x, s.points[1].x);
            const float t = std::min(s.points[0].y, s.points[1].y);
            const float b = std::max(s.points[0].y, s.points[1].y);
            e.outline = { { l, t }, { r, t }, { r, b }, { l, b } };
            e.closed = true;
            break;
        }
        case ShapeType::Circle:
        {
            const float cx = s.points[0].x, cy = s.points[0].y;
            const float radius = Distance(s.points[0], s.points[1]);
            constexpr int kSegments = 48;
            e.outline.reserve(kSegments);
            for (int i = 0; i < kSegments; ++i)
            {
                const float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(kSegments);
                e.outline.push_back({ cx + radius * std::cos(angle), cy + radius * std::sin(angle) });
            }
            e.closed = true;
            e.hasCenterMarker = true;
            e.centerMarker = s.points[0];
            break;
        }
        case ShapeType::Line:
            e.outline = s.points;
            e.closed = false;
            if (!s.finished && m_hasLivePreview && s.id == m_inProgressShapeId)
                e.outline.push_back(m_livePreviewPoint);
            break;
        case ShapeType::Polygon:
            e.outline = s.points;
            e.closed = s.finished;
            if (!s.finished && m_hasLivePreview && s.id == m_inProgressShapeId)
                e.outline.push_back(m_livePreviewPoint);
            break;
        }

        if (e.selected)
            GetHandleCenters(s, e.handles);

        e.wantFill = m_filled && e.closed; // open shapes (Line, an in-progress Polygon) never fill

        result.push_back(std::move(e));
    }
    return result;
}
