#include "interactive_composite_target.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "detail/composition_support.hpp"
#include "detail/input_mailbox_admission.hpp"
#include "detail/qt_window_system_input.hpp"
#include "hyremote/core/input.hpp"

namespace HyRemote::Runtime::Automatic {
namespace {

class CompositeInputSink final : public hyremote::InputSink
{
public:
    CompositeInputSink(CompositeTarget *target,
                       ::HyRemote::detail::BuiltinTargetResolver resolver)
        : m_state(std::make_shared<State>())
    {
        m_state->target = target;
        m_state->resolver = std::move(resolver);
    }

    ~CompositeInputSink() override { shutdown(); }

    // Exactly-once acceptance contract (#164 acceptance 3):
    //
    //   * `post` accepts an event only if it can be queued for the GUI thread. On acceptance the caller gets no
    //     error, and the event is delivered to exactly one child - the routed surface, or the surface holding the
    //     pointer grab for a button lifecycle.
    //   * Pointer history is semantic input to Qt. This arbitration layer never coalesces or evicts PointerMove;
    //     normal-lane saturation is explicit backpressure. Matching releases use the same bounded protected reserve
    //     as the leaf Widgets/Quick adapters so a delivered hold can still be balanced.
    //   * A child adapter that rejects an event during the drain is recorded as a deferred error and **surfaces on
    //     the next `post`**, which then throws **without accepting its own event** - so a caller always learns
    //     about a failed delivery before its next event is taken, and never after.
    //   * Queueing the first GUI turn is part of acceptance. Later continuation turns are internal to the already
    //     accepted sequence; if one cannot be queued, the remaining pending sequence is discarded and the failure
    //     is surfaced through the same deferred-error channel.
    //   * The timestamp captured here is replayed through a Runtime-private thread-local scope when the leaf sink is
    //     invoked on the GUI thread. Queue delay therefore cannot change Qt's click classification.
    //   * One composite GUI turn forwards exactly one accepted event. The leaf drain is therefore enqueued before
    //     the next composite turn, preserving global A -> B -> A chronology across different child surfaces.
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
                const ::HyRemote::detail::InputMailboxAdmission::Class admissionClass =
                    state->admission.classify(event);
                if (admissionClass
                    == ::HyRemote::detail::InputMailboxAdmission::Class::DropUnmatchedRelease) {
                    return;
                }

                if (admissionClass
                    == ::HyRemote::detail::InputMailboxAdmission::Class::ProtectedRelease) {
                    if (!state->admission.canAcceptProtectedRelease())
                        throw std::runtime_error("bounded composite protected-release mailbox is full");
                    state->admission.acceptProtectedRelease(event);
                } else {
                    if (!state->admission.canAcceptNormal())
                        throw std::runtime_error("bounded automatic composite input mailbox is full");
                    state->admission.acceptNormal(event);
                }

                state->pending.push_back(QueuedInput{event,
                                                     ::HyRemote::detail::qtWindowSystemTimestamp(),
                                                     admissionClass});
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
            throw std::runtime_error("failed to queue automatic composite input drain to the Qt GUI thread");
        }
    }

    void shutdown() noexcept override
    {
        std::vector<std::shared_ptr<hyremote::InputSink>> childSinks;
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            if (m_state->shutdownRequested)
                return;
            m_state->shutdownRequested = true;
            m_state->active = false;
            m_state->pending.clear();
            m_state->admission.resetAll();
            m_state->drainScheduled = false;
            m_state->pointerGrabSurface.reset();
            m_state->pressedButtons = 0;
            childSinks.reserve(m_state->children.size());
            for (const auto &entry : m_state->children) {
                if (entry.second.sink)
                    childSinks.push_back(entry.second.sink);
            }
            m_state->children.clear();
        }

        // Each child is a normal Widgets/Quick sink. Its terminal shutdown discards child events
        // that never reached Qt and balances only held state already delivered to that surface.
        for (const auto &sink : childSinks) {
            if (sink)
                sink->shutdown();
        }
    }

private:
    struct QueuedInput
    {
        hyremote::InputEvent event;
        unsigned long acceptedTimestamp = 0;
        ::HyRemote::detail::InputMailboxAdmission::Class admissionClass =
            ::HyRemote::detail::InputMailboxAdmission::Class::Normal;
    };

    struct ChildInput
    {
        QPointer<QObject> target;
        std::shared_ptr<hyremote::InputSink> sink;
    };

    struct State
    {
        std::mutex mutex;
        QPointer<CompositeTarget> target;
        ::HyRemote::detail::BuiltinTargetResolver resolver;
        bool active = true;
        bool drainScheduled = false;
        bool shutdownRequested = false;
        std::deque<QueuedInput> pending;
        ::HyRemote::detail::InputMailboxAdmission admission;
        std::unordered_map<SurfaceId, ChildInput> children;
        std::string deferredError;

        // GUI-thread-owned pointer lifecycle state. A drag/release stays with the surface that
        // received the press even if the pointer crosses an overlapping application window.
        std::optional<SurfaceId> pointerGrabSurface;
        std::uint8_t pressedButtons = 0;
    };

    static std::uint8_t buttonBit(hyremote::PointerButton button)
    {
        switch (button) {
        case hyremote::PointerButton::Left:
            return 1U << 0U;
        case hyremote::PointerButton::Middle:
            return 1U << 1U;
        case hyremote::PointerButton::Right:
            return 1U << 2U;
        case hyremote::PointerButton::None:
            return 0;
        }
        return 0;
    }

    static void deferError(const std::shared_ptr<State> &state, std::string message)
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->deferredError.empty())
            state->deferredError = std::move(message);
    }

    static std::shared_ptr<hyremote::InputSink> ensureChildInput(
        const std::shared_ptr<State> &state,
        const CompositeSurfaceSnapshot &surface)
    {
        QObject *target = surface.target.data();
        if (!target)
            return {};

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            auto it = state->children.find(surface.id);
            if (it != state->children.end() && it->second.target.data() == target
                && it->second.sink) {
                return it->second.sink;
            }
        }

        ::HyRemote::detail::TargetComponents components = state->resolver(target, true);
        if (!components.supported || !components.input) {
            deferError(state,
                       "an automatic composite child surface has no built-in remote-input adapter");
            return {};
        }

        ChildInput child;
        child.target = target;
        child.sink = std::move(components.input);
        const std::shared_ptr<hyremote::InputSink> sink = child.sink;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
                return {};
            state->children[surface.id] = std::move(child);
        }
        return sink;
    }

    static void pruneChildInputs(const std::shared_ptr<State> &state,
                                 const CompositeTargetSnapshot &snapshot)
    {
        std::unordered_set<SurfaceId> live;
        live.reserve(static_cast<std::size_t>(snapshot.backToFront.size()));
        for (const CompositeSurfaceSnapshot &surface : snapshot.backToFront)
            live.insert(surface.id);

        std::vector<std::shared_ptr<hyremote::InputSink>> removed;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            for (auto it = state->children.begin(); it != state->children.end();) {
                if (live.find(it->first) == live.end()) {
                    if (it->second.sink)
                        removed.push_back(it->second.sink);
                    it = state->children.erase(it);
                } else {
                    ++it;
                }
            }
            if (state->pointerGrabSurface
                && live.find(*state->pointerGrabSurface) == live.end()) {
                state->pointerGrabSurface.reset();
                state->pressedButtons = 0;
            }
        }

        // A child may disappear while it owns delivered remote input. Shut it down before its last
        // shared_ptr is released so hidden/detached but still-live Qt objects cannot keep a
        // synthetic remote press after leaving the composed canvas.
        for (const auto &sink : removed) {
            if (sink)
                sink->shutdown();
        }
    }

    static std::optional<CompositeRoutedPoint> routePointer(
        const std::shared_ptr<State> &state,
        CompositeTarget *target,
        const hyremote::InputEvent &event)
    {
        const CompositeTargetSnapshot snapshot = target->captureSnapshot();
        if (snapshot.canvasBounds.isEmpty())
            return std::nullopt;

        const auto mapped = HyRemote::detail::mapPointerToNormalizedTarget(
            event,
            static_cast<float>(snapshot.canvasBounds.width()),
            static_cast<float>(snapshot.canvasBounds.height()));
        if (!mapped)
            return std::nullopt;

        const QPoint canvasPoint(qRound(mapped->x), qRound(mapped->y));

        std::optional<SurfaceId> grab;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            grab = state->pointerGrabSurface;
        }

        if (grab) {
            const auto surface = target->surfaceById(*grab);
            if (surface) {
                const QPoint globalPoint = snapshot.canvasBounds.topLeft() + canvasPoint;
                return CompositeRoutedPoint{*surface,
                                            globalPoint - surface->globalGeometry.topLeft()};
            }

            std::lock_guard<std::mutex> lock(state->mutex);
            state->pointerGrabSurface.reset();
            state->pressedButtons = 0;
        }

        return target->routeCanvasPoint(canvasPoint);
    }

    static void postToSurface(const std::shared_ptr<State> &state,
                              const CompositeSurfaceSnapshot &surface,
                              const QueuedInput &queued)
    {
        const auto sink = ensureChildInput(state, surface);
        if (!sink)
            return;
        try {
            ::HyRemote::detail::QtWindowSystemAcceptedTimestampScope timestampScope(
                queued.acceptedTimestamp);
            sink->post(queued.event);
        } catch (const std::exception &error) {
            deferError(state,
                       std::string("automatic composite child input adapter rejected an event: ")
                           + error.what());
        } catch (...) {
            deferError(state, "automatic composite child input adapter rejected an event");
        }
    }

    static void deliverPointerOnGuiThread(const std::shared_ptr<State> &state,
                                          CompositeTarget *target,
                                          const QueuedInput &queued)
    {
        const hyremote::InputEvent &event = queued.event;
        std::optional<CompositeRoutedPoint> routed = routePointer(state, target, event);
        if (!routed)
            return;

        const int width = routed->surface.globalGeometry.width();
        const int height = routed->surface.globalGeometry.height();
        if (width <= 0 || height <= 0)
            return;

        QueuedInput childQueued = queued;
        childQueued.event.sourceViewport = {static_cast<std::uint32_t>(width),
                                            static_cast<std::uint32_t>(height),
                                            1.0F};
        childQueued.event.x = static_cast<float>(routed->localPosition.x());
        childQueued.event.y = static_cast<float>(routed->localPosition.y());

        const std::uint8_t bit = buttonBit(event.button);
        if (event.kind == hyremote::InputEventKind::PointerButton && event.pressed && bit != 0) {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->pointerGrabSurface)
                state->pointerGrabSurface = routed->surface.id;
            state->pressedButtons |= bit;
        }

        postToSurface(state, routed->surface, childQueued);

        if (event.kind == hyremote::InputEventKind::PointerButton && !event.pressed && bit != 0) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->pressedButtons &= static_cast<std::uint8_t>(~bit);
            if (state->pressedButtons == 0)
                state->pointerGrabSurface.reset();
        }
    }

    static void deliverKeyOrTextOnGuiThread(const std::shared_ptr<State> &state,
                                             CompositeTarget *target,
                                             const QueuedInput &queued)
    {
        const auto surface = target->activeSurface();
        if (!surface)
            return;
        postToSurface(state, *surface, queued);
    }

    static void deliverOnGuiThread(const std::shared_ptr<State> &state,
                                   const QueuedInput &queued)
    {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
                return;
        }

        CompositeTarget *target = state->target.data();
        if (!target)
            return;

        if (auto *interactive = dynamic_cast<InteractiveCompositeTarget *>(target))
            interactive->refreshSurfaces();

        const CompositeTargetSnapshot snapshot = target->captureSnapshot();
        pruneChildInputs(state, snapshot);

        switch (queued.event.kind) {
        case hyremote::InputEventKind::PointerMove:
        case hyremote::InputEventKind::PointerButton:
        case hyremote::InputEventKind::PointerScroll:
            deliverPointerOnGuiThread(state, target, queued);
            break;
        case hyremote::InputEventKind::Key:
        case hyremote::InputEventKind::Text:
            deliverKeyOrTextOnGuiThread(state, target, queued);
            break;
        case hyremote::InputEventKind::None:
            break;
        }
    }

    static void drainOnGuiThread(const std::shared_ptr<State> &state)
    {
        QueuedInput queued;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active) {
                state->pending.clear();
                state->admission.resetAll();
                state->drainScheduled = false;
                return;
            }
            if (state->pending.empty()) {
                state->drainScheduled = false;
                return;
            }

            queued = std::move(state->pending.front());
            state->pending.pop_front();
            if (queued.admissionClass
                == ::HyRemote::detail::InputMailboxAdmission::Class::ProtectedRelease) {
                state->admission.removePendingProtectedRelease();
            } else {
                state->admission.removePendingNormal();
            }
        }

        // Forward exactly one event before scheduling the next composite turn. Built-in leaf sinks
        // post their own GUI drain here, so Qt's FIFO queue keeps cross-surface chronology intact.
        deliverOnGuiThread(state, queued);

        QObject *dispatcher = QCoreApplication::instance();
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
                return;
            if (state->pending.empty()) {
                state->drainScheduled = false;
                return;
            }
        }

        if (!dispatcher
            || !QMetaObject::invokeMethod(
                dispatcher,
                [state] { drainOnGuiThread(state); },
                Qt::QueuedConnection)) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->drainScheduled = false;
            state->pending.clear();
            state->admission.resetAll();
            if (state->deferredError.empty()) {
                state->deferredError =
                    "failed to queue automatic composite continuation drain to the Qt GUI thread";
            }
        }
    }

    std::shared_ptr<State> m_state;
};

}  // namespace

void InteractiveCompositeTarget::setSurfaceRefreshHandler(SurfaceRefreshHandler handler)
{
    m_surfaceRefreshHandler = std::move(handler);
}

void InteractiveCompositeTarget::refreshSurfaces()
{
    if (m_surfaceRefreshHandler)
        m_surfaceRefreshHandler();
}

::HyRemote::detail::TargetComponents InteractiveCompositeTarget::createTargetComponents(
    bool remoteInputEnabled,
    const ::HyRemote::detail::BuiltinTargetResolver &resolveBuiltinTarget)
{
    // Reuse the exact capture composition from the predecessor gate. Passing false prevents the
    // base gate from manufacturing an error solely because input is being added by this layer.
    ::HyRemote::detail::TargetComponents result =
        CompositeTarget::createTargetComponents(false, resolveBuiltinTarget);
    if (remoteInputEnabled)
        result.input = std::make_shared<CompositeInputSink>(this, resolveBuiltinTarget);
    return result;
}

}  // namespace HyRemote::Runtime::Automatic
