#pragma once
// Internal source-level fail-closed policy for Qt5.15+ and Qt6 Quick.
// No per-platform assumptions, QtPrivate, duplicate FrameStorage or
// unbounded cached frame queue. Only one of the two already-started backends
// receives each Core request. A failed native route falls back one-way until
// the next CaptureSource start (the next explicitly controlled epoch).
#include <QMetaObject>
#include <QPointer>
#include <QQuickWindow>
#include <QScreen>
#include <QWindow>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "hyremote/core/capture_source.hpp"

namespace HyRemote::detail {

class AdaptiveQuickCaptureSource final : public hyremote::CaptureSource
{
    enum class Route : unsigned char { Native, Public };
    struct Pending {
        std::uint64_t generation = 0;
        std::uint64_t surfaceEpoch = 0;
        Route route = Route::Public;
        hyremote::TimePoint requestTime{};
    };
    struct State {
        std::mutex mutex;
        std::uint64_t generation = 0;
        std::uint64_t surfaceEpoch = 1;
        bool active = false;
        bool nativeEnabled = false;
        unsigned int consecutiveNativeFailures = 0;
        hyremote::TimePoint newestContentPts{};
        std::unordered_map<hyremote::CaptureRequestId, Pending> pending;
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
    };

    static constexpr unsigned int kFallbackAfterFailures = 2;
    static constexpr std::size_t kMaxOutstandingRequests = 8;
    static constexpr auto kMaxRenderFrameAge = std::chrono::milliseconds(500);

public:
    AdaptiveQuickCaptureSource(
        QQuickWindow *window,
        std::unique_ptr<hyremote::CaptureSource> native,
        std::unique_ptr<hyremote::CaptureSource> publicQt)
        : m_native(std::move(native))
        , m_public(std::move(publicQt))
        , m_state(std::make_shared<State>())
    {
        if (window) {
            // These signals describe Qt's actual Surface, not a GPU model.
            // A callback can be queued on the GUI or render thread; only the
            // small epoch counter is shared with the capture scheduler.
            auto bump = [state = m_state] {
                std::lock_guard<std::mutex> lock(state->mutex);
                ++state->surfaceEpoch;
                state->newestContentPts = {};
                state->consecutiveNativeFailures = 0;
            };
            m_connections.push_back(QObject::connect(
                window, &QWindow::widthChanged, window,
                [bump](int) { bump(); }, Qt::DirectConnection));
            m_connections.push_back(QObject::connect(
                window, &QWindow::heightChanged, window,
                [bump](int) { bump(); }, Qt::DirectConnection));
            m_connections.push_back(QObject::connect(
                window, &QWindow::screenChanged, window,
                [bump](QScreen *) { bump(); }, Qt::DirectConnection));
            m_connections.push_back(QObject::connect(
                window, &QQuickWindow::sceneGraphInvalidated, window,
                [bump] { bump(); }, Qt::DirectConnection));
        }
    }

    ~AdaptiveQuickCaptureSource() override
    {
        stop();
        for (const auto &connection : m_connections)
            QObject::disconnect(connection);
    }

    hyremote::CaptureCapabilities capabilities() const override
    {
        return m_public->capabilities(); // stable across GPU fallback
    }

    bool start(hyremote::FrameReadyHandler onFrame,
               hyremote::CaptureEventHandler onEvent) override
    {
        auto state = m_state;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->active)
                return false;
            ++state->generation;
            state->active = true;
            state->nativeEnabled = false;
            state->consecutiveNativeFailures = 0;
            state->newestContentPts = {};
            state->pending.clear();
            state->onFrame = std::move(onFrame);
            state->onEvent = std::move(onEvent);
        }
        const std::uint64_t generation = currentGeneration(state);
        // Prepare the public fallback, but never issue a public grab while
        // native pixels are working. Both backends remain source-owned.
        bool publicStarted = false;
        bool nativeStarted = false;
        try {
            publicStarted = m_public->start(
                [state, generation](hyremote::RemoteFrame frame) {
                    deliverFrame(state, generation, Route::Public, std::move(frame));
                },
                [state, generation](const hyremote::CaptureEvent &event) {
                    deliverEvent(state, generation, Route::Public, event);
                });
            if (publicStarted && m_native) {
                nativeStarted = m_native->start(
                    [state, generation](hyremote::RemoteFrame frame) {
                        deliverFrame(state, generation, Route::Native, std::move(frame));
                    },
                    [state, generation](const hyremote::CaptureEvent &event) {
                        deliverEvent(state, generation, Route::Native, event);
                    });
            }
        } catch (...) {
            // A partial start still needs a deterministic stop before reuse.
        }
        if (!publicStarted) {
            if (m_native)
                m_native->stop();
            m_public->stop();
            std::lock_guard<std::mutex> lock(state->mutex);
            state->active = false;
            state->onFrame = {};
            state->onEvent = {};
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->active && state->generation == generation)
                state->nativeEnabled = nativeStarted;
        }
        return true;
    }

    void stop() noexcept override
    {
        auto state = m_state;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->active = false;
            ++state->generation;
            state->pending.clear();
            state->onFrame = {};
            state->onEvent = {};
            state->nativeEnabled = false;
        }
        // Backend stop() is the callback quiescence contract. Never call it
        // from a backend's own onFrame/onEvent callback (would deadlock).
        if (m_native)
            m_native->stop();
        if (m_public)
            m_public->stop();
    }

    bool requestFrame(const hyremote::CaptureRequest &request) override
    {
        auto state = m_state;
        Route route = Route::Public;
        std::uint64_t generation;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active || state->pending.size() >= kMaxOutstandingRequests)
                return false;
            generation = state->generation;
            route = state->nativeEnabled ? Route::Native : Route::Public;
            if (!state->pending.emplace(request.id,
                    Pending{generation, state->surfaceEpoch, route,
                            request.requestTime}).second)
                return false;
        }
        bool accepted = false;
        try {
            auto *backend = route == Route::Native ? m_native.get() : m_public.get();
            accepted = backend && backend->requestFrame(request);
        } catch (...) {
            accepted = false;
        }
        if (!accepted) {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->active && state->generation == generation) {
                state->pending.erase(request.id);
                if (route == Route::Native)
                    countNativeFailure(*state);
            }
        }
        return accepted;
    }

private:
    static std::uint64_t currentGeneration(const std::shared_ptr<State> &state)
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        return state->generation;
    }

    static void countNativeFailure(State &state)
    {
        if (state.consecutiveNativeFailures < kFallbackAfterFailures)
            ++state.consecutiveNativeFailures;
        if (state.consecutiveNativeFailures >= kFallbackAfterFailures)
            state.nativeEnabled = false; // permanently for this start()
    }

    static void deliverEvent(const std::shared_ptr<State> &state,
                             std::uint64_t generation, Route route,
                             const hyremote::CaptureEvent &event)
    {
        hyremote::CaptureEventHandler callback;
        hyremote::CaptureEvent delivered = event;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active || state->generation != generation)
                return;
            callback = state->onEvent;
            // A recoverable GPU-resource failure is not an unrecoverable
            // product Session error when a public fallback is available.
            if (route == Route::Native && !event.recoverable) {
                state->nativeEnabled = false;
                delivered.recoverable = true;
                delivered.code = hyremote::CaptureEventCode::TemporarilyUnavailable;
            }
        }
        if (callback) {
            try { callback(delivered); } catch (...) {}
        }
    }

    static void deliverFrame(const std::shared_ptr<State> &state,
                             std::uint64_t generation, Route route,
                             hyremote::RemoteFrame frame)
    {
        if (!frame.requestId)
            return;
        hyremote::FrameReadyHandler callback;
        bool dropPixels = false;
        const auto now = hyremote::Clock::now();
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active || state->generation != generation)
                return;
            auto it = state->pending.find(*frame.requestId);
            if (it == state->pending.end()
                || it->second.generation != generation
                || it->second.route != route)
                return; // no duplicate/retired/new-generation delivery
            // Never reuse pixels produced before the Surface was resized,
            // reparented to a screen or invalidated by Qt's scene graph.
            const bool invalidEpoch = it->second.surfaceEpoch != state->surfaceEpoch;
            const bool renderPts = frame.timing.ptsSource == hyremote::PtsSource::Render;
            const bool oldContent = renderPts
                && frame.timing.pts != hyremote::TimePoint{}
                && ((now > frame.timing.pts
                     && now - frame.timing.pts > kMaxRenderFrameAge)
                    || (state->newestContentPts != hyremote::TimePoint{}
                        && frame.timing.pts < state->newestContentPts));
            dropPixels = invalidEpoch || oldContent;
            const bool nativeFailure = route == Route::Native
                && !invalidEpoch && (!frame.storage || oldContent);
            if (nativeFailure)
                countNativeFailure(*state);
            else if (route == Route::Native && frame.storage && !dropPixels)
                state->consecutiveNativeFailures = 0;
            if (!dropPixels && frame.storage && renderPts
                && frame.timing.pts > state->newestContentPts)
                state->newestContentPts = frame.timing.pts;
            callback = state->onFrame;
            state->pending.erase(it);
        }
        if (!callback)
            return;
        if (dropPixels) {
            // One terminal outcome per admitted request. An empty completion
            // retires Core's in-flight slot without publishing stale pixels.
            frame.storage.reset();
            frame.geometry = {};
            frame.damage = {};
            frame.timing.pts = {};
            frame.timing.ptsSource = hyremote::PtsSource::Completion;
            frame.timing.completionTime = now;
        }
        try { callback(std::move(frame)); } catch (...) {}
    }

    std::unique_ptr<hyremote::CaptureSource> m_native;
    std::unique_ptr<hyremote::CaptureSource> m_public;
    std::shared_ptr<State> m_state;
    std::vector<QMetaObject::Connection> m_connections;
};

} // namespace HyRemote::detail
