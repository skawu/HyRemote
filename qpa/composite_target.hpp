#pragma once

#include "application_surface_model.hpp"
#include "detail/target_component_provider.hpp"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QVector>

#include <optional>

namespace HyRemote::Qpa {

struct CompositeSurfaceSnapshot
{
    SurfaceId id = 0;
    QPointer<QObject> target;
    QRect globalGeometry;
};

struct CompositeTargetSnapshot
{
    QRect canvasBounds;
    QVector<CompositeSurfaceSnapshot> backToFront;
};

struct CompositeRoutedPoint
{
    CompositeSurfaceSnapshot surface;
    QPoint localPosition;
};

// QPA-private QObject target presented to the normal RemoteAccess facade.
//
// It is not a public application target type. RemoteAccess discovers the private
// TargetComponentProvider interface and obtains one composite CaptureSource/InputSink while still
// owning the normal Session/Transport lifecycle. The target/model may change while that Session
// stays alive.
class CompositeTarget : public QObject, public ::HyRemote::detail::TargetComponentProvider
{
public:
    explicit CompositeTarget(QObject *parent = nullptr);

    void upsertSurface(SurfaceId id, QObject *target, const QRect &globalGeometry, bool visible);
    void removeSurface(SurfaceId id);
    void setSurfaceVisible(SurfaceId id, bool visible);
    void setSurfaceGeometry(SurfaceId id, const QRect &globalGeometry);
    void raiseSurface(SurfaceId id);
    void setActiveSurface(SurfaceId id);
    void clearActiveSurface();

    CompositeTargetSnapshot captureSnapshot() const;
    std::optional<CompositeSurfaceSnapshot> surfaceById(SurfaceId id) const;
    std::optional<CompositeSurfaceSnapshot> activeSurface() const;
    std::optional<CompositeRoutedPoint> routeCanvasPoint(const QPoint &canvasPosition) const;

    ::HyRemote::detail::TargetComponents createTargetComponents(
        bool remoteInputEnabled,
        const ::HyRemote::detail::BuiltinTargetResolver &resolveBuiltinTarget) override;

private:
    ApplicationSurfaceModel m_model;
    QHash<SurfaceId, QPointer<QObject>> m_targets;
    std::optional<SurfaceId> m_activeSurface;
};

}  // namespace HyRemote::Qpa
