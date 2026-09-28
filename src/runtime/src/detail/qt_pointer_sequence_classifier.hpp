// #400: the shared, Runtime-private pointer click-sequence classifier used by both Qt target
// adapters. It exists because the public-Qt ingress cannot inherit the classification Qt performs
// itself (see docs/internal/q401-input-backend-preflight.md): direct delivery
// (QCoreApplication::sendEvent) sits above Qt's window-system mouse processing, so the adapter has to
// hand Qt the semantic it would otherwise have produced.
//
// Ownership stays deliberately finite - this helper knows nothing about controls:
//   - it classifies one press against the previous press of the same button only;
//   - the policy comes from Qt's own style hints (QStyleHints::mouseDoubleClickInterval /
//     mouseDoubleClickDistance); no product constants are invented here;
//   - the caller supplies an opaque identity token (the resolved QWidget receiver on the Widgets
//     route, the configured QQuickWindow on the Quick route) and owns that token's lifetime.
//
// Timing uses the accepted-arrival timestamp captured when the event entered the adapter's bounded
// mailbox, never the GUI thread's later delivery time: a busy GUI thread must not be able to split a
// real double click into two single clicks, and two events accepted far apart must not become a
// double click just because they were drained in the same batch.
//
// Alignment notes (values and shape taken from Qt's public policy; conventions pinned by the
// boundary tests in the Widgets/Quick preflight fixtures):
//   - bounds are inclusive on both dimensions (elapsed <= interval, distance <= distance);
//   - distance is the Euclidean distance between the two press positions;
//   - a qualifying press consumes the pair (see armPointerClickTrace), mirroring the paired-press
//     model instead of emitting DblClick for every subsequent rapid press.

#pragma once

#include <QPointF>
#include <QtGlobal>

#include <cmath>

#include "hyremote/core/types.hpp"

namespace HyRemote::detail {

// Accepted-arrival bookkeeping for the previous press of one logical pointer button.
// GUI-thread-owned by the adapters; one instance per supported pointer button.
struct PointerClickTrace
{
    hyremote::TimePoint acceptedAt{};
    QPointF position;
    quintptr identity = 0;
    bool armed = false;
};

// Qt's declared double-click policy. intervalMs <= 0 disables classification entirely;
// distancePx <= 0 leaves the distance unconstrained (the platform theme declared none).
struct DoubleClickPolicy
{
    int intervalMs = 0;
    int distancePx = 0;
};

inline bool isDoubleClickPress(const PointerClickTrace &trace,
                               hyremote::TimePoint acceptedAt,
                               const QPointF &position,
                               quintptr identity,
                               const DoubleClickPolicy &policy)
{
    if (!trace.armed || policy.intervalMs <= 0)
        return false;
    if (identity != trace.identity)
        return false;
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(acceptedAt - trace.acceptedAt);
    if (elapsed.count() < 0 || elapsed > std::chrono::milliseconds(policy.intervalMs))
        return false;
    if (policy.distancePx > 0) {
        const QPointF delta = position - trace.position;
        if (std::hypot(delta.x(), delta.y()) > qreal(policy.distancePx))
            return false;
    }
    return true;
}

// Records the press that classification will compare the next one against. A press that produced a
// double click consumes the pair (armed stays false), so the next rapid press is a fresh single
// press rather than another DblClick.
inline void armPointerClickTrace(PointerClickTrace &trace,
                                 hyremote::TimePoint acceptedAt,
                                 const QPointF &position,
                                 quintptr identity,
                                 bool consumedByDoubleClick)
{
    trace.acceptedAt = acceptedAt;
    trace.position = position;
    trace.identity = identity;
    trace.armed = !consumedByDoubleClick;
}

// Lifecycle: a trace must never survive a target change or a session boundary. Clearing one trace
// entry (or all of them on shutdown) is all the state there is.
inline void clearPointerClickTrace(PointerClickTrace &trace)
{
    trace = PointerClickTrace{};
}

}  // namespace HyRemote::detail
