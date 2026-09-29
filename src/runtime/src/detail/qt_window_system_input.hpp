#pragma once

#include <QPointF>
#include <Qt>

#include <chrono>

#include "hyremote/core/input.hpp"

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

namespace HyRemote::detail {

// State that belongs to the remote pointer stream, not to any QWidget/QQuickItem.
// Qt owns hit testing, grabs, click classification, hover and control semantics after ingress.
struct QtWindowSystemPointerState
{
    Qt::MouseButtons buttons = Qt::NoButton;
    QPointF lastLocalPoint;
    bool positionKnown = false;
};

namespace accepted_timestamp_detail {
inline thread_local bool hasTimestamp = false;
inline thread_local unsigned long timestamp = 0;
}  // namespace accepted_timestamp_detail

// Composite Runtime layers may need GUI-thread surface arbitration before they can call a normal
// leaf input sink. This scope lets that leaf keep using qtWindowSystemTimestamp() while observing
// the timestamp captured when the composite originally accepted the remote fact. It is strictly
// thread-local and Runtime-private; no timestamp enters the public normalized-input contract.
class QtWindowSystemAcceptedTimestampScope final
{
public:
    explicit QtWindowSystemAcceptedTimestampScope(unsigned long timestamp) noexcept
        : m_previousTimestamp(accepted_timestamp_detail::timestamp)
        , m_hadPreviousTimestamp(accepted_timestamp_detail::hasTimestamp)
    {
        accepted_timestamp_detail::timestamp = timestamp;
        accepted_timestamp_detail::hasTimestamp = true;
    }

    ~QtWindowSystemAcceptedTimestampScope()
    {
        accepted_timestamp_detail::timestamp = m_previousTimestamp;
        accepted_timestamp_detail::hasTimestamp = m_hadPreviousTimestamp;
    }

    QtWindowSystemAcceptedTimestampScope(const QtWindowSystemAcceptedTimestampScope &) = delete;
    QtWindowSystemAcceptedTimestampScope &operator=(const QtWindowSystemAcceptedTimestampScope &) = delete;

private:
    unsigned long m_previousTimestamp = 0;
    bool m_hadPreviousTimestamp = false;
};

// Capture the remote fact's monotonic acceptance time before GUI queue delay. While an accepted
// timestamp scope is active, return that already-captured value instead of sampling the clock again.
inline unsigned long qtWindowSystemTimestamp() noexcept
{
    if (accepted_timestamp_detail::hasTimestamp)
        return accepted_timestamp_detail::timestamp;

    // QWindowSystemInterface only requires a monotonic timestamp domain for relative input timing.
    // Keep remote input on its own steady-clock domain: consecutive remote facts preserve their
    // real acceptance spacing, while a local physical press can never accidentally combine with a
    // remote press into one double click merely because both target the same window.
    using namespace std::chrono;
    const auto now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    return static_cast<unsigned long>(now);
}

// Deliver one transport-neutral pointer fact at Qt's window-system boundary.
// local/global are logical Qt coordinates; the implementation performs the private native-pixel
// conversion required by QWindowSystemInterface and deliberately does not synthesize Qt semantics.
void deliverQtWindowSystemPointer(QWindow *window,
                                  const hyremote::InputEvent &event,
                                  const QPointF &local,
                                  const QPointF &global,
                                  unsigned long timestamp,
                                  QtWindowSystemPointerState &state);

// Balance only button state that actually reached Qt. Used during terminal Runtime teardown.
void releaseQtWindowSystemPointer(QWindow *window,
                                  unsigned long timestamp,
                                  QtWindowSystemPointerState &state) noexcept;

}  // namespace HyRemote::detail
