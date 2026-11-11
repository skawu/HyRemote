#pragma once

// Qt5.15+ / Qt6 native Quick readback scheduling policy, independent of
// operating system, graphics API or chipset. All values are EXPERIMENTAL
// candidates: they are not product latency guarantees.
namespace HyRemote::detail {
struct QuickReadbackTiming final {
    static constexpr int normalFrameOpportunityMs = 17;

    // Qt renders only when it has work: on static surfaces an async GPU
    // result cannot be retired until a subsequent Qt render callback.
    // The prior fixed 25ms per attempt added avoidable waiting (often one
    // or two extra display frames) to every static-source capture.
    // First retry sooner, then give the GPU one nominal frame to finish.
    // No spin, glFinish, blocking D3D11 Map, or unbounded extra frames.
    static constexpr int firstRetirementRenderMs = 4;
    static constexpr int secondRetirementRenderMs = 16;
    static constexpr int maxExtraRenders = 2;
};
} // namespace HyRemote::detail
