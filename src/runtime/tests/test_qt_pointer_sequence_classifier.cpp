// #400: deterministic unit coverage for the shared Runtime-private click-sequence classifier.
//
// The policy boundaries are asserted with synthetic TimePoints and QPointF positions instead of real
// sleeps, so the exact rules - 0 <= delta < interval with backwards time rejected, per-axis distance,
// zero-distance meaning, accepted-movement invalidation, viewport change, button mismatch, identity
// mismatch and pair consumption - are pinned without depending on wall-clock timing or the host's
// configured double-click speed.

#include <QCoreApplication>
#include <QPointF>

#include <chrono>
#include <cstdio>

#include "detail/qt_pointer_sequence_classifier.hpp"

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++failures;
}

using hyremote::TimePoint;
using HyRemote::detail::DoubleClickPolicy;
using HyRemote::detail::PointerClickTrace;
using HyRemote::detail::PointerMovementSpan;

constexpr int kIntervalMs = 400;
constexpr int kDistancePx = 5;
constexpr quintptr kIdentityA = 0x1000;
constexpr quintptr kIdentityB = 0x2000;
constexpr int kLeft = 1;
constexpr int kRight = 2;

const QPointF kOrigin(100.0, 100.0);

TimePoint at(int milliseconds)
{
    return TimePoint{} + std::chrono::milliseconds(milliseconds);
}

DoubleClickPolicy policy()
{
    return DoubleClickPolicy{kIntervalMs, kDistancePx};
}

PointerClickTrace armedTrace(int button = kLeft, int acceptedMs = 0, const QPointF &position = kOrigin,
                             quintptr identity = kIdentityA)
{
    PointerClickTrace trace;
    HyRemote::detail::armPointerClickTrace(trace, button, at(acceptedMs), position, identity);
    return trace;
}

bool qualifies(const PointerClickTrace &trace, int acceptedMs, const QPointF &position, int button = kLeft,
               quintptr identity = kIdentityA, const PointerMovementSpan &movement = PointerMovementSpan{})
{
    return HyRemote::detail::isDoubleClickPress(trace, button, at(acceptedMs), position, identity, policy(),
                                               movement);
}

PointerMovementSpan movementBetween(const QPointF &minimum, const QPointF &maximum)
{
    PointerMovementSpan span;
    span.hasMovement = true;
    span.minimum = minimum;
    span.maximum = maximum;
    return span;
}

void testIntervalBoundary()
{
    const PointerClickTrace trace = armedTrace();
    check(qualifies(trace, 0, kOrigin),
          "delta = 0ms is eligible (Qt compares unsigned timestamps, so a same-millisecond pair counts)");
    check(qualifies(trace, 1, kOrigin), "delta = 1ms is eligible");
    check(qualifies(trace, kIntervalMs - 1, kOrigin), "delta = interval - 1ms is eligible");
    check(!qualifies(trace, kIntervalMs, kOrigin), "delta = interval is NOT eligible (strict upper bound)");
    check(!qualifies(trace, kIntervalMs + 1, kOrigin), "delta = interval + 1ms is NOT eligible");
    check(!qualifies(trace, -1, kOrigin), "a backwards synthetic TimePoint is NOT eligible");
    check(!qualifies(trace, -kIntervalMs, kOrigin), "a far backwards TimePoint is NOT eligible");
}

void testDistanceBoundary()
{
    const PointerClickTrace trace = armedTrace();
    const double d = static_cast<double>(kDistancePx);
    check(qualifies(trace, 10, kOrigin + QPointF(d, d)),
          "dx = distance and dy = distance is eligible (per-axis bound, both axes at the limit)");
    check(qualifies(trace, 10, kOrigin + QPointF(d, 0)), "dx = distance, dy = 0 is eligible");
    check(!qualifies(trace, 10, kOrigin + QPointF(d + 1, 0)), "dx = distance + 1 is NOT eligible");
    check(!qualifies(trace, 10, kOrigin + QPointF(0, d + 1)), "dy = distance + 1 is NOT eligible");
    check(!qualifies(trace, 10, kOrigin + QPointF(d + 1, d + 1)),
          "both axes beyond distance is NOT eligible");
}

void testZeroDistance()
{
    PointerClickTrace trace;
    const DoubleClickPolicy zeroDistance{kIntervalMs, 0};
    HyRemote::detail::armPointerClickTrace(trace, kLeft, at(0), kOrigin, kIdentityA);
    check(HyRemote::detail::isDoubleClickPress(trace, kLeft, at(10), kOrigin, kIdentityA, zeroDistance),
          "distance = 0 accepts a press with no movement at all");
    check(!HyRemote::detail::isDoubleClickPress(trace, kLeft, at(10), kOrigin + QPointF(1, 0), kIdentityA,
                                               zeroDistance),
          "distance = 0 rejects any non-zero movement (never treated as unconstrained)");
}

void testAcceptedMovementSpan()
{
    const PointerClickTrace trace = armedTrace();
    const double d = static_cast<double>(kDistancePx);

    // Far out and back: the press positions themselves qualify, but the accepted excursion does not.
    check(!qualifies(trace, 10, kOrigin, kLeft, kIdentityA,
                     movementBetween(kOrigin - QPointF(60, 60), kOrigin + QPointF(60, 60))),
          "an accepted excursion beyond the distance box invalidates the pair even when the press "
          "returns to the original position");
    check(!qualifies(trace, 10, kOrigin, kLeft, kIdentityA,
                     movementBetween(kOrigin, kOrigin + QPointF(d + 1, 0))),
          "a single accepted axis excursion beyond the box invalidates the pair");
    check(!qualifies(trace, 10, kOrigin, kLeft, kIdentityA,
                     movementBetween(kOrigin - QPointF(0, d + 1), kOrigin)),
          "the negative direction is bounded the same way");

    // Jitter inside the box must not disable classification.
    check(qualifies(trace, 10, kOrigin, kLeft, kIdentityA,
                    movementBetween(kOrigin - QPointF(d, d), kOrigin + QPointF(d, d))),
          "accepted jitter whose extremes stay inside the box keeps the pair eligible");
    check(qualifies(trace, 10, kOrigin + QPointF(d, 0), kLeft, kIdentityA,
                    movementBetween(kOrigin, kOrigin + QPointF(d, 0))),
          "an in-box excursion combined with an in-box second press is eligible");

    PointerMovementSpan viewportChanged = movementBetween(kOrigin, kOrigin + QPointF(d, 0));
    viewportChanged.viewportChanged = true;
    check(!qualifies(trace, 10, kOrigin + QPointF(d, 0), kLeft, kIdentityA, viewportChanged),
          "a source-viewport change invalidates the pair instead of mixing coordinate spaces");
    check(!qualifies(trace, 10, kOrigin, kLeft, kIdentityA, viewportChanged),
          "a source-viewport change invalidates even a same-position pair");
}

void testDeliveredMoveInvalidation()
{
    PointerClickTrace trace = armedTrace();
    HyRemote::detail::notePointerMove(trace, kOrigin + QPointF(kDistancePx + 10, 0), policy());
    HyRemote::detail::notePointerMove(trace, kOrigin, policy());  // moving back must not restore it
    check(!qualifies(trace, 10, kOrigin),
          "a promptly delivered far move and a return to the original position is NOT eligible");
    check(!trace.armed, "the pending pair stays invalidated after the far move");

    PointerClickTrace inside = armedTrace();
    HyRemote::detail::notePointerMove(inside, kOrigin + QPointF(kDistancePx, kDistancePx), policy());
    check(inside.armed, "an in-box delivered move keeps the pair eligible");
    check(qualifies(inside, 10, kOrigin + QPointF(1, 1)), "an in-box move keeps the pair effective");
}

void testButtonAndIdentity()
{
    const PointerClickTrace trace = armedTrace();
    check(!qualifies(trace, 10, kOrigin, kRight), "a different button never qualifies");
    check(!qualifies(trace, 10, kOrigin, kLeft, kIdentityB), "a different identity never qualifies");
    check(qualifies(trace, 10, kOrigin, kLeft, kIdentityA), "the same button and identity qualifies");

    // left, right, left: the intervening press of another button cuts the old pair.
    PointerClickTrace chained = armedTrace();
    HyRemote::detail::armPointerClickTrace(chained, kRight, at(5), kOrigin, kIdentityA);
    check(!qualifies(chained, 10, kOrigin, kLeft, kIdentityA),
          "left click, right click, left click does NOT form a left double click");
}

void testPairConsumption()
{
    PointerClickTrace trace = armedTrace();
    check(qualifies(trace, 10, kOrigin), "press2 qualifies against press1");
    HyRemote::detail::disarmPointerClickTrace(trace);
    check(!qualifies(trace, 20, kOrigin), "press3 does not qualify after the pair was consumed");
    HyRemote::detail::armPointerClickTrace(trace, kLeft, at(30), kOrigin, kIdentityA);
    check(qualifies(trace, 40, kOrigin), "press4 may form the next double click with press3");
}

void testLifecycle()
{
    PointerClickTrace trace = armedTrace();
    HyRemote::detail::clearPointerClickTrace(trace);
    check(!trace.armed && !qualifies(trace, 10, kOrigin),
          "cleared state cannot pair across a session or target change");
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    testIntervalBoundary();
    testDistanceBoundary();
    testZeroDistance();
    testAcceptedMovementSpan();
    testDeliveredMoveInvalidation();
    testButtonAndIdentity();
    testPairConsumption();
    testLifecycle();

    std::printf("%s: qt pointer sequence classifier (%d checks failed)\n",
                failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
