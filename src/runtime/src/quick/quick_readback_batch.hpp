#pragma once

// Shared Qt-free lifetime rule for Qt5/Qt6 native GPU readback batches.
// One physical GPU readback may service multiple distinct Core request IDs.
// When the earliest ID exceeds its deadline, retire the WHOLE readback
// batch with one provider event (while preserving a terminal frame for every
// accepted ID). The remaining per-ID deadline timers then become harmless.
// Does not combine unrelated queued/submitted/converting batches.
#include <algorithm>
#include <iterator>
#include <vector>

#include "hyremote/core/capture_source.hpp"

namespace HyRemote::detail {

inline bool retireTimedOutReadbackBatch(
    std::vector<hyremote::CaptureRequest> &batch,
    hyremote::CaptureRequestId expiredId,
    std::vector<hyremote::CaptureRequest> &terminal)
{
    const auto found = std::find_if(batch.begin(), batch.end(),
        [expiredId](const hyremote::CaptureRequest &request) {
            return request.id == expiredId;
        });
    if (found == batch.end())
        return false;
    terminal.reserve(terminal.size() + batch.size());
    terminal.insert(terminal.end(),
                    std::make_move_iterator(batch.begin()),
                    std::make_move_iterator(batch.end()));
    batch.clear();
    return true;
}

} // namespace HyRemote::detail
