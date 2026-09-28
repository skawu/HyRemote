// #400: the shared, Runtime-private pointer click-sequence classifier used by both Qt target
// adapters. The public-Qt ingress (QCoreApplication::sendEvent) sits above the window-system mouse
// processing that lets Qt classify a press as a double click, so this helper reproduces the
// mechanics Qt 6.8.3 applies in QGuiApplicationPrivate::processMouseEvent() and the adapters append
// one QEvent::MouseButtonDblClick after the ordinary qualifying second press.
//
// Mechanics mirrored from Qt 6.8.3 (pinned by the deterministic classifier test):
//   - time:       acceptedAt >= trace.acceptedAt (backwards time never qualifies) and
//                 (acceptedAt - trace.acceptedAt) < mouseDoubleClickInterval().
//                 The delta is an unsigned timestamp difference in Qt, so a 0 ms delta is eligible:
//                 0 <= delta < interval.
//   - distance:   abs(dx) <= mouseDoubleClickDistance AND abs(dy) <= mouseDoubleClickDistance
//                 (per axis, not a radius); distance == 0 therefore allows no non-zero movement
//                 at all and is never treated as "unconstrained".
//   - movement:   the accepted input stream invalidates the pending pair when any accepted move
//                 between the two presses left the distance box. The bound is the accepted-stream
//                 movement span (see accepted_pointer_motion.hpp), not just the last delivered
//                 coordinate, so a far excursion cannot be hidden by pointer-move coalescing.
//                 Coming back inside the box never re-arms the pair.
//   - viewport:   a source-viewport change between the two presses invalidates the pair instead of
//                 mixing two coordinate spaces (#400 does not invent resize-gesture semantics).
//   - button:     one single pending trace; any press of a different button cuts the old pair
//   - identity:   an opaque caller-owned token (the resolved QWidget receiver on the Widgets route,
//                 the configured QQuickWindow on the Quick route) is part of the condition
//   - lifecycle:  a qualifying press consumes the pair and the trace is disarmed, so the next press
//                 is ordinary and only the one after it may start the following pair
//
// The delivered sequence is Press, Release, Press, DblClick, Release: the qualifying second press is
// still delivered normally and the DblClick is appended immediately after it.
//
// The helper knows nothing about controls: no QPushButton/QComboBox/QListView/MouseArea knowledge,
// no per-control branches.

#pragma once

#include <QPointF>
#include <QtGlobal>

#include <chrono>
#include <cmath>

#include "hyremote/core/types.hpp"

namespace HyRemote::detail {

// One pending click. GUI-thread-owned by the adapters; a single instance per adapter, matching Qt's
// single "current press" classification state rather than a per-button table.
struct PointerClickTrace
{
    int button = 0;  // Qt::MouseButton value
    hyremote::TimePoint acceptedAt{};
    QPointF position;
    quintptr identity = 0;
    bool armed = false;
};

// Qt's declared double-click policy. intervalMs <= 0 disables classification; distancePx is applied
// per axis exactly as declared (0 means "no movement allowed", never "unconstrained").
struct DoubleClickPolicy
{
    int intervalMs = 0;
    int distancePx = 0;
};

// Accepted movement between two presses, expressed in target logical coordinates. `minimum` and
// `maximum` are the per-axis extremes of every accepted move of that period (mapping to target space
// is monotone per axis, so the extremes are enough; no move is stored individually).
struct PointerMovementSpan
{
    bool hasMovement = false;
    QPointF minimum;
    QPointF maximum;
    bool viewportChanged = false;
};

inline bool isDoubleClickPress(const PointerClickTrace &trace,
                               int button,
                               hyremote::TimePoint acceptedAt,
                               const QPointF &position,
                               quintptr identity,
                               const DoubleClickPolicy &policy,
                               const PointerMovementSpan &movement = PointerMovementSpan{})
{
    if (!trace.armed || policy.intervalMs <= 0)
        return false;
    if (button != trace.button || identity != trace.identity)
        return false;
    // Backwards time never qualifies; a same-millisecond delta does, matching Qt's unsigned
    // timestamp comparison.
    if (acceptedAt < trace.acceptedAt)
        return false;
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(acceptedAt - trace.acceptedAt);
    if (elapsed >= std::chrono::milliseconds(policy.intervalMs))
        return false;
    if (movement.viewportChanged)
        return false;

    const qreal distance = qreal(policy.distancePx);
    const QPointF delta = position - trace.position;
    if (std::abs(delta.x()) > distance || std::abs(delta.y()) > distance)
        return false;

    if (!movement.hasMovement)
        return true;
    // Any accepted excursion beyond the box invalidates the pair for good.
    return std::abs(movement.minimum.x() - trace.position.x()) <= distance
           && std::abs(movement.maximum.x() - trace.position.x()) <= distance
           && std::abs(movement.minimum.y() - trace.position.y()) <= distance
           && std::abs(movement.maximum.y() - trace.position.y()) <= distance;
}

// A move that leaves the per-axis distance box clears the pending eligibility, exactly as it does
// for physical input. This covers the promptly-drained case; the queued case is covered by the
// accepted-stream span above.
inline bool moveInvalidatesTrace(const PointerClickTrace &trace,
                                 const QPointF &position,
                                 const DoubleClickPolicy &policy)
{
    if (!trace.armed)
        return false;
    const QPointF delta = position - trace.position;
    return std::abs(delta.x()) > qreal(policy.distancePx) || std::abs(delta.y()) > qreal(policy.distancePx);
}

inline void notePointerMove(PointerClickTrace &trace,
                            const QPointF &position,
                            const DoubleClickPolicy &policy)
{
    if (moveInvalidatesTrace(trace, position, policy))
        trace.armed = false;
}

// Every press records itself as the pending click, so an intervening press of another button
// replaces the pair instead of extending it.
inline void armPointerClickTrace(PointerClickTrace &trace,
                                 int button,
                                 hyremote::TimePoint acceptedAt,
                                 const QPointF &position,
                                 quintptr identity)
{
    trace.button = button;
    trace.acceptedAt = acceptedAt;
    trace.position = position;
    trace.identity = identity;
    trace.armed = true;
}

// A completed double click consumes the pair: the trace stays invalid until the next ordinary press.
inline void disarmPointerClickTrace(PointerClickTrace &trace)
{
    trace.armed = false;
}

inline void clearPointerClickTrace(PointerClickTrace &trace)
{
    trace = PointerClickTrace{};
}

}  // namespace HyRemote::detail
