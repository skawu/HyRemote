// SPDX-License-Identifier: Apache-2.0
#pragma once

// HyRemote ARCH-01 header-level proposal.
//
// This file is DESIGN INPUT ONLY. It is intentionally kept under docs/proposals and is not
// part of the build or a frozen public ABI. #6/#7 may implement/refine it while preserving the
// semantics frozen by the ARCH-01 ADRs.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hyremote {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using FrameId = std::uint64_t;
using CaptureRequestId = std::uint64_t;

struct Size {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct Rect {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

enum class PixelFormat {
    Unknown,
    Bgra8888,
    Rgba8888,
    Bgrx8888,
    Rgbx8888,
    // Optimized/YUV formats may be added without changing Session semantics.
};

enum class AlphaMode {
    Opaque,
    Straight,
    Premultiplied,
};

enum class Rotation {
    Rotate0,
    Rotate90,
    Rotate180,
    Rotate270,
};

struct FrameTransform {
    Rotation rotation = Rotation::Rotate0;
    bool mirrorX = false;
    bool mirrorY = false;
};

enum class StorageKind {
    Cpu,
    External,
};

struct PlaneView {
    const std::byte *data = nullptr;
    std::size_t bytes = 0;
    std::size_t stride = 0;
};

// Base class for platform/encoder-specific storage extensions. Core knows only this lifetime
// anchor plus an extension id. #17 may define (for example) a DMA-BUF descriptor extension in
// its own module without adding DRM/GBM types to hyremote-core.
class StorageExtension {
public:
    virtual ~StorageExtension() = default;
};

// Lifetime anchor for frame pixels/resources.
//
// A published storage object is immutable from the consumer's point of view. If its backing
// resource has thread-affine recycle/destruction requirements, the concrete implementation
// must marshal that work itself when the final shared_ptr is released.
class FrameStorage {
public:
    virtual ~FrameStorage() = default;

    virtual StorageKind kind() const noexcept = 0;
    virtual std::size_t planeCount() const noexcept = 0;

    // Returns an immutable CPU view when mapping is supported. The returned pointer is valid
    // only while this FrameStorage object is retained.
    virtual std::optional<PlaneView> mapRead(std::size_t plane) const = 0;

    // Generic extension boundary for optimized/platform storage. Core never interprets the
    // domain or extension payload. A consumer that understands the named domain may request a
    // lifetime-owned extension object and cast it to the interface defined by that optional
    // module. Exact DMA-BUF/GBM/native-handle descriptors remain deferred to #17.
    virtual std::string_view externalDomain() const noexcept = 0;
    virtual std::shared_ptr<const StorageExtension>
    extension(std::string_view extensionId) const = 0;
};

struct FrameGeometry {
    Size size;
    PixelFormat pixelFormat = PixelFormat::Unknown;
    AlphaMode alphaMode = AlphaMode::Opaque;
    FrameTransform transform;
    std::uint32_t planeCount = 0;
};

enum class DamageKind {
    Unknown,
    FullFrame,
    Regions,
};

struct Damage {
    DamageKind kind = DamageKind::Unknown;
    std::vector<Rect> regions;
};

enum class PtsSource {
    Completion,
    Render,
    Presentation,
};

struct FrameTiming {
    TimePoint pts;
    PtsSource ptsSource = PtsSource::Completion;
    std::optional<TimePoint> requestTime;    // diagnostics only
    std::optional<TimePoint> completionTime;
};

struct RemoteFrame {
    FrameId id = 0;
    FrameGeometry geometry;
    std::shared_ptr<const FrameStorage> storage;
    FrameTiming timing;
    Damage damage;
    std::optional<CaptureRequestId> requestId; // diagnostics only
};

struct CaptureRequest {
    CaptureRequestId id = 0;
    TimePoint requestTime;
};

struct CaptureCapabilities {
    bool asynchronous = false;
    bool regionDamage = false;
    bool renderPts = false;
    bool presentationPts = false;
    bool cpuReadable = true;
    std::vector<PixelFormat> cpuFormats;
    std::vector<std::string> externalDomains;
};

struct FrameConsumerCapabilities {
    bool acceptsCpu = true;
    std::vector<PixelFormat> cpuFormats;
    std::vector<std::string> externalDomains;
};

enum class CaptureEventCode {
    TemporarilyUnavailable,
    RequestRejected,
    TargetLost,
    BackendFailure,
};

struct CaptureEvent {
    CaptureEventCode code = CaptureEventCode::BackendFailure;
    std::string message;
    bool recoverable = true;
};

using FrameReadyHandler = std::function<void(RemoteFrame)>;
using CaptureEventHandler = std::function<void(const CaptureEvent &)>;

class CaptureSource {
public:
    virtual ~CaptureSource() = default;
    virtual CaptureCapabilities capabilities() const = 0;
    virtual bool start(FrameReadyHandler onFrame, CaptureEventHandler onEvent) = 0;
    virtual void stop() noexcept = 0;
    virtual bool requestFrame(const CaptureRequest &request) = 0;
};

enum class BackpressurePolicy {
    DropOldest,
    ProducerThrottle,
};

struct FrameQueueConfig {
    std::size_t capacity = 2; // host-evidence starting default; dispatcher-owned frame extra
    BackpressurePolicy policy = BackpressurePolicy::DropOldest;
};

struct CaptureSchedule {
    std::size_t maxInFlight = 2; // host-evidence starting default; #18 may tune
    std::optional<double> targetFramesPerSecond;
};

// Exact normalized keyboard/pointer payload schema is intentionally not frozen in ARCH-01.
struct InputEvent;

class InputSink {
public:
    virtual ~InputSink() = default;

    // Must enqueue/marshal to the target-required thread; it must not synchronously wait on
    // GUI execution from the transport runtime thread.
    virtual void post(const InputEvent &event) = 0;
};

enum class TransportEventCode {
    ClientConnected,
    ClientDisconnected,
    AuthenticationRejected,
    RecoverableFailure,
    FatalFailure,
};

struct TransportEvent {
    TransportEventCode code = TransportEventCode::RecoverableFailure;
    std::string message;
};

using InputHandler = std::function<void(const InputEvent &)>;
using TransportEventHandler = std::function<void(const TransportEvent &)>;

class Transport {
public:
    virtual ~Transport() = default;
    virtual FrameConsumerCapabilities frameCapabilities() const = 0;
    virtual bool start(InputHandler onInput, TransportEventHandler onEvent) = 0;
    virtual void stop() noexcept = 0;

    // Core dispatch calls this, never the Qt capture callback. The implementation MUST be a
    // bounded/nonblocking post/enqueue operation: no network I/O, encode wait, client fan-out
    // or unbounded queue wait is permitted on this call.
    virtual void enqueueFrame(RemoteFrame frame) = 0;
};

enum class SessionState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Faulted,
};

enum class SessionErrorCode {
    InvalidConfiguration,
    IncompatibleFrameCapabilities,
    CaptureStartFailed,
    TransportStartFailed,
    TargetLost,
    ComponentFailure,
};

struct SessionError {
    SessionErrorCode code = SessionErrorCode::ComponentFailure;
    std::string message;
    bool recoverable = false;
};

struct SessionConfig {
    FrameQueueConfig frameQueue;
    CaptureSchedule capture;
};

class Session {
public:
    explicit Session(SessionConfig config = {});
    ~Session();

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    void setCaptureSource(std::unique_ptr<CaptureSource> source);
    void setTransport(std::unique_ptr<Transport> transport);
    void setInputSink(std::shared_ptr<InputSink> sink);

    bool start();
    void stop() noexcept;

    SessionState state() const noexcept;
    std::optional<SessionError> lastError() const;
};

// Encoder extension point (architectural, not a frozen ABI in #5): an optional encoder module
// composes downstream of RemoteFrame and before/inside a transport. It advertises input
// FrameConsumerCapabilities and may understand a StorageExtension domain such as the future
// #17 DMA-BUF extension. Session does not require an encoder and codec-specific packet types do
// not belong in hyremote-core.

} // namespace hyremote
