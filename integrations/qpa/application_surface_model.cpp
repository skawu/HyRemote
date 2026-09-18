// SPDX-License-Identifier: Apache-2.0
#include "application_surface_model.hpp"

#include <algorithm>
#include <limits>

namespace HyRemote::Qpa {

void ApplicationSurfaceModel::upsert(SurfaceId id, const QRect &globalGeometry, bool visible)
{
    if (SurfaceRecord *surface = find(id)) {
        surface->globalGeometry = globalGeometry;
        const bool becameVisible = visible && !surface->visible;
        surface->visible = visible;
        if (becameVisible)
            surface->stackSerial = nextStackSerial();
        return;
    }

    SurfaceRecord surface;
    surface.id = id;
    surface.globalGeometry = globalGeometry;
    surface.visible = visible;
    surface.stackSerial = visible ? nextStackSerial() : 0;
    m_surfaces.push_back(surface);
}

void ApplicationSurfaceModel::remove(SurfaceId id)
{
    const auto it = std::remove_if(m_surfaces.begin(), m_surfaces.end(),
                                   [id](const SurfaceRecord &surface) { return surface.id == id; });
    m_surfaces.erase(it, m_surfaces.end());
}

void ApplicationSurfaceModel::setVisible(SurfaceId id, bool visible)
{
    SurfaceRecord *surface = find(id);
    if (!surface || surface->visible == visible)
        return;

    surface->visible = visible;
    if (visible)
        surface->stackSerial = nextStackSerial();
}

void ApplicationSurfaceModel::setGeometry(SurfaceId id, const QRect &globalGeometry)
{
    if (SurfaceRecord *surface = find(id))
        surface->globalGeometry = globalGeometry;
}

void ApplicationSurfaceModel::raise(SurfaceId id)
{
    SurfaceRecord *surface = find(id);
    if (!surface || !surface->visible)
        return;
    surface->stackSerial = nextStackSerial();
}

bool ApplicationSurfaceModel::contains(SurfaceId id) const
{
    return find(id) != nullptr;
}

QRect ApplicationSurfaceModel::canvasBounds() const
{
    QRect bounds;
    bool haveVisibleSurface = false;
    for (const SurfaceRecord &surface : m_surfaces) {
        if (!surface.visible || surface.globalGeometry.isEmpty())
            continue;
        if (!haveVisibleSurface) {
            bounds = surface.globalGeometry;
            haveVisibleSurface = true;
        } else {
            bounds = bounds.united(surface.globalGeometry);
        }
    }
    return haveVisibleSurface ? bounds : QRect{};
}

QVector<SurfaceRecord> ApplicationSurfaceModel::visibleBackToFront() const
{
    QVector<SurfaceRecord> visible;
    visible.reserve(m_surfaces.size());
    for (const SurfaceRecord &surface : m_surfaces) {
        if (surface.visible && !surface.globalGeometry.isEmpty())
            visible.push_back(surface);
    }

    std::sort(visible.begin(), visible.end(), [](const SurfaceRecord &left, const SurfaceRecord &right) {
        if (left.stackSerial != right.stackSerial)
            return left.stackSerial < right.stackSerial;
        return left.id < right.id;
    });
    return visible;
}

std::optional<RoutedPoint> ApplicationSurfaceModel::routeCanvasPoint(const QPoint &canvasPosition) const
{
    const QRect bounds = canvasBounds();
    if (bounds.isEmpty() || !QRect(QPoint{0, 0}, bounds.size()).contains(canvasPosition))
        return std::nullopt;

    const QPoint globalPosition = bounds.topLeft() + canvasPosition;
    const QVector<SurfaceRecord> surfaces = visibleBackToFront();
    for (auto it = surfaces.crbegin(); it != surfaces.crend(); ++it) {
        if (!it->globalGeometry.contains(globalPosition))
            continue;
        return RoutedPoint{it->id, globalPosition - it->globalGeometry.topLeft()};
    }
    return std::nullopt;
}

SurfaceRecord *ApplicationSurfaceModel::find(SurfaceId id)
{
    const auto it = std::find_if(m_surfaces.begin(), m_surfaces.end(),
                                 [id](const SurfaceRecord &surface) { return surface.id == id; });
    return it == m_surfaces.end() ? nullptr : &*it;
}

const SurfaceRecord *ApplicationSurfaceModel::find(SurfaceId id) const
{
    const auto it = std::find_if(m_surfaces.cbegin(), m_surfaces.cend(),
                                 [id](const SurfaceRecord &surface) { return surface.id == id; });
    return it == m_surfaces.cend() ? nullptr : &*it;
}

quint64 ApplicationSurfaceModel::nextStackSerial()
{
    // Wraparound is practically unreachable, but keeping 0 reserved for never-visible records makes
    // behavior deterministic even if an extreme synthetic stress test reaches the integer limit.
    if (m_nextStackSerial == std::numeric_limits<quint64>::max()) {
        QVector<SurfaceRecord> ordered = visibleBackToFront();
        quint64 serial = 1;
        for (const SurfaceRecord &entry : ordered) {
            if (SurfaceRecord *surface = find(entry.id))
                surface->stackSerial = serial++;
        }
        m_nextStackSerial = serial;
    }
    return m_nextStackSerial++;
}

}  // namespace HyRemote::Qpa
