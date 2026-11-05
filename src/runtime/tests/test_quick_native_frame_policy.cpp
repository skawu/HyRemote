#include "quick/quick_native_frame_policy.hpp"

#include <cassert>

using HyRemote::detail::NativeFrameExperimentFacts;
using HyRemote::detail::NativeFrameExperimentMode;
using HyRemote::detail::nativeFrameExperimentMode;
using HyRemote::detail::nativeFrameProductionAllowed;

int main()
{
    const NativeFrameExperimentFacts allEnabled{true, true, true, true};
    assert(nativeFrameExperimentMode(allEnabled) ==
           NativeFrameExperimentMode::ShadowObservationOnly);

    for (int i = 0; i < 4; ++i) {
        NativeFrameExperimentFacts facts = allEnabled;
        if (i == 0) facts.buildOptIn = false;
        if (i == 1) facts.runtimeOptIn = false;
        if (i == 2) facts.exactQtKit = false;
        if (i == 3) facts.quickWindow = false;
        assert(nativeFrameExperimentMode(facts) ==
               NativeFrameExperimentMode::Disabled);
        assert(!nativeFrameProductionAllowed(facts));
    }
    assert(!nativeFrameProductionAllowed(allEnabled));
    assert(nativeFrameExperimentMode({}) ==
           NativeFrameExperimentMode::Disabled);
    return 0;
}
