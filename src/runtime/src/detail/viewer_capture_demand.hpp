#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>

// Private Shared Runtime demand gate. Never changes Core Session scheduling or exposes a public
// integration knob: it suppresses unnecessary Qt captures after RFB has compared *served* pixels.
namespace HyRemote::detail {

class ViewerCaptureDemand final
{
public:
    // A local-only Qt Quick page change still becomes visible through the next bounded probe.
    // Do not rely solely on QQuickWindow rendering signals: offscreen grabs can generate them.
    // #789 diagnostic A/B ONLY: 250ms reduces idle Quick grab/readback load
    // but can defer local-only page changes by up to 150ms beyond the 100ms
    // product baseline. DO NOT MERGE without matched CPU+freshness acceptance.
    static constexpr auto kStaticProbeInterval = std::chrono::milliseconds(250);
    static constexpr std::size_t kConsecutiveUnchangedFrames = 3;

    void start()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopped = false;
        m_viewers = 0;
        m_bootstrapFrameQueued = false;
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
            if (m_viewers != viewers)
                resetIdleLocked(); // every new viewer requires a prompt, full initialization
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

    void noteRemoteInput()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            resetIdleLocked(); // input-to-remote pixels must not wait for an idle probe
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
            if (m_identicalFrames < kConsecutiveUnchangedFrames)
                return true; // active/changing frames retain the existing 60fps cap

            const auto now = Clock::now();
            if (now >= m_nextStaticProbe) {
                m_nextStaticProbe = now + kStaticProbeInterval;
                return true; // local-only changes have bounded detection delay
            }
            m_changed.wait_until(lock, m_nextStaticProbe);
        }
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopped = true;
            m_viewers = 0;
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
    bool m_stopped = true;
};

}  // namespace HyRemote::detail
