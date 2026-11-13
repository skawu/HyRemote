#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>

#if defined(HYREMOTE_VIEWER_DEMAND_TEST_HOOK)
#include <functional>
#include <utility>
#endif

// Private Shared Runtime demand gate. Never changes Core Session scheduling or exposes a public
// integration knob: it suppresses unnecessary Qt captures after RFB has compared *served* pixels.
namespace HyRemote::detail {

class ViewerCaptureDemand final
{
public:
    // An afterAnimating signal is merely a Qt render-loop ACTIVITY hint.
    // Recording a reason here prevents a fake "hint-to-request latency"
    // from being reported as actual scheduler wakeup gain.
    enum class ActivityResult {
        Stopped,
        NoViewer,
        CaptureInFlight,
        DeliveryBackpressure,
        AlreadyActive,
        ReleasedStaticWait
    };
    // A local-only Qt Quick page change still becomes visible through the next bounded probe.
    // Do not rely solely on QQuickWindow rendering signals: offscreen grabs can generate them.
    // #789: matched 1080p A/B/A static tests showed about half the capture
    // load with 250ms idle probes (9.2% -> 4.5% one-core CPU, Windows).
    // Local-only UI changes can take up to 150ms longer to be discovered than
    // the 100ms baseline; remote input and new viewers reset idle immediately.
    // Keep this private to the shared Runtime; active frames retain 60fps.
    static constexpr auto kStaticProbeInterval = std::chrono::milliseconds(250);
    static constexpr std::size_t kConsecutiveUnchangedFrames = 3;

#if defined(HYREMOTE_VIEWER_DEMAND_TEST_HOOK)
    // Test-only concurrency seam; never defined in the shipped Runtime. The
    // long deadline makes a wake necessarily notification-driven rather than
    // an accidental 250ms fallback. The hook signals *after* the waiter
    // acquired m_mutex and immediately before wait_until releases it.
    void armStaticWaitObserverForTest(std::function<void()> observer)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_nextStaticProbe = Clock::now() + std::chrono::hours(1);
        m_testStaticWaitObserver = std::move(observer);
    }
#endif

    // Outcome of one scheduler wait. `IdleRelease` is a real transition, not a
    // capture: the caller must release the source's idle pixel/GPU storage on
    // this same scheduler thread and then wait again.
    enum class WaitResult {
        Stopped,
        IdleRelease,
        Capture,
    };

    void start()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopped = false;
        m_viewers = 0;
        m_capturesInFlight = 0;
        m_bootstrapFrameQueued = false;
        m_deliveryBackpressured = false;
        m_noViewerUpdateDemand = false;
        m_nextNoDemandProbe = {};
        m_backpressureEscape = false;
        m_idleReleasePending = false;
        resetIdleLocked();
    }

    void frameQueued(bool usableForServerInit)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            // First ServerInit must always receive a valid geometry, even with zero clients.
            m_bootstrapFrameQueued = usableForServerInit;
            if (!usableForServerInit)
                resetIdleLocked();
            // #996/#967: a clientless bootstrap frame can fill a CaptureSource's
            // idle pixel/GPU cache (GL PBO, D3D11 staging, composite). If no
            // Viewer ever connects there is NO >0->0 transition to latch
            // setViewerCount(), so those SDK-owned bytes would stay pinned for
            // the whole run. Deliver the SAME one-shot IdleRelease from this
            // common gate instead of adding a per-backend viewer counter. The
            // geometry is recorded by the transport before the release, so
            // ServerInit and a later reconnect are unchanged; a Viewer that
            // arrives first makes waitForCapture() drop the stale edge.
            if (usableForServerInit && !m_stopped && m_viewers == 0)
                m_idleReleasePending = true;
        }
        m_changed.notify_all();
    }

    void setViewerCount(std::size_t viewers)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_viewers != viewers) {
                resetIdleLocked(); // every new viewer requires a prompt, full initialization
                if (viewers > m_viewers)
                    m_backpressureEscape = true;
                // #790: the >0 -> 0 edge is the ONE product signal that no Server
                // can consume new pixels. It is latched, so the release still runs
                // if the scheduler was not already parked in waitForCapture().
                if (viewers == 0)
                    m_idleReleasePending = true;
            }
            m_viewers = viewers;
        }
        m_changed.notify_all();
    }

    // Called from the RFB worker after its *existing*, exact changedRfbTiles() check.
    // This avoids computing a duplicate 1920x1080 comparison in the Qt GUI capture path.
    void noteFrameChange(bool changed)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopped || m_viewers == 0)
                return;
            if (changed) {
                resetIdleLocked();
            } else {
                if (m_identicalFrames < kConsecutiveUnchangedFrames)
                    ++m_identicalFrames;
                if (m_identicalFrames >= kConsecutiveUnchangedFrames)
                    m_nextStaticProbe = Clock::now() + kStaticProbeInterval;
            }
        }
        m_changed.notify_all();
    }

    // Backpressure reflects Qt's *queued socket bytes*, not a measured kernel ACK
    // or physical link rate. Only the producer-backed Runtime enables this path.
    // It is a hint to stop expensive captures while every Normal viewer is busy
    // draining an already encoded framebuffer. New viewers and real remote input
    // may each escape it once to preserve first-frame/input responsiveness.
    void noteDeliveryBackpressure(bool blocked, bool noViewerUpdateDemand = false)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_deliveryBackpressured = blocked;
            // A Viewer that has no outstanding RFB update request may resume
            // later without sending a second request. Keep a bounded periodic
            // freshness probe in that state. When Qt's socket/encoder is busy,
            // no probe is useful: the already queued output must drain first.
            const bool noDemand = blocked && noViewerUpdateDemand;
            if (noDemand && !m_noViewerUpdateDemand)
                m_nextNoDemandProbe = Clock::now() + kStaticProbeInterval;
            if (!noDemand)
                m_nextNoDemandProbe = {};
            m_noViewerUpdateDemand = noDemand;
            if (!blocked)
                m_backpressureEscape = false;
        }
        m_changed.notify_all();
    }

    // A non-incremental RFB request explicitly asks for a full refresh.
    // Without this escape, successive full requests can be served from the
    // last cached source frame while an encoder/network-constrained viewer
    // keeps the producer asleep. Allow one new capture per explicit request,
    // still subject to Core's existing 60fps and in-flight bounds.
    void noteForcedViewerRefresh()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopped || m_viewers == 0)
                return;
            resetIdleLocked();
            m_backpressureEscape = true;
        }
        m_changed.notify_all();
    }

    void noteRemoteInput()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            resetIdleLocked(); // input-to-remote pixels must not wait for an idle probe
            m_backpressureEscape = true;
        }
        m_changed.notify_all();
    }

    // Qt 5.15 and Qt 6.8 both emit QQuickWindow::afterAnimating on the
    // GUI thread before scenegraph synchronization. This is only an ACTIVITY
    // hint, never trustworthy damage or proof of a different pixel.
    //
    // A public grabToImage() itself schedules a Qt update. Suppress render
    // hints while a Core-authorized capture is outstanding or the grab would
    // retrigger itself forever at 60fps on an otherwise static surface.
    // A genuine UI change concurrent with a grab remains covered by the
    // captured pixels or the existing bounded 250ms freshness probe.
    void noteCaptureStarted()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_stopped)
            ++m_capturesInFlight;
    }

    void noteCaptureCompleted()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_capturesInFlight != 0)
            --m_capturesInFlight;
    }

    ActivityResult noteQtRenderActivity()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopped)
                return ActivityResult::Stopped;
            if (m_viewers == 0)
                return ActivityResult::NoViewer;
            if (m_capturesInFlight != 0)
                return ActivityResult::CaptureInFlight;
            // A render hint is NOT an explicit Viewer refresh request.
            // It must never spend the single-use backpressure escape.
            if (m_deliveryBackpressured)
                return ActivityResult::DeliveryBackpressure;
            // Already-active capture should not be rearmed by the Qt
            // render loop itself; this was causing even unchanged scenes
            // to stay in the 60fps mode just because Qt kept rendering.
            if (m_identicalFrames < kConsecutiveUnchangedFrames)
                return ActivityResult::AlreadyActive;
            resetIdleLocked();
        }
        m_changed.notify_all();
        return ActivityResult::ReleasedStaticWait;
    }

    WaitResult waitForCapture()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        // Never block while holding a GUI/rendering/transport lock. Core calls this only from
        // its capture scheduler, and stop() always wakes it before joining Core workers.
        for (;;) {
            if (m_stopped)
                return WaitResult::Stopped;
            // A latched zero-viewer edge is delivered exactly once, on this
            // thread, before the producer is allowed to sleep again. A viewer
            // that reconnected in between needs fresh pixels, not a release,
            // so the stale edge is simply dropped.
            if (m_idleReleasePending) {
                m_idleReleasePending = false;
                if (m_viewers == 0)
                    return WaitResult::IdleRelease;
            }
            if (m_viewers == 0) {
                if (!m_bootstrapFrameQueued)
                    return WaitResult::Capture; // one valid bootstrap frame before any RFB ClientConnected
                m_changed.wait(lock);
                continue;
            }
            if (m_deliveryBackpressured) {
                if (!m_backpressureEscape) {
                    if (m_noViewerUpdateDemand) {
                        const auto now = Clock::now();
                        if (now >= m_nextNoDemandProbe) {
                            m_nextNoDemandProbe = now + kStaticProbeInterval;
                            return WaitResult::Capture; // bound stale cached pixels for later requests
                        }
                        m_changed.wait_until(lock, m_nextNoDemandProbe);
                    } else {
                        m_changed.wait(lock); // encoding/network output cannot consume more
                    }
                    continue;
                }
                m_backpressureEscape = false; // at most one extra frame per input/join event
            } else {
                m_backpressureEscape = false;
            }
            if (m_identicalFrames < kConsecutiveUnchangedFrames)
                return WaitResult::Capture; // active/changing frames retain the existing 60fps cap

            const auto now = Clock::now();
            if (now >= m_nextStaticProbe) {
                m_nextStaticProbe = now + kStaticProbeInterval;
                return WaitResult::Capture; // local-only changes have bounded detection delay
            }
#if defined(HYREMOTE_VIEWER_DEMAND_TEST_HOOK)
            if (m_testStaticWaitObserver) {
                auto observer = std::move(m_testStaticWaitObserver);
                observer(); // test callback only signals a promise; never reenters this gate
            }
#endif
            m_changed.wait_until(lock, m_nextStaticProbe);
        }
    }

    // Boolean view of the same gate for callers/tests that only need "may I
    // capture?". A latched idle release is consumed here, never turned into a
    // capture or a busy loop.
    bool waitForViewer()
    {
        for (;;) {
            switch (waitForCapture()) {
            case WaitResult::Stopped:
                return false;
            case WaitResult::Capture:
                return true;
            case WaitResult::IdleRelease:
                continue;
            }
        }
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopped = true;
            m_viewers = 0;
            m_capturesInFlight = 0;
            m_deliveryBackpressured = false;
            m_noViewerUpdateDemand = false;
            m_nextNoDemandProbe = {};
            m_backpressureEscape = false;
            // stop() frees the source itself; a pending idle release must not
            // leak into the next run.
            m_idleReleasePending = false;
            resetIdleLocked();
        }
        m_changed.notify_all();
    }

private:
    using Clock = std::chrono::steady_clock;

    void resetIdleLocked()
    {
        m_identicalFrames = 0;
        m_nextStaticProbe = Clock::time_point{};
    }

    std::mutex m_mutex;
    std::condition_variable m_changed;
    std::size_t m_viewers = 0;
    std::size_t m_capturesInFlight = 0;
    std::size_t m_identicalFrames = 0;
    Clock::time_point m_nextStaticProbe{};
    bool m_bootstrapFrameQueued = false;
    bool m_deliveryBackpressured = false;
    bool m_noViewerUpdateDemand = false;
    Clock::time_point m_nextNoDemandProbe{};
    bool m_backpressureEscape = false;
    bool m_idleReleasePending = false;
    bool m_stopped = true;
#if defined(HYREMOTE_VIEWER_DEMAND_TEST_HOOK)
    std::function<void()> m_testStaticWaitObserver;
#endif
};

}  // namespace HyRemote::detail
