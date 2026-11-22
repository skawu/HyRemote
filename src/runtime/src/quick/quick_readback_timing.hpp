#pragma once

#include <cstdint>
#include <optional>

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

// A Qt natural render may arrive repeatedly while ONE D3D11 staging Map is
// outstanding. Its retirement attempt has one bounded GUI timer per batch
// epoch; the timer is never a general paint/poll loop. Plain C++ state is
// intentionally reusable across Qt5/Qt6 and graphics APIs. The current GL
// implementation retires its fences on a dedicated worker and does not need
// this Qt render-pumping gate.
class QuickRetirementRenderGate final {
public:
    struct Ticket {
        std::uint64_t epoch = 0;
        std::uint64_t sequence = 0;
        int delayMs = 0;
    };

    void reset() noexcept
    {
        ++m_epoch;
        ++m_sequence;
        m_armed = false;
        m_forcedRenders = 0;
    }

    std::optional<Ticket> arm(bool gpuPending) noexcept
    {
        if (!gpuPending || m_armed
            || m_forcedRenders >= QuickReadbackTiming::maxExtraRenders)
            return std::nullopt;
        m_armed = true;
        return Ticket{m_epoch, ++m_sequence,
                      m_forcedRenders == 0
                          ? QuickReadbackTiming::firstRetirementRenderMs
                          : QuickReadbackTiming::secondRetirementRenderMs};
    }

    bool fire(Ticket ticket, bool gpuPending) noexcept
    {
        if (!m_armed || ticket.epoch != m_epoch
            || ticket.sequence != m_sequence)
            return false;
        m_armed = false;
        if (!gpuPending
            || m_forcedRenders >= QuickReadbackTiming::maxExtraRenders)
            return false;
        ++m_forcedRenders;
        return true;
    }

    // Allocation of QTimer's queued functor may fail before Qt accepts the
    // timer. Clear only our own ticket so the next natural callback can
    // retry; never disarm a new batch or a subsequently accepted timer.
    void abandon(Ticket ticket) noexcept
    {
        if (m_armed && ticket.epoch == m_epoch
            && ticket.sequence == m_sequence)
            m_armed = false;
    }

    int forcedRenders() const noexcept { return m_forcedRenders; }
    bool timerArmed() const noexcept { return m_armed; }

private:
    std::uint64_t m_epoch = 0;
    std::uint64_t m_sequence = 0;
    int m_forcedRenders = 0;
    bool m_armed = false;
};
} // namespace HyRemote::detail

