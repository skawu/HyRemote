// SPDX-License-Identifier: Apache-2.0
#pragma once

// RemoteFrame: immutable metadata plus shared ownership of immutable-for-the-frame storage.
//
// Frozen semantics: docs/adr/0002-remoteframe-lifetime-timestamps.md.
//
// The three rules that matter most for callers:
//   1. the storage anchor carries the lifetime; a naked pixel pointer never does;
//   2. the content PTS is the timestamp of the content, and the capture request time is never
//      implicitly promoted to it;
//   3. damage is tri-state, and empty `Regions` is not `Unknown`.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hyremote/core/storage.hpp"
#include "hyremote/core/types.hpp"

namespace hyremote {

struct FrameGeometry
{
    Size size;
    PixelFormat pixelFormat = PixelFormat::Unknown;
    AlphaMode alphaMode = AlphaMode::Opaque;
    FrameTransform transform;
    std::uint32_t planeCount = 0;
};

enum class DamageKind {
    Unknown,    // backend cannot provide trustworthy region geometry
    FullFrame,  // backend declares that the whole frame changed
    Regions,    // zero or more rectangles in frame/root coordinates
};

struct Damage
{
    DamageKind kind = DamageKind::Unknown;
    std::vector<Rect> regions;

    static Damage unknown() { return Damage{}; }
    static Damage fullFrame() { return Damage{DamageKind::FullFrame, {}}; }
    static Damage fromRegions(std::vector<Rect> rects)
    {
        return Damage{DamageKind::Regions, std::move(rects)};
    }

    // True only for `Regions` with at least one rectangle. An empty `Regions` list means
    // "region tracking is available and nothing changed", which is explicitly not `Unknown`.
    bool hasRegions() const noexcept { return kind == DamageKind::Regions && !regions.empty(); }
};

// Where the content PTS came from. Ordered from least to most precise.
enum class PtsSource {
    Completion,    // capture completion / ready() timestamp: the v0.1 baseline
    Render,        // backend-observed render timestamp
    Presentation,  // backend-observed presentation / scanout timestamp
};

struct FrameTiming
{
    // Content PTS in the local monotonic domain. Set by the capture backend or derived by Core
    // from completionTime when the source is Completion.
    TimePoint pts;
    PtsSource ptsSource = PtsSource::Completion;

    // Diagnostics only. `requestTime` is the time the capture request was issued and is never
    // used as the content PTS (a pipelined request can be served by a later render carrying a
    // newer scene state).
    std::optional<TimePoint> requestTime;
    std::optional<TimePoint> completionTime;
};

struct RemoteFrame
{
    FrameId id = 0;  // assigned by Core at acceptance; 0 means "not yet accepted"
    FrameGeometry geometry;
    std::shared_ptr<const FrameStorage> storage;  // lifetime anchor; never null when accepted
    FrameTiming timing;
    Damage damage;

    // Diagnostics only. Correlates the frame with the request that produced it; never a proxy
    // for visual age (ADR-0002 "Consumer freshness").
    std::optional<CaptureRequestId> requestId;
};

struct FrameValidationResult
{
    bool ok = false;
    std::string reason;

    explicit operator bool() const noexcept { return ok; }
};

// Validates the parts of a frame that Core must be able to rely on, independently of timing:
// a storage anchor and a usable geometry. Timing is validated separately by
// normalizeFrameTiming(), because it can be repaired from completionTime.
FrameValidationResult validateFrame(const RemoteFrame &frame);

// Normalizes the timing of a frame that Core is about to accept:
//   - `PtsSource::Completion` with `completionTime` set: pts becomes completionTime;
//   - `PtsSource::Completion` with only an explicit pts: kept;
//   - `PtsSource::Render` / `Presentation`: the supplied pts is preserved unchanged;
//   - a declared Render/Presentation source without a pts is rejected rather than silently
//     relabelled;
//   - a frame that carries only `requestTime` is always rejected, because request time is not a
//     content timestamp.
// Returns false and fills `reason` when the timing cannot be normalized without inventing a
// timestamp. On success the frame is safe to publish.
bool normalizeFrameTiming(RemoteFrame &frame, std::string *reason);

// Convenience: request-to-completion latency when both diagnostics are present.
std::optional<std::chrono::nanoseconds> requestToCompletionLatency(const RemoteFrame &frame);

}  // namespace hyremote
