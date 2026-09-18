// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "application_surface_model.hpp"
#include "detail/target_component_provider.hpp"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QVector>

#include <functional>
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
    using SurfaceUnavailableHandler = std::function<void(SurfaceId)>;

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

    // Internal capture-orchestration hook. A surface leaving the visible composite must release
    // any already-admitted composite request that was waiting for that child. This does not stop or
    // recreate RemoteAccess/Session/Transport; it only changes the child expectation of in-flight
    // composite capture work.
    quint64 addSurfaceUnavailableHandler(SurfaceUnavailableHandler handler);
    void removeSurfaceUnavailableHandler(quint64 token);

    ::HyRemote::detail::TargetComponents createTargetComponents(
        bool remoteInputEnabled,
        const ::HyRemote::detail::BuiltinTargetResolver &resolveBuiltinTarget) override;

private:
    void notifySurfaceUnavailable(SurfaceId id);

    ApplicationSurfaceModel m_model;
    QHash<SurfaceId, QPointer<QObject>> m_targets;
    std::optional<SurfaceId> m_activeSurface;
    QHash<quint64, SurfaceUnavailableHandler> m_surfaceUnavailableHandlers;
    quint64 m_nextSurfaceUnavailableHandler = 1;
};

}  // namespace HyRemote::Qpa
