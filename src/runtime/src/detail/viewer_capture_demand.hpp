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

    void start()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopped = false;
        m_viewers = 0;
        m_bootstrapFrameQueued = false;
        m_deliveryBackpressured = false;
        m_backpressureEscape = false;
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
    void noteDeliveryBackpressure(bool blocked)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_deliveryBackpressured = blocked;
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

    bool waitForViewer()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        // Never block while holding a GUI/rendering/transport lock. Core calls this only from
        // its capture scheduler, and stop() always wakes it before joining Core workers.
        for (;;) {
            if (m_stopped)
                return false;
            if (m_viewers == 0) {
                if (!m_bootstrapFrameQueued)
                    return true; // one valid bootstrap frame before any RFB ClientConnected
                m_changed.wait(lock);
                continue;
            }
            if (m_deliveryBackpressured) {
                if (!m_backpressureEscape) {
                    m_changed.wait(lock);
                    continue;
                }
                m_backpressureEscape = false; // at most one extra frame per input/join event
            } else {
                m_backpressureEscape = false;
            }
            if (m_identicalFrames < kConsecutiveUnchangedFrames)
                return true; // active/changing frames retain the existing 60fps cap

            const auto now = Clock::now();
            if (now >= m_nextStaticProbe) {
                m_nextStaticProbe = now + kStaticProbeInterval;
                return true; // local-only changes have bounded detection delay
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

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopped = true;
            m_viewers = 0;
            m_deliveryBackpressured = false;
            m_backpressureEscape = false;
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
    std::size_t m_identicalFrames = 0;
    Clock::time_point m_nextStaticProbe{};
    bool m_bootstrapFrameQueued = false;
    bool m_deliveryBackpressured = false;
    bool m_backpressureEscape = false;
    bool m_stopped = true;
#if defined(HYREMOTE_VIEWER_DEMAND_TEST_HOOK)
    std::function<void()> m_testStaticWaitObserver;
#endif
};

}  // namespace HyRemote::detail
