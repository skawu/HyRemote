#pragma once

#include <QPoint>
#include <QRect>
#include <QVector>
#include <QtGlobal>

#include <optional>

namespace HyRemote::Qpa {

using SurfaceId = quint64;

struct SurfaceRecord
{
    SurfaceId id = 0;
    QRect globalGeometry;
    bool visible = false;
    quint64 stackSerial = 0;
};

struct RoutedPoint
{
    SurfaceId surfaceId = 0;
    QPoint localPosition;
};

// Deterministic application-surface geometry/stacking model for Transparent QPA Proxy mode.
//
// The model knows nothing about RFB, capture backends, QPA private types or transport lifecycle. It
// converts the set of visible application-owned top-level surfaces into one logical remote canvas
// and maps canvas-relative pointer coordinates back to the topmost eligible surface.
class ApplicationSurfaceModel
{
public:
    void upsert(SurfaceId id, const QRect &globalGeometry, bool visible);
    void remove(SurfaceId id);
    void setVisible(SurfaceId id, bool visible);
    void setGeometry(SurfaceId id, const QRect &globalGeometry);
    void raise(SurfaceId id);

    bool contains(SurfaceId id) const;
    bool empty() const noexcept { return m_surfaces.isEmpty(); }
    int size() const noexcept { return m_surfaces.size(); }

    QRect canvasBounds() const;
    QVector<SurfaceRecord> visibleBackToFront() const;
    std::optional<RoutedPoint> routeCanvasPoint(const QPoint &canvasPosition) const;

private:
    SurfaceRecord *find(SurfaceId id);
    const SurfaceRecord *find(SurfaceId id) const;
    quint64 nextStackSerial();

    QVector<SurfaceRecord> m_surfaces;
    quint64 m_nextStackSerial = 1;
};

}  // namespace HyRemote::Qpa
