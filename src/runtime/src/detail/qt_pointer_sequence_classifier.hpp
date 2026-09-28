// #400: the shared, Runtime-private pointer click-sequence classifier used by both Qt target
// adapters. The public-Qt ingress (QCoreApplication::sendEvent) sits above the window-system mouse
// processing that lets Qt classify a press as a double click, so this helper reproduces the
// mechanics Qt's own mouse processing applies and the adapters deliver the extra
// QEvent::MouseButtonDblClick after the ordinary second press.
//
// Mechanics mirrored from Qt (see the deterministic classifier test for the pinned rules):
//   - time:       0 < (acceptedAt - trace.acceptedAt) < mouseDoubleClickInterval()
//                 (strict on the upper bound, and a non-positive delta never qualifies)
//   - distance:   abs(dx) <= mouseDoubleClickDistance AND abs(dy) <= mouseDoubleClickDistance
//                 (per axis, not a radius); distance == 0 therefore allows no non-zero movement
//                 at all and is never treated as "unconstrained"
//   - movement:   a pointer move that leaves the distance box invalidates the pending pair, so a
//                 far move and a return cannot form a double click
//   - button:     one single pending trace; any press of a different button cuts the old pair
//   - identity:   an opaque caller-owned token (the resolved QWidget receiver on the Widgets route,
//                 the configured QQuickWindow on the Quick route) is part of the condition
//   - lifecycle:  a qualifying press consumes the pair and the trace is disarmed, so the next press
//                 is ordinary and only the one after it may start the following pair
//
// The helper knows nothing about controls: no QPushButton/QComboBox/QListView/MouseArea knowledge,
// no per-control branches.

#pragma once

#include <QPointF>
#include <QtGlobal>

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

inline bool isDoubleClickPress(const PointerClickTrace &trace,
                               int button,
                               hyremote::TimePoint acceptedAt,
                               const QPointF &position,
                               quintptr identity,
                               const DoubleClickPolicy &policy)
{
    if (!trace.armed || policy.intervalMs <= 0)
        return false;
    if (button != trace.button || identity != trace.identity)
        return false;
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(acceptedAt - trace.acceptedAt);
    if (elapsed.count() <= 0 || elapsed >= std::chrono::milliseconds(policy.intervalMs))
        return false;
    const QPointF delta = position - trace.position;
    return std::abs(delta.x()) <= qreal(policy.distancePx) && std::abs(delta.y()) <= qreal(policy.distancePx);
}

// A move that leaves the per-axis distance box clears the pending eligibility, exactly as it does
// for physical input.
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
