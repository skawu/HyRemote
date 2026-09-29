#include "widgets/widget_surface_scope.hpp"

#include "automatic/interactive_composite_target.hpp"
#include "widgets/widget_target.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QHash>
#include <QImage>
#include <QMetaObject>
#include <QPainter>
#include <QPointer>
#include <QSet>
#include <QThread>
#include <QWidget>
#include <QWindow>
#include <QtMath>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/frame.hpp"
#include "hyremote/core/storage.hpp"

namespace HyRemote::detail {
namespace {

using Runtime::Automatic::CompositeSurfaceSnapshot;
using Runtime::Automatic::CompositeTargetSnapshot;
using Runtime::Automatic::InteractiveCompositeTarget;
using Runtime::Automatic::SurfaceId;

struct SurfaceRegistry
{
    QHash<QWidget *, SurfaceId> ids;
    SurfaceId nextId = 1;
};

bool eligibleTransientType(Qt::WindowType type)
{
    return type != Qt::Desktop && type != Qt::SplashScreen && type != Qt::ToolTip
           && type != Qt::ForeignWindow;
}

bool widgetParentChainReaches(QWidget *candidate, QWidget *root)
{
    for (QWidget *parent = candidate ? candidate->parentWidget() : nullptr;
         parent;
         parent = parent->parentWidget()) {
        if (parent == root)
            return true;
    }
    return false;
}

bool isOwnedTransient(QWidget *candidate, QWidget *root)
{
    if (!candidate || !root || candidate == root || !candidate->isWindow()
        || !eligibleTransientType(candidate->windowType())) {
        return false;
    }

    // A top-level QWidget may still have a QWidget parent. This is the strongest public ownership
    // relation for popups produced from a configured subtree and works even when the configured
    // target itself is not a top-level window.
    if (widgetParentChainReaches(candidate, root))
        return true;

    // Only a configured top-level may widen admission through QWindow parent/transient ancestry.
    // A child-widget target must never inherit unrelated transients merely because they share its
    // containing top-level window.
    if (!root->isWindow())
        return false;

    QWindow *rootWindow = root->windowHandle();
    QWindow *candidateWindow = candidate->windowHandle();
    return rootWindow && candidateWindow
           && rootWindow->isAncestorOf(candidateWindow, QWindow::IncludeTransients);
}

SurfaceId idFor(QWidget *widget, const std::shared_ptr<SurfaceRegistry> &registry)
{
    SurfaceId id = registry->ids.value(widget, 0);
    if (id == 0) {
        id = registry->nextId++;
        registry->ids.insert(widget, id);
    }
    return id;
}

void refreshScopedSurfaces(InteractiveCompositeTarget *composite,
                           const QPointer<QWidget> &root,
                           const std::shared_ptr<SurfaceRegistry> &registry)
{
    QWidget *rootWidget = root.data();
    if (!composite)
        return;

    if (!rootWidget) {
        const QList<SurfaceId> ids = registry->ids.values();
        for (SurfaceId id : ids)
            composite->removeSurface(id);
        registry->ids.clear();
        composite->clearActiveSurface();
        return;
    }

    QSet<QWidget *> live;
    live.insert(rootWidget);
    const SurfaceId rootId = idFor(rootWidget, registry);
    composite->upsertSurface(rootId,
                             rootWidget,
                             QRect(rootWidget->mapToGlobal(QPoint(0, 0)), rootWidget->size()),
                             rootWidget->width() > 0 && rootWidget->height() > 0);

    for (QWidget *candidate : QApplication::topLevelWidgets()) {
        if (!candidate || candidate == rootWidget || !candidate->isVisible()
            || candidate->isMinimized() || candidate->width() <= 0 || candidate->height() <= 0
            || !isOwnedTransient(candidate, rootWidget)) {
            continue;
        }

        live.insert(candidate);
        const SurfaceId id = idFor(candidate, registry);
        composite->upsertSurface(id,
                                 candidate,
                                 QRect(candidate->mapToGlobal(QPoint(0, 0)), candidate->size()),
                                 true);
    }

    for (auto it = registry->ids.begin(); it != registry->ids.end();) {
        QWidget *candidate = it.key();
        if (live.contains(candidate)) {
            ++it;
            continue;
        }
        composite->removeSurface(it.value());
        it = registry->ids.erase(it);
    }

    QWidget *active = QApplication::activePopupWidget();
    if (!active || !live.contains(active))
        active = QApplication::activeWindow();
    if (!active || !live.contains(active))
        active = rootWidget;

    const SurfaceId activeId = registry->ids.value(active, rootId);
    composite->setActiveSurface(activeId);
    if (active != rootWidget)
        composite->raiseSurface(activeId);
}

class ScopedWidgetCaptureSource final : public hyremote::CaptureSource
{
public:
    ScopedWidgetCaptureSource(std::shared_ptr<InteractiveCompositeTarget> composite,
                              QWidget *root)
        : m_state(std::make_shared<State>())
    {
        m_state->composite = std::move(composite);
        m_state->root = root;
    }

    hyremote::CaptureCapabilities capabilities() const override
    {
        hyremote::CaptureCapabilities result;
        result.asynchronous = true;
        result.regionDamage = false;
        result.cpuReadable = true;
        result.cpuFormats = {hyremote::PixelFormat::Rgba8888};
        return result;
    }

    bool start(hyremote::FrameReadyHandler onFrame,
               hyremote::CaptureEventHandler onEvent) override
    {
        QCoreApplication *application = QCoreApplication::instance();
        if (!application || m_state->root.isNull() || !m_state->composite
            || m_state->root->thread() != application->thread()
            || m_state->composite->thread() != application->thread()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_state->mutex);
        if (m_state->active)
            return false;
        m_state->onFrame = std::move(onFrame);
        m_state->onEvent = std::move(onEvent);
        m_state->active = true;
        return true;
    }

    void stop() noexcept override
    {
        std::unique_lock<std::mutex> lock(m_state->mutex);
        m_state->active = false;
        m_state->onFrame = {};
        m_state->onEvent = {};
        m_state->callbacksDrained.wait(lock, [state = m_state] {
            return state->callbacksInFlight == 0;
        });
    }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            if (!m_state->active)
                return false;
        }

        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher)
            return false;
        const std::shared_ptr<State> state = m_state;
        return QMetaObject::invokeMethod(
            dispatcher,
            [state, request] { captureOnGuiThread(state, request); },
            Qt::QueuedConnection);
    }

private:
    struct State
    {
        std::mutex mutex;
        std::condition_variable callbacksDrained;
        std::shared_ptr<InteractiveCompositeTarget> composite;
        QPointer<QWidget> root;
        bool active = false;
        std::size_t callbacksInFlight = 0;
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
    };

    template <typename Handler, typename Payload>
    static void publish(const std::shared_ptr<State> &state,
                        Handler State::*member,
                        Payload payload)
    {
        Handler callback;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
                return;
            callback = state.get()->*member;
            if (!callback)
                return;
            ++state->callbacksInFlight;
        }
        try {
            callback(std::move(payload));
        } catch (...) {
        }
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            --state->callbacksInFlight;
            if (state->callbacksInFlight == 0)
                state->callbacksDrained.notify_all();
        }
    }

    static void publishEvent(const std::shared_ptr<State> &state,
                             hyremote::CaptureEventCode code,
                             const char *message,
                             bool recoverable)
    {
        hyremote::CaptureEvent event;
        event.code = code;
        event.message = message;
        event.recoverable = recoverable;
        publish(state, &State::onEvent, std::move(event));
    }

    static void captureOnGuiThread(const std::shared_ptr<State> &state,
                                   const hyremote::CaptureRequest &request)
    {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
                return;
        }

        QWidget *root = state->root.data();
        if (!root) {
            publishEvent(state,
                         hyremote::CaptureEventCode::TargetLost,
                         "the configured QWidget target was destroyed",
                         false);
            return;
        }

        state->composite->refreshSurfaces();
        const CompositeTargetSnapshot snapshot = state->composite->captureSnapshot();
        if (snapshot.canvasBounds.isEmpty()) {
            publishEvent(state,
                         hyremote::CaptureEventCode::TemporarilyUnavailable,
                         "the configured QWidget target has no capturable geometry",
                         true);
            return;
        }

        const qreal dpr = qMax<qreal>(1.0, root->devicePixelRatioF());
        const int pixelWidth = qMax(1, qCeil(snapshot.canvasBounds.width() * dpr));
        const int pixelHeight = qMax(1, qCeil(snapshot.canvasBounds.height() * dpr));
        const std::size_t bytesPerLine = static_cast<std::size_t>(pixelWidth) * 4U;
        auto storage = hyremote::CpuFrameStorage::createSinglePlane(
            bytesPerLine, static_cast<std::size_t>(pixelHeight));
        if (!storage || !storage->mutablePlane(0)) {
            publishEvent(state,
                         hyremote::CaptureEventCode::BackendFailure,
                         "failed to allocate scoped QWidget frame storage",
                         false);
            return;
        }

        QImage image(reinterpret_cast<uchar *>(storage->mutablePlane(0)),
                     pixelWidth,
                     pixelHeight,
                     static_cast<qsizetype>(bytesPerLine),
                     QImage::Format_RGBA8888_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        for (const CompositeSurfaceSnapshot &surface : snapshot.backToFront) {
            QWidget *widget = qobject_cast<QWidget *>(surface.target.data());
            if (!widget)
                continue;
            widget->render(&painter,
                           surface.globalGeometry.topLeft() - snapshot.canvasBounds.topLeft());
        }
        painter.end();

        hyremote::RemoteFrame frame;
        frame.geometry.size = {static_cast<std::uint32_t>(pixelWidth),
                               static_cast<std::uint32_t>(pixelHeight)};
        frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
        frame.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
        frame.geometry.planeCount = 1;
        frame.storage = std::move(storage);
        frame.timing.ptsSource = hyremote::PtsSource::Completion;
        frame.timing.requestTime = request.requestTime;
        frame.timing.completionTime = hyremote::Clock::now();
        frame.damage = hyremote::Damage::fullFrame();
        frame.requestId = request.id;
        publish(state, &State::onFrame, std::move(frame));
    }

    std::shared_ptr<State> m_state;
};

class LifetimeInputSink final : public hyremote::InputSink
{
public:
    LifetimeInputSink(std::shared_ptr<InteractiveCompositeTarget> composite,
                      std::shared_ptr<hyremote::InputSink> inner)
        : m_composite(std::move(composite))
        , m_inner(std::move(inner))
    {
    }

    void post(const hyremote::InputEvent &event) override { m_inner->post(event); }
    void shutdown() noexcept override { m_inner->shutdown(); }

private:
    // Declared first so the inner sink is destroyed before the QObject it references.
    std::shared_ptr<InteractiveCompositeTarget> m_composite;
    std::shared_ptr<hyremote::InputSink> m_inner;
};

std::shared_ptr<InteractiveCompositeTarget> makeCompositeTarget(QWidget *root)
{
    QCoreApplication *application = QCoreApplication::instance();
    if (!application)
        return {};

    auto *raw = new InteractiveCompositeTarget;
    if (raw->thread() != application->thread())
        raw->moveToThread(application->thread());

    const auto deleter = [](InteractiveCompositeTarget *target) {
        if (!target)
            return;
        if (QThread::currentThread() == target->thread()) {
            delete target;
            return;
        }
        (void)QMetaObject::invokeMethod(target, "deleteLater", Qt::QueuedConnection);
    };
    std::shared_ptr<InteractiveCompositeTarget> composite(raw, deleter);

    auto registry = std::make_shared<SurfaceRegistry>();
    const QPointer<QWidget> guardedRoot(root);
    raw->setSurfaceRefreshHandler([raw, guardedRoot, registry] {
        refreshScopedSurfaces(raw, guardedRoot, registry);
    });
    return composite;
}

}  // namespace

TargetComponents createScopedWidgetsTargetComponents(QObject *target, bool remoteInputEnabled)
{
    TargetComponents result;
    auto *widget = qobject_cast<QWidget *>(target);
    if (!widget)
        return result;

    std::shared_ptr<InteractiveCompositeTarget> composite = makeCompositeTarget(widget);
    if (!composite) {
        result.error = QStringLiteral("Qt application event dispatcher is unavailable");
        return result;
    }

    result.supported = true;
    result.capture = std::make_unique<ScopedWidgetCaptureSource>(composite, widget);

    if (remoteInputEnabled) {
        const BuiltinTargetResolver leafResolver = [](QObject *child, bool childRemoteInputEnabled) {
            return createWidgetsTargetComponents(child, childRemoteInputEnabled);
        };
        TargetComponents compositeComponents =
            composite->createTargetComponents(true, leafResolver);
        if (!compositeComponents.input) {
            result.supported = false;
            result.capture.reset();
            result.error = compositeComponents.error.isEmpty()
                               ? QStringLiteral("failed to construct scoped QWidget input routing")
                               : compositeComponents.error;
            return result;
        }
        result.input = std::make_shared<LifetimeInputSink>(composite,
                                                           std::move(compositeComponents.input));
    }

    return result;
}

}  // namespace HyRemote::detail
