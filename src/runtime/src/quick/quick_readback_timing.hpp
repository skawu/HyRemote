#pragma once

// Qt5.15+ / Qt6 native Quick readback scheduling policy, independent of
// operating system, graphics API or chipset. All values are EXPERIMENTAL
// candidates: they are not product latency guarantees.
namespace HyRemote::detail {
struct QuickReadbackTiming final {
    static constexpr int normalFrameOpportunityMs = 17;

    // #967: the GL/PBO route must not wait a whole nominal display frame
    // before arming its bounded fallback update on a static surface.
    // Fixed-SHA 4eb45ec1 traces show capture_request -> gpu_readback_submitted
    // P50 41.6/42.6 ms while gpu_readback_submitted -> source_completed is
    // only 1.1/1.0 ms: the 17 ms opportunity timer plus the next Qt render
    // callback dominate. Arming the update immediately still produces at most
    // one forced render per render callback (guarded in the GL source), and
    // the D3D11 staging route keeps its own 17 ms policy.
    static constexpr int glFallbackRenderDelayMs = 0;

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
