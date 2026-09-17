#include "hyremote/core/frame.hpp"

namespace hyremote {

FrameValidationResult validateFrame(const RemoteFrame &frame)
{
    if (!frame.storage)
        return {false, "frame has no storage anchor; a borrowed pixel pointer alone is not a valid frame"};

    if (frame.geometry.size.width == 0 || frame.geometry.size.height == 0)
        return {false, "frame geometry has a zero width or height"};

    if (frame.geometry.planeCount == 0)
        return {false, "frame geometry declares no planes"};

    if (frame.storage->planeCount() == 0)
        return {false, "frame storage exposes no planes"};

    return {true, {}};
}

bool normalizeFrameTiming(RemoteFrame &frame, std::string *reason)
{
    const auto reject = [reason](const char *message) {
        if (reason != nullptr)
            *reason = message;
        return false;
    };

    FrameTiming &timing = frame.timing;
    const bool hasPts = timing.pts.time_since_epoch().count() != 0;

    if (timing.ptsSource == PtsSource::Completion) {
        if (timing.completionTime.has_value()) {
            // Safe v0.1 baseline: the completion/ready() time is the content PTS. It is also the
            // moment the buffer entered the caller's ownership.
            timing.pts = *timing.completionTime;
            return true;
        }
        if (hasPts)
            return true;  // backend already supplied an explicit completion-equivalent PTS

        return reject("frame carries no content PTS and no completion time; the capture request "
                      "time is not a content timestamp and is never promoted to one");
    }

    // Render / Presentation: a backend that can observe the real frame time must supply it. Core
    // does not silently relabel the source as Completion.
    if (hasPts)
        return true;

    return reject("frame declares a Render or Presentation PTS source but supplies no PTS value");
}

std::optional<std::chrono::nanoseconds> requestToCompletionLatency(const RemoteFrame &frame)
{
    if (!frame.timing.requestTime.has_value() || !frame.timing.completionTime.has_value())
        return std::nullopt;

    const auto latency = *frame.timing.completionTime - *frame.timing.requestTime;
    if (latency.count() < 0)
        return std::nullopt;

    return std::chrono::duration_cast<std::chrono::nanoseconds>(latency);
}

}  // namespace hyremote
