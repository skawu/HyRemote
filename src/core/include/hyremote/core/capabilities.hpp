#pragma once

// Capture/consumer capability description and the compatibility check that must succeed before
// a Session is allowed to reach `Running` (docs/internal/core-architecture.md section 9).
//
// The check is intentionally narrow: it only answers "can this capture source feed this
// consumer without a converter?". Core fails early instead of silently reinterpreting an
// unsupported format.

#include <string>
#include <vector>

#include "hyremote/core/types.hpp"

namespace hyremote {

struct CaptureCapabilities
{
    bool asynchronous = false;
    bool regionDamage = false;
    bool renderPts = false;
    bool presentationPts = false;
    bool cpuReadable = true;

    // Empty means "unspecified": Core then does not constrain the pixel format.
    std::vector<PixelFormat> cpuFormats;

    // Domain tags for external (platform/encoder) storage, e.g. a future #17 DMA-BUF domain.
    // Empty means "no external storage".
    std::vector<std::string> externalDomains;
};

struct FrameConsumerCapabilities
{
    bool acceptsCpu = true;

    // Empty means "unspecified": any CPU format is accepted.
    std::vector<PixelFormat> cpuFormats;

    // Empty plus `acceptsCpu == false` means "external storage only, unspecified domain".
    std::vector<std::string> externalDomains;
};

struct CompatibilityResult
{
    bool compatible = false;
    std::string reason;

    explicit operator bool() const noexcept { return compatible; }
};

// Deterministic, order-independent capability check. Returns the first reason that applies,
// so callers can surface it directly in a SessionError.
CompatibilityResult checkFrameCompatibility(const CaptureCapabilities &source,
                                            const FrameConsumerCapabilities &consumer);

}  // namespace hyremote
