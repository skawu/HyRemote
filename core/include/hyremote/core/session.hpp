#pragma once

// Session: lifecycle, capture scheduling and the bounded completed-frame pipeline.
//
// Frozen semantics (docs/core-architecture.md, ADR-0001/0002/0003):
//   - one Session owns one capture source, one transport and one optional input sink;
//   - a slow transport never blocks capture by default (DropOldest / LatestFrameWins);
//   - the completed-frame mailbox capacity counts *waiting* frames; the frame currently owned by
//     the dispatch worker is additional, so Core-side ownership is bounded by `capacity + 1`;
//   - `start()` fails before `Running` for configuration and capability errors;
//   - `stop()` is deterministic and never waits on a remote peer, because
//     `Transport::enqueueFrame()` is bounded by contract.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "hyremote/core/capture_source.hpp"
#include "hyremote/core/input.hpp"
#include "hyremote/core/transport.hpp"
#include "hyremote/core/types.hpp"

namespace hyremote {

// DropOldest is the default: capture stays decoupled from the transport rate, the viewer sees
// the newest content, and a drop is normal flow control rather than an error.
// ProducerThrottle is an explicit alternative: capture admission is reduced while the mailbox is
// full, so there are no intentional Core-side drops but the capture rate follows the transport.
// It is never silently substituted for DropOldest.
enum class BackpressurePolicy {
    DropOldest,
    ProducerThrottle,
};

struct FrameQueueConfig
{
    // Counts frames waiting for dispatch. The dispatch-worker-owned frame is additional
    // (ADR-0003), so pass a capacity of at least 1.
    std::size_t capacity = 2;

    BackpressurePolicy policy = BackpressurePolicy::DropOldest;
};

struct CaptureSchedule
{
    // Maximum number of capture requests that may be outstanding at the same time.
    // Host evidence suggests 2 for the public Quick async path; #18 may tune the default.
    std::size_t maxInFlight = 2;

    // Optional pacing. When set, Core issues at most this many requests per second.
    std::optional<double> targetFramesPerSecond;

    // Core addition to the ARCH-01 proposal schedule: a rejected request (`requestFrame()`
    // returned false, e.g. a temporarily hidden target) is retried after this bounded delay
    // instead of spinning. It only affects the retry path, never the healthy path.
    std::chrono::milliseconds rejectedRequestRetryDelay{50};
};

struct SessionConfig
{
    FrameQueueConfig frameQueue;
    CaptureSchedule capture;
};

// Validates option values that Core cannot repair (capacity/inFlight >= 1, positive pacing).
// Returns an empty string when the configuration is usable.
std::string validateSessionConfig(const SessionConfig &config);

enum class SessionState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Faulted,
};

std::string_view sessionStateName(SessionState state);

enum class SessionErrorCode {
    InvalidConfiguration,
    IncompatibleFrameCapabilities,
    CaptureStartFailed,
    TransportStartFailed,
    TargetLost,
    ComponentFailure,
};

std::string_view sessionErrorCodeName(SessionErrorCode code);

struct SessionError
{
    SessionErrorCode code = SessionErrorCode::ComponentFailure;
    std::string message;
    bool recoverable = false;
};

// Counters and gauges owned by Core. Cumulative counters are reset by `start()` so that a
// repeated start/stop cycle is observable and deterministic.
struct SessionStats
{
    // Capture scheduling.
    std::uint64_t captureRequestsIssued = 0;
    std::uint64_t captureRequestsRejected = 0;   // requestFrame() returned false
    std::size_t inFlight = 0;
    std::size_t maxInFlightObserved = 0;

    // Events reported by the backends.
    std::uint64_t captureEvents = 0;
    std::uint64_t captureEventsNonRecoverable = 0;
    std::uint64_t transportEvents = 0;
    std::uint64_t transportEventsFatal = 0;

    // Frame acceptance.
    std::uint64_t framesAccepted = 0;            // assigned a FrameId and stored in the mailbox
    std::uint64_t framesRejectedInvalid = 0;     // missing storage anchor / unusable geometry
    std::uint64_t framesRejectedTiming = 0;      // no content PTS and none can be derived
    std::uint64_t framesArrivedAfterStop = 0;    // completion delivered outside a running Session
    std::uint64_t lastFrameId = 0;
    std::string lastFrameRejection;

    // Mailbox.
    std::size_t mailboxWaiting = 0;
    std::size_t mailboxDispatcherOwned = 0;
    std::size_t maxMailboxWaitingObserved = 0;
    std::size_t maxMailboxOwnedObserved = 0;
    std::uint64_t framesDroppedByPolicy = 0;     // DropOldest removed a waiting frame
    std::uint64_t framesRejectedOverflow = 0;    // ProducerThrottle anomaly (backend over-produced)

    // Transport.
    std::uint64_t framesDispatched = 0;             // handed to Transport::enqueueFrame()
    std::uint64_t transportEnqueueFailures = 0;     // a transport threw from enqueueFrame()

    // Input.
    std::uint64_t inputEventsPosted = 0;
    std::uint64_t inputEventsDropped = 0;        // no InputSink installed
};

class Session
{
public:
    explicit Session(SessionConfig config = {});
    ~Session();

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    Session(Session &&) = delete;
    Session &operator=(Session &&) = delete;

    // Components must be installed before start(). They are owned by the Session.
    void setCaptureSource(std::unique_ptr<CaptureSource> source);
    void setTransport(std::unique_ptr<Transport> transport);

    // Optional: without a sink, remote input is counted and dropped instead of being delivered.
    void setInputSink(std::shared_ptr<InputSink> sink);

    // Validates the configuration and components, checks capability compatibility, starts the
    // capture source and the transport, then starts the scheduler and dispatch workers.
    // Returns false and leaves the Session out of `Running` on any failure.
    bool start();

    // Deterministic stop: stop scheduling, stop the capture source, close the mailbox (the
    // dispatch worker drains accepted frames), join the workers, stop the transport. Idempotent
    // and safe from every state, including `Faulted`.
    void stop() noexcept;

    SessionState state() const;
    std::optional<SessionError> lastError() const;

    // Snapshot of the Core counters/gauges for adapters, diagnostics and tests.
    SessionStats stats() const;

    // Configuration in effect (useful for adapters that build the Session from outside).
    SessionConfig config() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace hyremote
