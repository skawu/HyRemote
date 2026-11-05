#pragma once

// #609 P3, developer-only branch: this policy never authorizes publication.
// A render-stage observer is not a qualified image capture source. These checks
// remain independent from the product's existing ViewerCaptureDemand fallback.
namespace HyRemote::detail {

enum class NativeFrameExperimentMode {
    Disabled,
    ShadowObservationOnly,
};

struct NativeFrameExperimentFacts
{
    bool buildOptIn = false;
    bool runtimeOptIn = false;
    bool exactQtKit = false;
    bool quickWindow = false;
};

constexpr NativeFrameExperimentMode nativeFrameExperimentMode(
    NativeFrameExperimentFacts facts) noexcept
{
    return facts.buildOptIn && facts.runtimeOptIn && facts.exactQtKit && facts.quickWindow
               ? NativeFrameExperimentMode::ShadowObservationOnly
               : NativeFrameExperimentMode::Disabled;
}

constexpr bool nativeFrameProductionAllowed(NativeFrameExperimentFacts) noexcept
{
    // The engineering branch has no approval to change remote output.
    return false;
}

}  // namespace HyRemote::detail
