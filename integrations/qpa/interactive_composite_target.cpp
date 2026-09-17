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

#include "hyremote/core/input.hpp"

namespace HyRemote::Qpa {
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
                // Match the built-in target-adapter backpressure rule: pointer motion is freshness
                // oriented, while button/key/text lifecycles are never silently overwritten.
                if (event.kind == hyremote::InputEventKind::PointerMove
                    && !state->pending.empty()
                    && state->pending.back().kind == hyremote::InputEventKind::PointerMove) {
                    state->pending.back() = event;
                } else {
                    if (state->pending.size() >= kMaxPendingInputEvents) {
                        const auto staleMove = std::find_if(
                            state->pending.begin(),
                            state->pending.end(),
                            [](const hyremote::InputEvent &queued) {
                                return queued.kind == hyremote::InputEventKind::PointerMove;
                            });
                        if (staleMove != state->pending.end())
                            state->pending.erase(staleMove);
                    }
                    if (state->pending.size() >= kMaxPendingInputEvents)
                        throw std::runtime_error("bounded QPA composite input mailbox is full");
                    state->pending.push_back(event);
                }

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
            throw std::runtime_error("failed to queue QPA composite input drain to the Qt GUI thread");
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
    static constexpr std::size_t kMaxPendingInputEvents = 64;

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
        std::deque<hyremote::InputEvent> pending;
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
                       "a QPA composite child surface has no built-in remote-input adapter");
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

        const auto mapped = hyremote::mapPointerToTarget(
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
                              const hyremote::InputEvent &event)
    {
        const auto sink = ensureChildInput(state, surface);
        if (!sink)
            return;
        try {
            sink->post(event);
        } catch (const std::exception &error) {
            deferError(state,
                       std::string("QPA composite child input adapter rejected an event: ")
                           + error.what());
        } catch (...) {
            deferError(state, "QPA composite child input adapter rejected an event");
        }
    }

    static void deliverPointerOnGuiThread(const std::shared_ptr<State> &state,
                                          CompositeTarget *target,
                                          const hyremote::InputEvent &event)
    {
        std::optional<CompositeRoutedPoint> routed = routePointer(state, target, event);
        if (!routed)
            return;

        const int width = routed->surface.globalGeometry.width();
        const int height = routed->surface.globalGeometry.height();
        if (width <= 0 || height <= 0)
            return;

        hyremote::InputEvent childEvent = event;
        childEvent.sourceViewport = {static_cast<std::uint32_t>(width),
                                     static_cast<std::uint32_t>(height),
                                     1.0F};
        childEvent.x = static_cast<float>(routed->localPosition.x());
        childEvent.y = static_cast<float>(routed->localPosition.y());

        const std::uint8_t bit = buttonBit(event.button);
        if (event.kind == hyremote::InputEventKind::PointerButton && event.pressed && bit != 0) {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->pointerGrabSurface)
                state->pointerGrabSurface = routed->surface.id;
            state->pressedButtons |= bit;
        }

        postToSurface(state, routed->surface, childEvent);

        if (event.kind == hyremote::InputEventKind::PointerButton && !event.pressed && bit != 0) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->pressedButtons &= static_cast<std::uint8_t>(~bit);
            if (state->pressedButtons == 0)
                state->pointerGrabSurface.reset();
        }
    }

    static void deliverKeyOrTextOnGuiThread(const std::shared_ptr<State> &state,
                                             CompositeTarget *target,
                                             const hyremote::InputEvent &event)
    {
        const auto surface = target->activeSurface();
        if (!surface)
            return;
        postToSurface(state, *surface, event);
    }

    static void deliverOnGuiThread(const std::shared_ptr<State> &state,
                                   const hyremote::InputEvent &event)
    {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active)
                return;
        }

        CompositeTarget *target = state->target.data();
        if (!target)
            return;

        const CompositeTargetSnapshot snapshot = target->captureSnapshot();
        pruneChildInputs(state, snapshot);

        switch (event.kind) {
        case hyremote::InputEventKind::PointerMove:
        case hyremote::InputEventKind::PointerButton:
        case hyremote::InputEventKind::PointerScroll:
            deliverPointerOnGuiThread(state, target, event);
            break;
        case hyremote::InputEventKind::Key:
        case hyremote::InputEventKind::Text:
            deliverKeyOrTextOnGuiThread(state, target, event);
            break;
        case hyremote::InputEventKind::None:
            break;
        }
    }

    static void drainOnGuiThread(const std::shared_ptr<State> &state)
    {
        std::deque<hyremote::InputEvent> batch;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active) {
                state->pending.clear();
                state->drainScheduled = false;
                return;
            }
            batch.swap(state->pending);
            state->drainScheduled = false;
        }

        for (const hyremote::InputEvent &event : batch) {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (!state->active)
                    return;
            }
            deliverOnGuiThread(state, event);
        }
    }

    std::shared_ptr<State> m_state;
};

}  // namespace

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

}  // namespace HyRemote::Qpa
