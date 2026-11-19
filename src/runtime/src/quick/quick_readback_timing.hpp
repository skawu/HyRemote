#pragma once

#include <cstdint>
#include <optional>

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

// Shared normal-frame-first GPU retirement timer contract for GL PBO and D3D11.
// Every method is called under its provider's existing State mutex; no Qt
// objects, clocks, extra threads or allocations are needed here.
//
// Before this gate, each natural Qt render callback could queue another 4ms
// (then 16ms) timer while the previous timer remained armed. Those timers could
// wake the GUI loop and issue speculative window->update() even when one timer
// was enough. Tickets also prevent a previous GPU batch's delayed timeout
// from consuming the next batch's bounded forced-render budget.
class QuickRetirementRenderGate final {
public:
    struct Ticket {
        std::uint64_t epoch = 0;
        int delayMs = 0;
    };

    void reset() noexcept
    {
        ++m_epoch;
        m_armed = false;
        m_forcedRenders = 0;
    }

    std::optional<Ticket> arm(bool gpuPending) noexcept
    {
        if (!gpuPending || m_armed
            || m_forcedRenders >= QuickReadbackTiming::maxExtraRenders)
            return std::nullopt;
        m_armed = true;
        return Ticket{m_epoch, m_forcedRenders == 0
                                  ? QuickReadbackTiming::firstRetirementRenderMs
                                  : QuickReadbackTiming::secondRetirementRenderMs};
    }

    bool fire(Ticket ticket, bool gpuPending) noexcept
    {
        // A timer scheduled before a new GPU batch (or stop/restart) cannot
        // request a render for that new batch, or clear its current timer.
        if (!m_armed || ticket.epoch != m_epoch)
            return false;
        m_armed = false;
        if (!gpuPending
            || m_forcedRenders >= QuickReadbackTiming::maxExtraRenders)
            return false;
        ++m_forcedRenders;
        return true;
    }

    int forcedRenders() const noexcept { return m_forcedRenders; }
    bool timerArmed() const noexcept { return m_armed; }

private:
    std::uint64_t m_epoch = 0;
    int m_forcedRenders = 0;
    bool m_armed = false;
};
} // namespace HyRemote::detail
