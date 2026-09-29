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

namespace HyRemote::Runtime::Automatic {

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

// Runtime-private QObject target shared by application-surface compositions. TargetComponentProvider
// yields one composite CaptureSource/InputSink while the target/model may change without replacing
// the underlying Session or transport listener.
class CompositeTarget : public QObject, public ::HyRemote::detail::TargetComponentProvider
{
public:
    using SurfaceUnavailableHandler = std::function<void(SurfaceId)>;
    using SurfaceRefreshHandler = std::function<void()>;

    explicit CompositeTarget(QObject *parent = nullptr);

    void upsertSurface(SurfaceId id, QObject *target, const QRect &globalGeometry, bool visible);
    void removeSurface(SurfaceId id);
    void setSurfaceVisible(SurfaceId id, bool visible);
    void setSurfaceGeometry(SurfaceId id, const QRect &globalGeometry);
    void raiseSurface(SurfaceId id);
    void setActiveSurface(SurfaceId id);
    void clearActiveSurface();

    // Optional Runtime-private hook for bounded compositions whose surface membership is derived
    // from current Qt ownership. It runs on the same GUI-thread snapshot boundary used by capture
    // and composite input, so no second event-driven surface state machine is required.
    void setSurfaceRefreshHandler(SurfaceRefreshHandler handler);

    CompositeTargetSnapshot captureSnapshot() const;
    std::optional<CompositeSurfaceSnapshot> surfaceById(SurfaceId id) const;
    std::optional<CompositeSurfaceSnapshot> activeSurface() const;
    std::optional<CompositeRoutedPoint> routeCanvasPoint(const QPoint &canvasPosition) const;

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
    SurfaceRefreshHandler m_surfaceRefreshHandler;
    QHash<quint64, SurfaceUnavailableHandler> m_surfaceUnavailableHandlers;
    quint64 m_nextSurfaceUnavailableHandler = 1;
};

}  // namespace HyRemote::Runtime::Automatic
