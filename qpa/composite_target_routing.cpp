#include "composite_target.hpp"

#include <algorithm>

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
    const CompositeTargetSnapshot snapshot = captureSnapshot();
    if (m_activeSurface) {
        for (const CompositeSurfaceSnapshot &surface : snapshot.backToFront) {
            if (surface.id == *m_activeSurface)
                return surface;
        }
    }

    if (snapshot.backToFront.isEmpty())
        return std::nullopt;
    return snapshot.backToFront.back();
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
