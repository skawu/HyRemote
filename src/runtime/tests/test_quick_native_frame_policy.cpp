#include "quick/quick_native_frame_policy.hpp"

using HyRemote::detail::NativeFrameExperimentFacts;
using HyRemote::detail::NativeFrameExperimentMode;
using HyRemote::detail::nativeFrameExperimentMode;
using HyRemote::detail::nativeFrameProductionAllowed;

int main()
{
    // Explicit checks: CTest must remain effective in Release/NDEBUG builds.
    const NativeFrameExperimentFacts allEnabled{true, true, true, true};
    if (nativeFrameExperimentMode(allEnabled)
        != NativeFrameExperimentMode::ShadowObservationOnly)
        return 1;

    for (int i = 0; i < 4; ++i) {
        NativeFrameExperimentFacts facts = allEnabled;
        if (i == 0) facts.buildOptIn = false;
        if (i == 1) facts.runtimeOptIn = false;
        if (i == 2) facts.exactQtKit = false;
        if (i == 3) facts.quickWindow = false;
        if (nativeFrameExperimentMode(facts) != NativeFrameExperimentMode::Disabled)
            return 2;
        if (nativeFrameProductionAllowed(facts))
            return 3;
    }
    if (nativeFrameProductionAllowed(allEnabled))
        return 4;
    if (nativeFrameExperimentMode({}) != NativeFrameExperimentMode::Disabled)
        return 5;
    return 0;
}
