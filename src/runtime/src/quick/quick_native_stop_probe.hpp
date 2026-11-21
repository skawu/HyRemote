#pragma once

// Repository-internal test seam for experimental Qt Quick native providers.
// Never part of the public SDK: stop() is a slow lifecycle transition, not
// the capture hot path. The counters prove a real Core worker attempted Qt
// queued notification BEFORE aboutToQuit() closes the GUI event loop.
#include <atomic>

namespace HyRemote::detail {

struct QuickNativeStopPostEvidence {
    unsigned failures = 0;
    unsigned accepted = 0;
};

// Deterministic allocation/post rejection injection belongs to tests only.
// Production leaves the rejection budget at zero. An atomic compare/exchange
// makes exactly one attempt fail even with two simultaneous stop() callers.
inline bool consumeNativeStopPostTestRejection(std::atomic<unsigned> &remaining) noexcept
{
    unsigned count = remaining.load(std::memory_order_relaxed);
    while (count != 0) {
        if (remaining.compare_exchange_weak(
                count, count - 1, std::memory_order_relaxed))
            return true;
    }
    return false;
}

} // namespace HyRemote::detail
