#pragma once

// Frame storage: the Core lifetime anchor for frame pixels and resources.
//
// Contract (docs/adr/0002-remoteframe-lifetime-timestamps.md):
//   - storage referenced by a published frame stays readable for the whole frame lifetime;
//   - producer-owned reusable memory may only be published through a lifetime anchor that
//     prevents overwrite while any consumer still references it;
//   - the storage implementation owns recycle/destruction and any thread affinity it needs,
//     because the final `shared_ptr` may be released on any Core/transport thread;
//   - a consumer may not retain a naked pointer/handle beyond the frame/storage lifetime.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "hyremote/core/types.hpp"

namespace hyremote {

enum class StorageKind {
    Cpu,
    External,
};

// Immutable, read-only view of one plane. The pointer is valid only while the owning
// FrameStorage object is retained; it is never a standalone lifetime contract.
struct PlaneView
{
    const std::byte *data = nullptr;
    std::size_t bytes = 0;
    std::size_t stride = 0;
};

// Base class for platform/encoder-specific storage extensions. Core knows only this lifetime
// anchor plus an extension id; #17 may define, for example, a DMA-BUF descriptor extension in
// its own module without adding DRM/GBM types to hyremote-core.
class StorageExtension
{
public:
    virtual ~StorageExtension() = default;
};

class FrameStorage
{
public:
    virtual ~FrameStorage() = default;

    FrameStorage(const FrameStorage &) = delete;
    FrameStorage &operator=(const FrameStorage &) = delete;

    virtual StorageKind kind() const noexcept = 0;
    virtual std::size_t planeCount() const noexcept = 0;

    // Returns an immutable CPU view when mapping is supported. The returned pointer is valid
    // only while this object is retained.
    virtual std::optional<PlaneView> mapRead(std::size_t plane) const = 0;

    // Generic extension boundary for optimized/platform storage. Core never interprets the
    // domain or the extension payload; a consumer that understands the named domain may request
    // a lifetime-owned extension object and cast it to the interface of that optional module.
    virtual std::string_view externalDomain() const noexcept = 0;
    virtual std::shared_ptr<const StorageExtension> extension(std::string_view extensionId) const = 0;

protected:
    FrameStorage() = default;
};

// Plain owned CPU storage: the v0.1 baseline for Widgets raster output and for Quick readback
// snapshots.
//
// The producer writes through mutablePlane() before publishing; after publication the storage is
// treated as immutable by every consumer. When the last owning reference is released, the
// optional release hook runs exactly once.
class CpuFrameStorage final : public FrameStorage
{
public:
    struct PlaneLayout
    {
        std::size_t bytesPerLine = 0;
        std::size_t bytes = 0;
    };

    // Invoked exactly once, when the final owning reference is destroyed. A storage whose
    // resource must be returned to a specific thread marshals that work itself (ADR-0002).
    // Must not throw: it runs from a destructor, possibly inside a noexcept cleanup path.
    using ReleaseHook = std::function<void()>;

    // Creates storage with the given per-plane layout. `planes` must not be empty and every plane
    // must have a non-zero size; an invalid layout returns nullptr instead of throwing.
    static std::shared_ptr<CpuFrameStorage> create(const std::vector<PlaneLayout> &planes,
                                                  ReleaseHook onRelease = {});

    // Convenience for the common single-plane packed RGB(A) case.
    static std::shared_ptr<CpuFrameStorage> createSinglePlane(std::size_t bytesPerLine,
                                                              std::size_t height,
                                                              ReleaseHook onRelease = {});

    // Runs the release hook exactly once, on the thread that destroys the last owning reference.
    ~CpuFrameStorage() override;

    StorageKind kind() const noexcept override { return StorageKind::Cpu; }
    std::size_t planeCount() const noexcept override;
    std::optional<PlaneView> mapRead(std::size_t plane) const override;
    std::string_view externalDomain() const noexcept override { return {}; }
    std::shared_ptr<const StorageExtension> extension(std::string_view extensionId) const override;

    // Producer-side write access, valid before publication.
    std::byte *mutablePlane(std::size_t plane);
    const std::vector<PlaneLayout> &layout() const noexcept { return m_planes; }
    std::size_t totalBytes() const noexcept { return m_totalBytes; }

private:
    CpuFrameStorage(std::vector<PlaneLayout> planes, ReleaseHook onRelease);

    std::vector<PlaneLayout> m_planes;
    std::vector<std::vector<std::byte>> m_buffers;
    ReleaseHook m_onRelease;
    std::size_t m_totalBytes = 0;
};

}  // namespace hyremote
