#pragma once

#include <QPointF>
#include <Qt>

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

// Composite Runtime layers may need GUI-thread surface arbitration before they can call a normal
// leaf input sink. This scope lets that leaf keep using qtWindowSystemTimestamp() while observing
// the timestamp captured when the composite originally accepted the remote fact. It is strictly
// thread-local and Runtime-private; no timestamp enters the public normalized-input contract.
class QtWindowSystemAcceptedTimestampScope final
{
public:
    explicit QtWindowSystemAcceptedTimestampScope(unsigned long timestamp) noexcept;
    ~QtWindowSystemAcceptedTimestampScope();

    QtWindowSystemAcceptedTimestampScope(const QtWindowSystemAcceptedTimestampScope &) = delete;
    QtWindowSystemAcceptedTimestampScope &operator=(const QtWindowSystemAcceptedTimestampScope &) = delete;

private:
    unsigned long m_previousTimestamp = 0;
    bool m_hadPreviousTimestamp = false;
};

// Capture the remote fact's monotonic acceptance time before GUI queue delay. While an accepted
// timestamp scope is active, return that already-captured value instead of sampling the clock again.
unsigned long qtWindowSystemTimestamp() noexcept;

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
