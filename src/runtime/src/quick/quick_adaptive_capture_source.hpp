#pragma once
// Internal source-level fail-closed policy for Qt5.15+ and Qt6 Quick.
// No per-platform assumptions, QtPrivate, duplicate FrameStorage or
// unbounded cached frame queue. Only one of the two already-started backends
// receives each Core request. A failed native route falls back one-way until
// the next CaptureSource start (the next explicitly controlled epoch).
#include <QDebug>
#include <QMetaObject>
#include <QPointer>
#include <QQuickWindow>
#include <QScreen>
#include <QWindow>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "detail/perf_probe.hpp"
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
        std::uint64_t unavailableSerialAtIssue = 0;
    };
    struct State {
        std::mutex mutex;
        std::condition_variable callbacksDrained;
        std::size_t callbacksInFlight = 0;
        std::uint64_t generation = 0;
        std::uint64_t surfaceEpoch = 1;
        bool active = false;
        bool nativeEnabled = false;
        unsigned int consecutiveNativeFailures = 0;
        // Count provider failure EPISODES, not each request ID sharing a
        // failed GPU readback (one event can complete many empty frames).
        std::uint64_t nativeUnavailableSerial = 0;
        std::string lastNativeUnavailableReason;
        hyremote::TimePoint newestContentPts{};
        std::unordered_map<hyremote::CaptureRequestId, Pending> pending;
        hyremote::FrameReadyHandler onFrame;
        hyremote::CaptureEventHandler onEvent;
    };

    // Reserve callback admission under the same lock used by stop().
    // A callback already admitted must finish before stop() returns, even
    // when the underlying provider has finished its own callback first.
    struct CallbackDrain {
        std::shared_ptr<State> state;
        bool admitted = false;
        ~CallbackDrain() noexcept
        {
            if (!state || !admitted)
                return;
            std::lock_guard<std::mutex> lock(state->mutex);
            --state->callbacksInFlight;
            if (state->callbacksInFlight == 0)
                state->callbacksDrained.notify_all();
        }
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
            auto bump = [state = m_state](bool nativeResourceInvalidated,
                                           const char *signalReason) {
                bool disabled = false;
                {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    ++state->surfaceEpoch;
                    state->newestContentPts = {};
                    if (nativeResourceInvalidated && state->nativeEnabled) {
                        state->nativeEnabled = false;
                        HYREMOTE_P1_COUNT(QuickNativeFallback);
                        disabled = true;
                    }
                    state->consecutiveNativeFailures = 0;
                }
                if (disabled)
                    logNativeFallback(signalReason, {});
            };
            m_connections.push_back(QObject::connect(
                window, &QWindow::widthChanged, window,
                [bump](int) { bump(false, "window_size_changed"); }, Qt::DirectConnection));
            m_connections.push_back(QObject::connect(
                window, &QWindow::heightChanged, window,
                [bump](int) { bump(false, "window_size_changed"); }, Qt::DirectConnection));
            m_connections.push_back(QObject::connect(
                window, &QWindow::screenChanged, window,
                [bump](QScreen *) { bump(true, "screen_changed"); }, Qt::DirectConnection));
            m_connections.push_back(QObject::connect(
                window, &QQuickWindow::sceneGraphInvalidated, window,
                [bump] { bump(true, "scene_graph_invalidated"); }, Qt::DirectConnection));
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
            state->nativeUnavailableSerial = 0;
            state->lastNativeUnavailableReason.clear();
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
        if (m_native && !nativeStarted) {
            m_native->stop(); // failed/throwing start may have partial GPU state
            if (publicStarted)
                qWarning() << "HYREMOTE_QUICK_NATIVE_START_FALLBACK=provider_start_failed";
        }
        if (!publicStarted) {
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
        // Backend stop() drains THEIR callbacks, not callbacks forwarded
        // to Core by this wrapper. Once inactive, no new wrapper callback
        // can enter; wait for every already-admitted callback to exit.
        std::unique_lock<std::mutex> lock(state->mutex);
        state->callbacksDrained.wait(lock, [&] {
            return state->callbacksInFlight == 0;
        });
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
                            request.requestTime,
                            state->nativeUnavailableSerial}).second)
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
            bool fallback = false;
            std::string providerReason;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (state->active && state->generation == generation) {
                    state->pending.erase(request.id);
                    if (route == Route::Native) {
                        fallback = countNativeFailure(*state);
                        providerReason = state->lastNativeUnavailableReason;
                    }
                }
            }
            if (fallback)
                logNativeFallback("native_request_rejected_twice", providerReason);
        }
        return accepted;
    }

private:
    static std::uint64_t currentGeneration(const std::shared_ptr<State> &state)
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        return state->generation;
    }

    // Caller holds State::mutex. Only an actual route transition is counted.
    static bool countNativeFailure(State &state)
    {
        if (!state.nativeEnabled)
            return false;
        if (state.consecutiveNativeFailures < kFallbackAfterFailures)
            ++state.consecutiveNativeFailures;
        if (state.consecutiveNativeFailures < kFallbackAfterFailures)
            return false;
        state.nativeEnabled = false; // one-way until a controlled restart
        HYREMOTE_P1_COUNT(QuickNativeFallback);
        return true;
    }

    static void logNativeFallback(const char *cause, const std::string &reason)
    {
        // One warning per real native-to-public route change, not per frame.
        // Provider text is a bounded internal failure reason, never pixels.
        qWarning().noquote() << "HYREMOTE_QUICK_NATIVE_FALLBACK_REASON="
                             << cause << "PROVIDER_DETAIL="
                             << QString::fromStdString(reason);
    }

    static void deliverEvent(const std::shared_ptr<State> &state,
                             std::uint64_t generation, Route route,
                             const hyremote::CaptureEvent &event)
    {
        hyremote::CaptureEventHandler callback;
        hyremote::CaptureEvent delivered = event;
        CallbackDrain drain{state};
        bool fallback = false;
        const char *fallbackCause = nullptr;
        std::string reason;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->active || state->generation != generation)
                return;
            // One provider TemporarilyUnavailable event describes one GPU
            // failure episode, possibly terminating several accepted IDs.
            if (route == Route::Native && event.recoverable
                && event.code == hyremote::CaptureEventCode::TemporarilyUnavailable) {
                ++state->nativeUnavailableSerial;
                state->lastNativeUnavailableReason = event.message;
                fallback = countNativeFailure(*state);
                fallbackCause = "consecutive_native_unavailable_events";
                reason = state->lastNativeUnavailableReason;
            }
            if (route == Route::Native && !event.recoverable
                && event.code == hyremote::CaptureEventCode::BackendFailure) {
                if (state->nativeEnabled) {
                    state->nativeEnabled = false;
                    HYREMOTE_P1_COUNT(QuickNativeFallback);
                    fallback = true;
                    fallbackCause = "native_backend_failure";
                    reason = event.message;
                }
                // Public Qt route is already prepared. Surface TargetLost
                // remains fatal; BackendFailure alone is recoverable.
                delivered.recoverable = true;
                delivered.code = hyremote::CaptureEventCode::TemporarilyUnavailable;
            }
            callback = state->onEvent;
            if (callback) {
                ++state->callbacksInFlight;
                drain.admitted = true;
            }
        }
        if (fallback)
            logNativeFallback(fallbackCause, reason);
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
        CallbackDrain drain{state};
        bool dropPixels = false;
        bool fallback = false;
        const char *fallbackCause = nullptr;
        std::string providerReason;
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
            // Public grabToImage reports Completion time as its best-known
            // content timestamp. Native readback has the stronger Render PTS.
            // Compare both in the same monotonic domain to prevent an old
            // native readback from replacing a newer public fallback frame.
            const auto contentPts = frame.timing.ptsSource
                == hyremote::PtsSource::Completion && frame.timing.completionTime
                ? *frame.timing.completionTime : frame.timing.pts;
            const bool hasContentPts = contentPts != hyremote::TimePoint{};
            const bool oldContent = hasContentPts
                && ((renderPts && now > contentPts
                     && now - contentPts > kMaxRenderFrameAge)
                    || (state->newestContentPts != hyremote::TimePoint{}
                        && contentPts < state->newestContentPts));
            // A non-null native storage anchor alone is not proof that the
            // frame can be published. Reject cheap geometry/format/timing
            // violations here, before an invalid native stream causes Core
            // to reject every frame without ever activating the fallback.
            const bool invalidNativeFrame = route == Route::Native
                && frame.storage
                && (frame.geometry.size.width == 0
                    || frame.geometry.size.height == 0
                    || frame.geometry.pixelFormat != hyremote::PixelFormat::Rgba8888
                    || frame.geometry.planeCount != 1
                    || (renderPts && !hasContentPts)
                    || (renderPts && frame.timing.completionTime
                        && contentPts > *frame.timing.completionTime));
            dropPixels = invalidEpoch || oldContent || invalidNativeFrame;
            if (dropPixels)
                HYREMOTE_P1_COUNT(QuickStaleDrop);
            const bool nativeFailure = route == Route::Native
                && !invalidEpoch && (!frame.storage || oldContent
                                      || invalidNativeFrame);
            // A provider event precedes every empty completion it produces.
            // Multiple IDs from one shared GPU failure are one failure,
            // not enough to force fallback by themselves.
            const bool emptyFromCountedEvent = route == Route::Native
                && !frame.storage
                && state->nativeUnavailableSerial
                   > it->second.unavailableSerialAtIssue;
            if (nativeFailure && !emptyFromCountedEvent) {
                fallback = countNativeFailure(*state);
                fallbackCause = oldContent ? "native_content_too_old"
                              : invalidNativeFrame ? "invalid_native_frame"
                              : "native_empty_frame_without_event";
                providerReason = state->lastNativeUnavailableReason;
            } else if (route == Route::Native && frame.storage && !dropPixels) {
                state->consecutiveNativeFailures = 0;
            }
            if (!dropPixels && frame.storage && hasContentPts
                && contentPts > state->newestContentPts)
                state->newestContentPts = contentPts;
            callback = state->onFrame;
            state->pending.erase(it);
            if (!callback)
                return;
            ++state->callbacksInFlight;
            drain.admitted = true;
        }
        if (fallback)
            logNativeFallback(fallbackCause, providerReason);
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
