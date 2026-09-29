#include "widgets/widget_surface_scope.hpp"

#include "automatic/application_surface_model.hpp"
#include "detail/input_mailbox_admission.hpp"
#include "detail/qt_window_system_input.hpp"
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
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace HyRemote::detail {
namespace {

using Runtime::Automatic::ApplicationSurfaceModel;
using Runtime::Automatic::RoutedPoint;
using Runtime::Automatic::SurfaceId;
using Runtime::Automatic::SurfaceRecord;

struct ScopedSurface
{
    SurfaceId id = 0;
    QPointer<QWidget> widget;
    QRect globalGeometry;
};

struct ScopedRoutedPoint
{
    ScopedSurface surface;
    QPoint localPosition;
};

bool isEligibleTransientType(Qt::WindowType type)
{
    // Same application-surface exclusions as the automatic whole-application path. A configured
    // target may admit real Popup/Tool/Dialog windows, but never desktop/tooltip/splash/foreign
    // presentation shells.
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
        || !isEligibleTransientType(candidate->windowType())) {
        return false;
    }

    // A top-level QWidget may still have a QWidget parent. This public ownership relation is the
    // strongest signal for popups produced by controls inside a configured subtree and also works
    // when the configured target is not itself a top-level window.
    if (widgetParentChainReaches(candidate, root))
        return true;

    // For an explicitly configured top-level target, also admit QWindows that Qt itself places in
    // its parent/transient-parent ancestry. This covers legitimate unparented transient windows
    // without broadening a child-widget target to every transient of its containing window.
    if (!root->isWindow())
        return false;

    QWindow *rootWindow = root->windowHandle();
    QWindow *candidateWindow = candidate->windowHandle();
    return rootWindow && candidateWindow
           && rootWindow->isAncestorOf(candidateWindow, QWindow::IncludeTransients);
}

class ScopedWidgetSurfaceState final
{
public:
    explicit ScopedWidgetSurfaceState(QWidget *root)
        : m_root(root)
    {
    }

    QWidget *root() const noexcept { return m_root.data(); }

    void refreshOnGuiThread()
    {
        QWidget *rootWidget = m_root.data();
        if (!rootWidget) {
            clear();
            return;
        }

        QSet<QWidget *> live;
        live.insert(rootWidget);
        upsert(rootWidget,
               QRect(rootWidget->mapToGlobal(QPoint(0, 0)), rootWidget->size()),
               rootWidget->width() > 0 && rootWidget->height() > 0);

        const QWidgetList topLevels = QApplication::topLevelWidgets();
        for (QWidget *candidate : topLevels) {
            if (!candidate || candidate == rootWidget || !isOwnedTransient(candidate, rootWidget))
                continue;

            live.insert(candidate);
            const bool visible = candidate->isVisible() && !candidate->isMinimized()
                                 && candidate->width() > 0 && candidate->height() > 0;
            upsert(candidate,
                   QRect(candidate->mapToGlobal(QPoint(0, 0)), candidate->size()),
                   visible);

            if (visible && candidate == QApplication::activePopupWidget())
                m_model.raise(m_ids.value(candidate));
        }

        for (auto it = m_ids.begin(); it != m_ids.end();) {
            QWidget *candidate = it.key();
            if (live.contains(candidate)) {
                ++it;
                continue;
            }
            const SurfaceId id = it.value();
            m_targets.remove(id);
            m_model.remove(id);
            it = m_ids.erase(it);
        }
    }

    QRect canvasBounds() const { return m_model.canvasBounds(); }

    QVector<ScopedSurface> visibleBackToFront() const
    {
        QVector<ScopedSurface> result;
        const QVector<SurfaceRecord> ordered = m_model.visibleBackToFront();
        result.reserve(ordered.size());
        for (const SurfaceRecord &record : ordered) {
            const QPointer<QWidget> widget = m_targets.value(record.id);
            if (!widget)
                continue;
            result.push_back(ScopedSurface{record.id, widget, record.globalGeometry});
        }
        return result;
    }

    std::optional<ScopedSurface> surfaceById(SurfaceId id) const
    {
        const QPointer<QWidget> widget = m_targets.value(id);
        if (!widget)
            return std::nullopt;
        const QVector<SurfaceRecord> ordered = m_model.visibleBackToFront();
        for (const SurfaceRecord &record : ordered) {
            if (record.id == id)
                return ScopedSurface{id, widget, record.globalGeometry};
        }
        return std::nullopt;
    }

    std::optional<ScopedRoutedPoint> routeCanvasPoint(const QPoint &canvasPosition) const
    {
        const std::optional<RoutedPoint> routed = m_model.routeCanvasPoint(canvasPosition);
        if (!routed)
            return std::nullopt;
        const QPointer<QWidget> widget = m_targets.value(routed->surfaceId);
        if (!widget)
            return std::nullopt;
        const auto surface = surfaceById(routed->surfaceId);
        if (!surface)
            return std::nullopt;
        return ScopedRoutedPoint{*surface, routed->localPosition};
    }

private:
    void clear()
    {
        const QList<SurfaceId> ids = m_targets.keys();
        for (SurfaceId id : ids)
            m_model.remove(id);
        m_targets.clear();
        m_ids.clear();
    }

    void upsert(QWidget *widget, const QRect &globalGeometry, bool visible)
    {
        SurfaceId id = m_ids.value(widget, 0);
        if (id == 0) {
            id = m_nextId++;
            m_ids.insert(widget, id);
        }
        m_targets.insert(id, widget);
        m_model.upsert(id, globalGeometry, visible);
    }

    QPointer<QWidget> m_root;
    ApplicationSurfaceModel m_model;
    QHash<QWidget *, SurfaceId> m_ids;
    QHash<SurfaceId, QPointer<QWidget>> m_targets;
    SurfaceId m_nextId = 1;
};

class ScopedWidgetCaptureSource final : public hyremote::CaptureSource
{
public:
    explicit ScopedWidgetCaptureSource(std::shared_ptr<ScopedWidgetSurfaceState> surfaces)
        : m_state(std::make_shared<State>())
    {
        m_state->surfaces = std::move(surfaces);
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
        QWidget *root = m_state->surfaces ? m_state->surfaces->root() : nullptr;
        if (!application || !root || root->thread() != application->thread())
            return false;

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
        std::shared_ptr<ScopedWidgetSurfaceState> surfaces;
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

        QWidget *root = state->surfaces ? state->surfaces->root() : nullptr;
        if (!root) {
            publishEvent(state,
                         hyremote::CaptureEventCode::TargetLost,
                         "the configured QWidget target was destroyed",
                         false);
            return;
        }

        state->surfaces->refreshOnGuiThread();
        const QRect canvas = state->surfaces->canvasBounds();
        if (canvas.isEmpty()) {
            publishEvent(state,
                         hyremote::CaptureEventCode::TemporarilyUnavailable,
                         "the configured QWidget target has no capturable geometry",
                         true);
            return;
        }

        const qreal dpr = qMax<qreal>(1.0, root->devicePixelRatioF());
        const int pixelWidth = qMax(1, qCeil(canvas.width() * dpr));
        const int pixelHeight = qMax(1, qCeil(canvas.height() * dpr));
        const std::size_t bytesPerLine = static_cast<std::size_t>(pixelWidth) * 4U;
        std::shared_ptr<hyremote::CpuFrameStorage> storage =
            hyremote::CpuFrameStorage::createSinglePlane(bytesPerLine,
                                                         static_cast<std::size_t>(pixelHeight));
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
        for (const ScopedSurface &surface : state->surfaces->visibleBackToFront()) {
            QWidget *widget = surface.widget.data();
            if (!widget)
                continue;
            const QPoint targetOffset = surface.globalGeometry.topLeft() - canvas.topLeft();
            widget->render(&painter, targetOffset);
        }
        painter.end();

        const hyremote::TimePoint completion = hyremote::Clock::now();
        hyremote::RemoteFrame frame;
        frame.geometry.size = {static_cast<std::uint32_t>(pixelWidth),
                               static_cast<std::uint32_t>(pixelHeight)};
        frame.geometry.pixelFormat = hyremote::PixelFormat::Rgba8888;
        frame.geometry.alphaMode = hyremote::AlphaMode::Premultiplied;
        frame.geometry.planeCount = 1;
        frame.storage = std::move(storage);
        frame.timing.ptsSource = hyremote::PtsSource::Completion;
        frame.timing.requestTime = request.requestTime;
        frame.timing.completionTime = completion;
        frame.damage = hyremote::Damage::fullFrame();
        frame.requestId = request.id;
        publish(state, &State::onFrame, std::move(frame));
    }

    std::shared_ptr<State> m_state;
};

class ScopedWidgetInputSink final : public hyremote::InputSink
{
public:
    ScopedWidgetInputSink(std::shared_ptr<ScopedWidgetSurfaceState> surfaces,
                          std::shared_ptr<hyremote::InputSink> rootLeaf)
        : m_state(std::make_shared<State>())
    {
        m_state->surfaces = std::move(surfaces);
        m_state->rootLeaf = std::move(rootLeaf);
    }

    ~ScopedWidgetInputSink() override { shutdown(); }

    void post(const hyremote::InputEvent &event) override
    {
        QObject *dispatcher = QCoreApplication::instance();
        if (!dispatcher)
            throw std::runtime_error("Qt application event dispatcher is unavailable");

        const std::shared_ptr<State> state = m_state;
        bool scheduleDrain = false;
        std::string deferredError;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
                return;

            if (!state->deferredError.empty()) {
                deferredError = std::move(state->deferredError);
                state->deferredError.clear();
            } else {
                const InputMailboxAdmission::Class admissionClass = state->admission.classify(event);
                if (admissionClass == InputMailboxAdmission::Class::DropUnmatchedRelease)
                    return;

                if (admissionClass == InputMailboxAdmission::Class::ProtectedRelease) {
                    if (!state->admission.canAcceptProtectedRelease())
                        throw std::runtime_error("bounded scoped Qt protected-release mailbox is full");
                    state->admission.acceptProtectedRelease(event);
                } else {
                    // Pointer history is semantic input to Qt. Surface arbitration never coalesces or
                    // evicts it; the bounded lane reports backpressure exactly like the leaf adapter.
                    if (!state->admission.canAcceptNormal())
                        throw std::runtime_error("bounded scoped Qt input mailbox is full");
                    state->admission.acceptNormal(event);
                }

                state->pending.push_back(QueuedInput{event, qtWindowSystemTimestamp()});
                if (!state->drainScheduled) {
                    state->drainScheduled = true;
                    scheduleDrain = true;
                }
            }
        }

        if (!deferredError.empty())
            throw std::runtime_error(deferredError);
        if (!scheduleDrain)
            return;

        if (!QMetaObject::invokeMethod(
                dispatcher,
                [state] { drainOnGuiThread(state); },
                Qt::QueuedConnection)) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->drainScheduled = false;
            state->pending.clear();
            state->admission.resetAll();
            throw std::runtime_error("failed to queue scoped QWidget input drain to the Qt GUI thread");
        }
    }

    void shutdown() noexcept override
    {
        QObject *dispatcher = QCoreApplication::instance();
        const std::shared_ptr<State> state = m_state;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->shutdownRequested)
                return;
            state->shutdownRequested = true;
            state->active = false;
            state->pending.clear();
            state->admission.resetAll();
            state->drainScheduled = false;
        }

        const auto release = [state] { releaseOnGuiThread(state); };
        if (dispatcher) {
            if (QThread::currentThread() == dispatcher->thread())
                release();
            else
                (void)QMetaObject::invokeMethod(dispatcher, release, Qt::QueuedConnection);
        } else {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->pointer = {};
            state->pointerGrabSurface.reset();
            state->pressedButtons = 0;
            state->pointerWindow.clear();
        }

        if (state->rootLeaf)
            state->rootLeaf->shutdown();
    }

private:
    struct QueuedInput
    {
        hyremote::InputEvent event;
        unsigned long acceptedTimestamp = 0;
    };

    struct State
    {
        std::mutex mutex;
        std::shared_ptr<ScopedWidgetSurfaceState> surfaces;
        std::shared_ptr<hyremote::InputSink> rootLeaf;
        bool active = true;
        bool drainScheduled = false;
        bool shutdownRequested = false;
        std::deque<QueuedInput> pending;
        InputMailboxAdmission admission;
        std::string deferredError;

        QtWindowSystemPointerState pointer;
        std::optional<SurfaceId> pointerGrabSurface;
        std::uint8_t pressedButtons = 0;
        QPointer<QWindow> pointerWindow;
    };

    static std::uint8_t buttonBit(hyremote::PointerButton button)
    {
        switch (button) {
        case hyremote::PointerButton::Left: return 1U << 0U;
        case hyremote::PointerButton::Middle: return 1U << 1U;
        case hyremote::PointerButton::Right: return 1U << 2U;
        case hyremote::PointerButton::None: return 0;
        }
        return 0;
    }

    static void deferError(const std::shared_ptr<State> &state, std::string message)
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->deferredError.empty())
            state->deferredError = std::move(message);
    }

    static void releasePointer(const std::shared_ptr<State> &state,
                               unsigned long timestamp) noexcept
    {
        QWindow *window = state->pointerWindow.data();
        if (window)
            releaseQtWindowSystemPointer(window, timestamp, state->pointer);
        else
            state->pointer = {};
        state->pointerGrabSurface.reset();
        state->pressedButtons = 0;
        state->pointerWindow.clear();
    }

    static void releaseOnGuiThread(const std::shared_ptr<State> &state) noexcept
    {
        releasePointer(state, qtWindowSystemTimestamp());
    }

    static void deliverPointerOnGuiThread(const std::shared_ptr<State> &state,
                                          const QueuedInput &queued)
    {
        state->surfaces->refreshOnGuiThread();
        const QRect canvas = state->surfaces->canvasBounds();
        if (canvas.isEmpty())
            return;

        const auto mapped = hyremote::mapPointerToTarget(queued.event,
                                                         static_cast<float>(canvas.width()),
                                                         static_cast<float>(canvas.height()));
        if (!mapped)
            return;
        const QPoint canvasPoint(qRound(mapped->x), qRound(mapped->y));

        std::optional<ScopedSurface> surface;
        if (state->pointerGrabSurface) {
            surface = state->surfaces->surfaceById(*state->pointerGrabSurface);
            if (!surface) {
                // The surface disappeared while it owned a delivered press. Balance the Qt state
                // against the last live QWindow and drop this now-unmatched continuation.
                releasePointer(state, queued.acceptedTimestamp);
                return;
            }
        } else {
            const auto routed = state->surfaces->routeCanvasPoint(canvasPoint);
            if (!routed)
                return;
            surface = routed->surface;
        }

        QWidget *widget = surface->widget.data();
        QWidget *topLevel = widget ? widget->window() : nullptr;
        QWindow *window = topLevel ? topLevel->windowHandle() : nullptr;
        if (!window)
            return;

        const QPointF global = QPointF(canvas.topLeft() + canvasPoint);
        const QPointF windowLocal = window->mapFromGlobal(global);
        const std::uint8_t bit = buttonBit(queued.event.button);

        if (queued.event.kind == hyremote::InputEventKind::PointerButton
            && queued.event.pressed && bit != 0) {
            if (!state->pointerGrabSurface)
                state->pointerGrabSurface = surface->id;
            state->pressedButtons |= bit;
        }

        state->pointerWindow = window;
        deliverQtWindowSystemPointer(window,
                                     queued.event,
                                     windowLocal,
                                     global,
                                     queued.acceptedTimestamp,
                                     state->pointer);

        if (queued.event.kind == hyremote::InputEventKind::PointerButton
            && !queued.event.pressed && bit != 0) {
            state->pressedButtons &= static_cast<std::uint8_t>(~bit);
            if (state->pressedButtons == 0)
                state->pointerGrabSurface.reset();
        }
    }

    static void deliverKeyOrTextOnGuiThread(const std::shared_ptr<State> &state,
                                             const hyremote::InputEvent &event)
    {
        if (!state->rootLeaf)
            return;
        try {
            // #404 does not redefine keyboard/IME semantics. Preserve the existing Widgets leaf
            // path and limit the scoped layer to top-level pointer surface arbitration.
            state->rootLeaf->post(event);
        } catch (const std::exception &error) {
            deferError(state, std::string("scoped QWidget leaf input rejected an event: ") + error.what());
        } catch (...) {
            deferError(state, "scoped QWidget leaf input rejected an event");
        }
    }

    static void deliverOnGuiThread(const std::shared_ptr<State> &state,
                                   const QueuedInput &queued)
    {
        switch (queued.event.kind) {
        case hyremote::InputEventKind::PointerMove:
        case hyremote::InputEventKind::PointerButton:
        case hyremote::InputEventKind::PointerScroll:
            deliverPointerOnGuiThread(state, queued);
            break;
        case hyremote::InputEventKind::Key:
        case hyremote::InputEventKind::Text:
            deliverKeyOrTextOnGuiThread(state, queued.event);
            break;
        case hyremote::InputEventKind::None:
            break;
        }
    }

    static void drainOnGuiThread(const std::shared_ptr<State> &state)
    {
        std::deque<QueuedInput> batch;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active) {
                state->pending.clear();
                state->admission.resetAll();
                state->drainScheduled = false;
                return;
            }
            batch.swap(state->pending);
            state->admission.pendingBatchTaken();
            state->drainScheduled = false;
        }

        for (const QueuedInput &queued : batch) {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (!state->active)
                    return;
            }
            deliverOnGuiThread(state, queued);
        }
    }

    std::shared_ptr<State> m_state;
};

}  // namespace

TargetComponents createScopedWidgetsTargetComponents(QObject *target, bool remoteInputEnabled)
{
    TargetComponents result;
    auto *widget = qobject_cast<QWidget *>(target);
    if (!widget)
        return result;

    auto surfaces = std::make_shared<ScopedWidgetSurfaceState>(widget);
    result.supported = true;
    result.capture = std::make_unique<ScopedWidgetCaptureSource>(surfaces);

    if (remoteInputEnabled) {
        // Keep the established leaf adapter for Key/Text only. Pointer input is handled by the
        // scoped sink so it can choose a target-owned QWindow before entering the same QWSI ingress.
        TargetComponents leaf = createWidgetsTargetComponents(widget, true);
        if (!leaf.supported || !leaf.input) {
            result.supported = false;
            result.capture.reset();
            result.error = leaf.error.isEmpty()
                               ? QStringLiteral("failed to construct the QWidget leaf input adapter")
                               : leaf.error;
            return result;
        }
        result.input = std::make_shared<ScopedWidgetInputSink>(surfaces, std::move(leaf.input));
    }

    return result;
}

}  // namespace HyRemote::detail
