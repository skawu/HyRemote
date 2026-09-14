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

enum class StorageKind {
    Cpu,
    External,
};

struct PlaneView {
    const std::byte *data = nullptr;
    std::size_t bytes = 0;
    std::size_t stride = 0;
    std::size_t offset = 0;
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
    std::uint32_t planeCount = 0;
};

enum class DamageKind {
    Unknown,   // no trustworthy region information; consumer falls back to full frame
    FullFrame,
    Regions,
};

struct Damage {
    DamageKind kind = DamageKind::Unknown;
    std::vector<Rect> regions; // root/frame coordinates when kind == Regions
};

enum class PtsSource {
    Completion,   // safe public-API fallback
    Render,       // backend observed render time
    Presentation, // backend observed presentation/scanout time
};

struct FrameTiming {
    TimePoint pts;
    PtsSource ptsSource = PtsSource::Completion;

    // Diagnostics only. requestTime is never promoted to content PTS automatically.
    std::optional<TimePoint> requestTime;
    std::optional<TimePoint> completionTime;
};

struct RemoteFrame {
    FrameId id = 0; // assigned in completion/Core acceptance order
    FrameGeometry geometry;
    std::shared_ptr<const FrameStorage> storage;
    FrameTiming timing;
    Damage damage;

    // Optional capture request identity for diagnostics only; not visual age/order semantics.
    std::optional<CaptureRequestId> requestId;
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

// Adapter-owned capture source. requestFrame() initiates one asynchronous logical request and
// must marshal to any required Qt GUI/render thread itself. It must never call the transport.
class CaptureSource {
public:
    virtual ~CaptureSource() = default;

    virtual CaptureCapabilities capabilities() const = 0;
    virtual bool start(FrameReadyHandler onFrame, CaptureEventHandler onEvent) = 0;
    virtual void stop() noexcept = 0;
    virtual bool requestFrame(const CaptureRequest &request) = 0;
};

enum class BackpressurePolicy {
    DropOldest,       // default near-live/latest-frame-wins policy
    ProducerThrottle, // explicit alternative when every frame matters
};

struct FrameQueueConfig {
    std::size_t capacity = 2; // waiting frames; dispatcher-owned frame is in addition
    BackpressurePolicy policy = BackpressurePolicy::DropOldest;
};

struct CaptureSchedule {
    std::size_t maxInFlight = 2;
    std::optional<double> targetFramesPerSecond;
};

// Exact normalized keyboard/pointer payload schema is intentionally not frozen in ARCH-01.
// The boundary is frozen: protocol-specific events must be translated before entering the
// target adapter, and target-thread marshaling is the InputSink's responsibility.
struct InputEvent;

class InputSink {
public:
    virtual ~InputSink() = default;
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

    // Transport owns its event loop/runtime. start() must not expose that runtime to Core.
    virtual bool start(InputHandler onInput, TransportEventHandler onEvent) = 0;
    virtual void stop() noexcept = 0;

    // Called only by the Core dispatch path, never by the Qt capture callback directly.
    // Implementations should post/enqueue promptly and own client-specific queueing themselves.
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

} // namespace hyremote
