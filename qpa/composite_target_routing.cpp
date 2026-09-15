#include "composite_target.hpp"

namespace HyRemote::Qpa {

void CompositeTarget::setActiveSurface(SurfaceId id)
{
    m_activeSurface = id;
}

void CompositeTarget::clearActiveSurface()
{
    m_activeSurface.reset();
}

std::optional<CompositeSurfaceSnapshot> CompositeTarget::surfaceById(SurfaceId id) const
{
    const CompositeTargetSnapshot snapshot = captureSnapshot();
    for (const CompositeSurfaceSnapshot &surface : snapshot.backToFront) {
        if (surface.id == id)
            return surface;
    }
    return std::nullopt;
}

std::optional<CompositeSurfaceSnapshot> CompositeTarget::activeSurface() const
{
    if (!m_activeSurface)
        return std::nullopt;

    // Keyboard/text routing must follow real application activation/focus state supplied by the
    // QPA controller. Stacking order is not a substitute for focus, so a hidden/removed active
    // surface yields no keyboard target until Qt reports another active surface.
    return surfaceById(*m_activeSurface);
}

std::optional<CompositeRoutedPoint> CompositeTarget::routeCanvasPoint(
    const QPoint &canvasPosition) const
{
    const CompositeTargetSnapshot snapshot = captureSnapshot();
    if (snapshot.canvasBounds.isEmpty())
        return std::nullopt;

    const QRect localCanvas(QPoint(0, 0), snapshot.canvasBounds.size());
    if (!localCanvas.contains(canvasPosition))
        return std::nullopt;

    const QPoint globalPosition = snapshot.canvasBounds.topLeft() + canvasPosition;
    for (auto it = snapshot.backToFront.crbegin(); it != snapshot.backToFront.crend(); ++it) {
        if (!it->globalGeometry.contains(globalPosition))
            continue;
        CompositeRoutedPoint result;
        result.surface = *it;
        result.localPosition = globalPosition - it->globalGeometry.topLeft();
        return result;
    }
    return std::nullopt;
}

}  // namespace HyRemote::Qpa
