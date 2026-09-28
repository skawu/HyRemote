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

// Capture Qt's own monotonic window-system timestamp when Runtime accepts the remote fact.
// Keeping the timestamp in Qt's domain makes GUI queue delay invisible to Qt's click policy.
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
