// #400: deterministic coverage for the Runtime-private accepted-stream motion bookkeeping.
//
// This is the level the GUI-thread classifier cannot see: what the accepted input stream actually
// contained between two accepted presses, after coalescing and eviction may have dropped moves. The
// cases below pin the period viewport truth - including the transient viewport excursion
// `press A -> move B -> move A -> press A` that must stay fail-closed - and prove the state does not
// leak into the next click period.
//
// The state must stay O(1): one viewport plus four extrema, never a per-move record.

#include <QCoreApplication>

#include <cstdint>
#include <cstdio>

#include "detail/accepted_pointer_motion.hpp"

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++failures;
}

using HyRemote::detail::AcceptedPointerMotionState;
using HyRemote::detail::PointerMovementSummary;
using HyRemote::detail::movementSpanForTarget;

constexpr std::uint32_t kWidthA = 500;
constexpr std::uint32_t kHeightA = 600;
constexpr std::uint32_t kWidthB = 1000;
constexpr std::uint32_t kHeightB = 1200;

hyremote::InputEvent viewportEvent(hyremote::InputEventKind kind, std::uint32_t width, std::uint32_t height,
                                   float dpr, float x, float y, bool pressed = false)
{
    hyremote::InputEvent event;
    event.kind = kind;
    event.sourceViewport.width = width;
    event.sourceViewport.height = height;
    event.sourceViewport.devicePixelRatio = dpr;
    event.x = x;
    event.y = y;
    event.pressed = pressed;
    return event;
}

hyremote::InputEvent pressA(float x = 100.0F, float y = 100.0F)
{
    return viewportEvent(hyremote::InputEventKind::PointerButton, kWidthA, kHeightA, 1.0F, x, y, true);
}

hyremote::InputEvent pressB(float x = 300.0F, float y = 300.0F)
{
    return viewportEvent(hyremote::InputEventKind::PointerButton, kWidthB, kHeightB, 1.0F, x, y, true);
}

hyremote::InputEvent moveA(float x, float y)
{
    return viewportEvent(hyremote::InputEventKind::PointerMove, kWidthA, kHeightA, 1.0F, x, y);
}

hyremote::InputEvent moveB(float x, float y)
{
    return viewportEvent(hyremote::InputEventKind::PointerMove, kWidthB, kHeightB, 1.0F, x, y);
}

// Case A: press A, move A, press A - a single coordinate space.
void testSameViewport()
{
    AcceptedPointerMotionState state;
    state.takeSummaryForAcceptedPress(pressA());
    state.noteAcceptedMove(moveA(102.0F, 101.0F));
    const PointerMovementSummary summary = state.takeSummaryForAcceptedPress(pressA(101.0F, 100.0F));
    check(!summary.viewportChanged, "Case A: press A, move A, press A keeps viewportChanged = false");
    check(summary.hasMovement, "Case A: the accepted move is still summarised");
}

// Case B: press A, move B, press A - one accepted move in another coordinate space.
void testMoveInOtherViewport()
{
    AcceptedPointerMotionState state;
    state.takeSummaryForAcceptedPress(pressA());
    state.noteAcceptedMove(moveB(400.0F, 400.0F));
    const PointerMovementSummary summary = state.takeSummaryForAcceptedPress(pressA(101.0F, 100.0F));
    check(summary.viewportChanged, "Case B: press A, move B, press A sets viewportChanged = true");
}

// Case C: press A, move B, move A, press A - the transient excursion this fix exists for. Comparing
// only the two press viewports would report "unchanged" while the extrema came from two spaces.
void testTransientViewportExcursion()
{
    AcceptedPointerMotionState state;
    state.takeSummaryForAcceptedPress(pressA());
    state.noteAcceptedMove(moveB(400.0F, 400.0F));
    state.noteAcceptedMove(moveA(101.0F, 100.0F));
    const PointerMovementSummary summary = state.takeSummaryForAcceptedPress(pressA(101.0F, 100.0F));
    check(summary.viewportChanged,
          "Case C: press A, move B, move A, press A stays viewportChanged = true (return does not restore)");

    // And the same closure point with no move at all after the excursion still fails closed.
    AcceptedPointerMotionState other;
    other.takeSummaryForAcceptedPress(pressA());
    other.noteAcceptedMove(moveB(400.0F, 400.0F));
    other.noteAcceptedMove(moveA(100.0F, 100.0F));
    other.noteAcceptedMove(moveA(100.0F, 100.0F));
    check(other.takeSummaryForAcceptedPress(pressA()).viewportChanged,
          "Case C': repeated return moves never reset the period viewport flag");
}

// Case D: press A, move A, press B - the closing press itself moves to another viewport.
void testPressViewportChange()
{
    AcceptedPointerMotionState state;
    state.takeSummaryForAcceptedPress(pressA());
    state.noteAcceptedMove(moveA(101.0F, 100.0F));
    const PointerMovementSummary summary = state.takeSummaryForAcceptedPress(pressB());
    check(summary.viewportChanged, "Case D: press A, move A, press B sets viewportChanged = true");
}

// Case E: the flag must not leak into the following period.
void testNextPeriodIsClean()
{
    AcceptedPointerMotionState state;
    state.takeSummaryForAcceptedPress(pressA());
    state.noteAcceptedMove(moveB(400.0F, 400.0F));
    state.noteAcceptedMove(moveA(101.0F, 100.0F));
    check(state.takeSummaryForAcceptedPress(pressA()).viewportChanged, "Case E: the polluted period fails closed");
    // Opening the next period at B is itself a viewport step (and is reported as one); after that the
    // period is clean again, so the flag is a period fact and not permanent state.
    check(state.takeSummaryForAcceptedPress(pressB()).viewportChanged,
          "Case E: the closing press of the polluted period still steps from A to B");
    state.noteAcceptedMove(moveB(301.0F, 301.0F));
    const PointerMovementSummary next = state.takeSummaryForAcceptedPress(pressB());
    check(!next.viewportChanged,
          "Case E: once a period is opened in B, moves in B keep it clean - the flag is not permanent");
}

// DPR-only transition, with identical width and height.
void testDevicePixelRatioOnlyChange()
{
    AcceptedPointerMotionState state;
    state.takeSummaryForAcceptedPress(pressA());
    state.noteAcceptedMove(viewportEvent(hyremote::InputEventKind::PointerMove, kWidthA, kHeightA, 2.0F, 101.0F,
                                        100.0F));
    const PointerMovementSummary summary = state.takeSummaryForAcceptedPress(pressA(101.0F, 100.0F));
    check(summary.viewportChanged,
          "a DPR-only change with identical width/height sets viewportChanged = true");
}

// Moves accepted before any press do not take part in a click period.
void testMovementsBeforeFirstPress()
{
    AcceptedPointerMotionState state;
    state.noteAcceptedMove(moveB(400.0F, 400.0F));
    const PointerMovementSummary summary = state.takeSummaryForAcceptedPress(pressA());
    check(!summary.havePreviousPress, "a move before the first accepted press has no previous press");
    check(!summary.viewportChanged,
          "a move before the first accepted press does not fabricate a viewport change for the pair");
}

// The excursion itself still has to be summarised correctly.
void testExcursionSummary()
{
    AcceptedPointerMotionState state;
    state.takeSummaryForAcceptedPress(pressA());
    state.noteAcceptedMove(moveA(104.0F, 96.0F));
    state.noteAcceptedMove(moveA(101.0F, 103.0F));
    state.noteAcceptedMove(moveA(100.0F, 100.0F));
    const PointerMovementSummary summary = state.takeSummaryForAcceptedPress(pressA(100.0F, 100.0F));
    check(!summary.viewportChanged, "same-viewport extrema keep viewportChanged = false");
    check(summary.hasMovement, "the excursion is recorded");
    check(summary.minX == 100.0F && summary.maxX == 104.0F && summary.minY == 96.0F && summary.maxY == 103.0F,
          "the excursion keeps the per-axis extremes of every accepted move");
    check(summary.viewportWidth == kWidthA && summary.viewportHeight == kHeightA
              && summary.devicePixelRatio == 1.0F,
          "the summary carries the period viewport used for mapping");
}

// The summary is what the adapters convert; the conversion must forward the viewport verdict.
void testSpanConversion()
{
    AcceptedPointerMotionState state;
    state.takeSummaryForAcceptedPress(pressA());
    state.noteAcceptedMove(moveB(400.0F, 400.0F));
    state.noteAcceptedMove(moveA(101.0F, 100.0F));
    const PointerMovementSummary summary = state.takeSummaryForAcceptedPress(pressA(101.0F, 100.0F));
    const HyRemote::detail::PointerMovementSpan span = movementSpanForTarget(summary, 500.0F, 600.0F);
    check(span.viewportChanged, "movementSpanForTarget forwards the period viewport verdict");

    AcceptedPointerMotionState clean;
    clean.takeSummaryForAcceptedPress(pressA());
    clean.noteAcceptedMove(moveA(101.0F, 100.0F));
    const HyRemote::detail::PointerMovementSpan mapped =
        movementSpanForTarget(clean.takeSummaryForAcceptedPress(pressA(100.0F, 100.0F)), 500.0F, 600.0F);
    check(!mapped.viewportChanged, "a same-viewport period maps with viewportChanged = false");
    check(mapped.hasMovement && mapped.minimum.x() <= mapped.maximum.x()
              && mapped.minimum.y() <= mapped.maximum.y(),
          "the mapped span keeps the monotone per-axis order");
}

void testBoundedStateAndClear()
{
    static_assert(sizeof(AcceptedPointerMotionState) <= 64,
                  "the accepted motion bookkeeping must stay O(1) bounded state");
    AcceptedPointerMotionState state;
    state.takeSummaryForAcceptedPress(pressA());
    state.noteAcceptedMove(moveB(400.0F, 400.0F));
    state.clear();
    const PointerMovementSummary summary = state.takeSummaryForAcceptedPress(pressA());
    check(!summary.viewportChanged && !summary.hasMovement && !summary.havePreviousPress,
          "clear() drops the period, its excursion and its viewport truth");
}

}  // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    testSameViewport();
    testMoveInOtherViewport();
    testTransientViewportExcursion();
    testPressViewportChange();
    testNextPeriodIsClean();
    testDevicePixelRatioOnlyChange();
    testMovementsBeforeFirstPress();
    testExcursionSummary();
    testSpanConversion();
    testBoundedStateAndClear();

    std::printf("%s: accepted pointer motion (%d checks failed)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
