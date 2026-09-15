#pragma once

#include "application_surface_model.hpp"
#include "detail/target_component_provider.hpp"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QVector>

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

// QPA-private QObject target presented to the normal RemoteAccess facade.
//
// It is not a public application target type. RemoteAccess discovers the private
// TargetComponentProvider interface and obtains one composite CaptureSource while still owning the
// normal Session/Transport lifecycle. The target/model may change while that Session stays alive.
class CompositeTarget final : public QObject, public ::HyRemote::detail::TargetComponentProvider
{
public:
    explicit CompositeTarget(QObject *parent = nullptr);

    void upsertSurface(SurfaceId id, QObject *target, const QRect &globalGeometry, bool visible);
    void removeSurface(SurfaceId id);
    void setSurfaceVisible(SurfaceId id, bool visible);
    void setSurfaceGeometry(SurfaceId id, const QRect &globalGeometry);
    void raiseSurface(SurfaceId id);

    CompositeTargetSnapshot captureSnapshot() const;

    ::HyRemote::detail::TargetComponents createTargetComponents(
        bool remoteInputEnabled,
        const ::HyRemote::detail::BuiltinTargetResolver &resolveBuiltinTarget) override;

private:
    ApplicationSurfaceModel m_model;
    QHash<SurfaceId, QPointer<QObject>> m_targets;
};

}  // namespace HyRemote::Qpa
