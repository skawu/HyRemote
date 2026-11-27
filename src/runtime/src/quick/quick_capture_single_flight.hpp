#pragma once

// One Qt Quick target may have many Core request IDs but must never submit
// overlapping, independently completing public grabToImage() operations.
// This state is owned and protected by QuickCaptureSource::State::mutex,
// never accessed unsynchronised by GUI, Core scheduler or timer callbacks.
namespace HyRemote::detail {

class QuickCaptureSingleFlight final
{
public:
    bool needsDispatch() const noexcept
    {
        return !m_grabInProgress && !m_dispatchScheduled;
    }

    void dispatchPosted() noexcept { m_dispatchScheduled = true; }

    // Called only by the GUI event already posted for this generation.
    bool beginDispatchedGrab() noexcept
    {
        m_dispatchScheduled = false;
        if (m_grabInProgress)
            return false;
        m_grabInProgress = true;
        return true;
    }

    // Hold the slot through bounded unavailability retries. A later Core
    // request must be served by a NEW grab, after the earlier batch completes,
    // rather than borrowing its potentially stale image.
    void grabCompleted() noexcept { m_grabInProgress = false; }

    void reset() noexcept
    {
        m_dispatchScheduled = false;
        m_grabInProgress = false;
    }

    bool grabInProgress() const noexcept { return m_grabInProgress; }

private:
    bool m_dispatchScheduled = false;
    bool m_grabInProgress = false;
};

} // namespace HyRemote::detail
