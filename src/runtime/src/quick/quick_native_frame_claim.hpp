#pragma once

// #609 P3-S2R-3 shadow-only claim bookkeeping (pure value logic, no Qt).
//
// Two contracts are deliberately separate:
//   1. every accepted capture request reaches exactly one terminal outcome;
//   2. every completed native frame is consumed by at most one request.
//
// Nothing here publishes pixels. The Quick adapter feeds this state machine only
// for diagnostics; the real grab path keeps running unchanged.

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

namespace HyRemote::detail {

struct NativeFrameToken
{
    std::uint64_t serial = 0;   // render serial assigned at afterRendering
    std::uint64_t epoch = 0;    // surface/pipeline identity generation
    int width = 0;
    int height = 0;
    int format = -1;
    double devicePixelRatio = 0.0;
    long long sourceMs = 0;     // afterRendering instant, not readback callback
    bool reusedUnserved = false;
};

struct NativeFrameClaimPolicy
{
    // An unsent frame may be older than the request; the bound is two 60 Hz
    // frame periods, not an arbitrary delay. Anything older is discarded.
    long long maxUnservedAgeMs = 33;

    // Evidence-based wait budget, NOT a fixed 120 ms hysteresis:
    //   16.7 ms (one 60 Hz frame) + 23 ms (F-G4 QRhi completion p95)
    //   + 10 ms (token poll interval) + ~10 ms scheduling slack = 60 ms.
    // Even with a perfect match, a single in-flight readback completes at most
    // once per ~23 ms, i.e. roughly 43 native frames/s. A 60 fps request stream
    // can therefore NOT be fully served by native frames; the remaining
    // requests must fall back to grab. Do not claim "grab approaches zero".
    long long waitBudgetMs = 60;
};

enum class RequestState
{
    Admitted,
    WaitingNative,
    GrabStarted,
    Terminal,
};

enum class ClaimKind
{
    None,
    Native,
    Grab,
    Unavailable,
};

struct RequestEntry
{
    std::uint64_t admissionSerial = 0;
    long long admittedMs = 0;
    RequestState state = RequestState::Admitted;
    ClaimKind kind = ClaimKind::None;
    bool reusedUnserved = false;
};

class RequestLedger
{
public:
    explicit RequestLedger(std::size_t capacity = 4)
        : m_capacity(capacity == 0 ? 1 : capacity)
    {
    }

    bool admit(std::uint64_t id, std::uint64_t admissionSerial, long long nowMs)
    {
        if (m_entries.size() >= m_capacity)
            return false;
        RequestEntry entry;
        entry.admissionSerial = admissionSerial;
        entry.admittedMs = nowMs;
        return m_entries.emplace(id, entry).second;
    }

    bool beginGrab(std::uint64_t id)
    {
        auto it = m_entries.find(id);
        if (it == m_entries.end() || it->second.state == RequestState::Terminal)
            return false;
        it->second.state = RequestState::GrabStarted;
        return true;
    }

    bool beginWait(std::uint64_t id)
    {
        auto it = m_entries.find(id);
        if (it == m_entries.end() || it->second.state == RequestState::Terminal)
            return false;
        it->second.state = RequestState::WaitingNative;
        return true;
    }

    // Exactly-once terminal transition. Returns false for unknown or already
    // terminal requests so a late grab/native completion can never double-publish.
    bool claim(std::uint64_t id, ClaimKind kind, bool reusedUnserved = false)
    {
        auto it = m_entries.find(id);
        if (it == m_entries.end() || it->second.state == RequestState::Terminal)
            return false;
        it->second.state = RequestState::Terminal;
        it->second.kind = kind;
        it->second.reusedUnserved = reusedUnserved;
        return true;
    }

    std::optional<std::uint64_t> oldestWaiting() const
    {
        for (const auto &pair : m_entries) {
            if (pair.second.state == RequestState::WaitingNative)
                return pair.first;
        }
        return std::nullopt;
    }

    bool hasWaiting() const { return waitingCount() > 0; }

    std::size_t waitingCount() const
    {
        std::size_t count = 0;
        for (const auto &pair : m_entries)
            count += pair.second.state == RequestState::WaitingNative ? 1U : 0U;
        return count;
    }

    std::optional<RequestEntry> entry(std::uint64_t id) const
    {
        auto it = m_entries.find(id);
        if (it == m_entries.end())
            return std::nullopt;
        return it->second;
    }

    void cancelAll() { m_entries.clear(); }

    std::size_t size() const { return m_entries.size(); }

private:
    std::map<std::uint64_t, RequestEntry> m_entries;
    std::size_t m_capacity;
};

class NativeFrameCache
{
public:
    void offer(const NativeFrameToken &token) { m_slot = token; }

    std::optional<NativeFrameToken> takeIfEligible(std::uint64_t lastConsumedSerial,
                                                   std::uint64_t currentEpoch,
                                                   long long nowMs,
                                                   const NativeFrameClaimPolicy &policy)
    {
        if (!m_slot)
            return std::nullopt;
        NativeFrameToken token = *m_slot;
        m_slot.reset();
        if (token.serial <= lastConsumedSerial)
            return std::nullopt;
        if (token.epoch != currentEpoch)
            return std::nullopt;
        if (token.sourceMs <= 0 || nowMs - token.sourceMs > policy.maxUnservedAgeMs)
            return std::nullopt;
        token.reusedUnserved = true;
        return token;
    }

    bool hasValue() const { return m_slot.has_value(); }
    void clear() { m_slot.reset(); }

private:
    std::optional<NativeFrameToken> m_slot;
};

enum class AdmissionAction
{
    UseCached,
    WaitForNative,
    GrabNow,
};

struct AdmissionDecision
{
    AdmissionAction action = AdmissionAction::GrabNow;
    NativeFrameToken token;
};

// The request may reuse an already-rendered-but-unserved frame (bounded age),
// otherwise it may briefly wait for the next natural frame only when rendering
// is provably active. A static window never waits and keeps today's grab path.
inline AdmissionDecision decideAdmission(NativeFrameCache &cache,
                                         std::uint64_t lastConsumedSerial,
                                         std::uint64_t currentEpoch,
                                         long long nowMs,
                                         bool renderingActive,
                                         const NativeFrameClaimPolicy &policy)
{
    AdmissionDecision decision;
    if (auto cached = cache.takeIfEligible(lastConsumedSerial, currentEpoch, nowMs, policy)) {
        decision.action = AdmissionAction::UseCached;
        decision.token = *cached;
        return decision;
    }
    decision.action = renderingActive ? AdmissionAction::WaitForNative
                                      : AdmissionAction::GrabNow;
    return decision;
}

struct ClaimStats
{
    std::uint64_t admitted = 0;
    std::uint64_t cachedEligible = 0;
    std::uint64_t waitEligible = 0;
    std::uint64_t grabOnly = 0;
    std::uint64_t nativeClaims = 0;
    std::uint64_t grabClaims = 0;
    std::uint64_t unavailableClaims = 0;
    std::uint64_t tokensReceived = 0;
    std::uint64_t staleTokens = 0;
    std::uint64_t duplicateTerminals = 0;
    std::uint64_t simulatedNativeThenRealGrab = 0;
    std::uint64_t grabInducedTokens = 0;
    std::uint64_t cancelled = 0;
    std::uint64_t unterminated = 0;
};

}  // namespace HyRemote::detail
