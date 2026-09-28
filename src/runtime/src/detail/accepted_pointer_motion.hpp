// #400: Runtime-private accepted-stream pointer-motion bookkeeping shared by both Qt target adapters.
//
// Why it exists: the bounded mailbox is freshness-oriented, so adjacent pending PointerMove events
// collapse to the newest coordinate and an older move can also be evicted to make room. The GUI-thread
// classifier therefore cannot see every move. Qt 6.8.3 clears a pending double click when a delivered
// move leaves the per-axis distance box, so dropping that evidence would let
// `click -> move far -> move back -> click` collapse into a false double click.
//
// The fix keeps the coalescing rules untouched and instead records bounded movement facts on the
// accepted-input side, in InputSink::post() where every accepted event passes exactly once:
//   - a successfully admitted move widens the excursion summary of the current period;
//   - a successfully admitted press snapshots the summary of the period that just ended and starts
//     the next period at that press.
// Only successful admissions update this state, so an event that was refused (or threw) is never
// treated as accepted. The stored state is four numbers plus a few flags - it does not grow with the
// number of moves.
//
// Coordinates stay in source-viewport space here (that is the space available at post()); the adapters
// convert the extremes to target logical coordinates with the existing mapPointerToTarget() rule.

#pragma once

#include <QPointF>
#include <QtGlobal>

#include <cstdint>

#include "detail/qt_pointer_sequence_classifier.hpp"
#include "hyremote/core/input.hpp"

namespace HyRemote::detail {

// Movement facts of one period (previous accepted press -> current accepted press), source space.
struct PointerMovementSummary
{
    bool hasMovement = false;
    float minX = 0.0F;
    float maxX = 0.0F;
    float minY = 0.0F;
    float maxY = 0.0F;
    bool havePreviousPress = false;
    bool viewportChanged = false;
    std::uint32_t viewportWidth = 0;
    std::uint32_t viewportHeight = 0;
    float devicePixelRatio = 1.0F;
};

// Transport-thread-owned. The adapters keep one instance inside their existing mailbox state, guarded
// by the mailbox mutex, and clear it wherever the mailbox is cleared.
//
// The period viewport is the viewport of the accepted press that opened the period; every accepted
// move of the period is compared against it. The comparison is sticky for the whole period: once any
// accepted move carried a different width/height/DPR, the period stays marked even if later moves and
// the closing press return to the original viewport. Otherwise `press A -> move B -> move A -> press A`
// would map B/A extrema as if they all belonged to A. The state stays O(1): one viewport plus four
// extrema, never a per-move record.
struct AcceptedPointerMotionState
{
    bool hasPreviousPress = false;
    std::uint32_t periodViewportWidth = 0;
    std::uint32_t periodViewportHeight = 0;
    float periodDevicePixelRatio = 1.0F;
    bool periodViewportChanged = false;
    bool hasMovement = false;
    float minX = 0.0F;
    float maxX = 0.0F;
    float minY = 0.0F;
    float maxY = 0.0F;

    // Call after a PointerMove was successfully admitted to the bounded mailbox.
    void noteAcceptedMove(const hyremote::InputEvent &event) noexcept
    {
        if (hasPreviousPress && !periodViewportChanged
            && (event.sourceViewport.width != periodViewportWidth
                || event.sourceViewport.height != periodViewportHeight
                || event.sourceViewport.devicePixelRatio != periodDevicePixelRatio)) {
            // The move was accepted in a different coordinate space than the press that opened the
            // period. Nothing may restore eligibility before the next accepted press opens a new one.
            periodViewportChanged = true;
        }

        const float x = event.x;
        const float y = event.y;
        if (!hasMovement) {
            hasMovement = true;
            minX = maxX = x;
            minY = maxY = y;
            return;
        }
        minX = x < minX ? x : minX;
        maxX = x > maxX ? x : maxX;
        minY = y < minY ? y : minY;
        maxY = y > maxY ? y : maxY;
    }

    // Call after a PointerButton press was successfully admitted. Returns what the classification of
    // this press has to know, then starts the next period from this press.
    PointerMovementSummary takeSummaryForAcceptedPress(const hyremote::InputEvent &event) noexcept
    {
        PointerMovementSummary summary;
        summary.hasMovement = hasMovement;
        summary.minX = minX;
        summary.maxX = maxX;
        summary.minY = minY;
        summary.maxY = maxY;
        summary.havePreviousPress = hasPreviousPress;
        // Fail closed on both facts: a viewport transition inside the period, or a closing press in a
        // different viewport than the one that opened it.
        summary.viewportChanged =
            periodViewportChanged
            || (hasPreviousPress
                && (event.sourceViewport.width != periodViewportWidth
                    || event.sourceViewport.height != periodViewportHeight
                    || event.sourceViewport.devicePixelRatio != periodDevicePixelRatio));
        summary.viewportWidth = event.sourceViewport.width;
        summary.viewportHeight = event.sourceViewport.height;
        summary.devicePixelRatio = event.sourceViewport.devicePixelRatio;

        hasPreviousPress = true;
        periodViewportWidth = event.sourceViewport.width;
        periodViewportHeight = event.sourceViewport.height;
        periodDevicePixelRatio = event.sourceViewport.devicePixelRatio;
        periodViewportChanged = false;
        hasMovement = false;
        return summary;
    }

    void clear() noexcept
    {
        *this = AcceptedPointerMotionState{};
    }
};

// Converts the source-space summary into the target logical span the classifier compares against the
// pending press position. Mapping is monotone per axis, so mapping the two extreme corners is enough.
inline PointerMovementSpan movementSpanForTarget(const PointerMovementSummary &summary,
                                                 float targetWidth,
                                                 float targetHeight) noexcept
{
    PointerMovementSpan span;
    span.viewportChanged = summary.viewportChanged;
    if (!summary.hasMovement)
        return span;

    const auto mapCorner = [&](float x, float y) -> std::optional<hyremote::MappedInputPoint> {
        hyremote::InputEvent corner;
        corner.kind = hyremote::InputEventKind::PointerMove;
        corner.sourceViewport.width = summary.viewportWidth;
        corner.sourceViewport.height = summary.viewportHeight;
        corner.sourceViewport.devicePixelRatio = summary.devicePixelRatio;
        corner.x = x;
        corner.y = y;
        return hyremote::mapPointerToTarget(corner, targetWidth, targetHeight);
    };

    const std::optional<hyremote::MappedInputPoint> low = mapCorner(summary.minX, summary.minY);
    const std::optional<hyremote::MappedInputPoint> high = mapCorner(summary.maxX, summary.maxY);
    if (!low || !high)
        return span;

    span.hasMovement = true;
    span.minimum = QPointF(low->x, low->y);
    span.maximum = QPointF(high->x, high->y);
    return span;
}

}  // namespace HyRemote::detail
