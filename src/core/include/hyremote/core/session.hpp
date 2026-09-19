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
//
// Lifecycle rules that adapters and callers can rely on:
//
//   - **Component mutation.** Capture source and transport may only be installed or replaced while
//     the Session is `Stopped`; the setters report refusal through their return value and leave the
//     active component untouched. The input sink is held by `shared_ptr` and *may* be replaced at
//     runtime (see `setInputSink`).
//   - **Worker startup handshake.** The Core workers may be created while the Session is still
//     `Starting`. They wait for the `Running` publication instead of treating "not Running yet" as
//     "exit", and no capture request is issued before `Running`, so a capture source never sees a
//     request before the transport has started.
//   - **Callback lifetime.** Core wraps every adapter callback in a Core-owned gate. Once `stop()`
//     has been entered, further callbacks are ignored and counted
//     (`SessionStats::callbacksIgnoredAfterStop`), and in-flight callbacks are drained before the
//     Session state is torn down. Combined with the quiescence rule documented on
//     `CaptureSource::stop()` / `Transport::stop()`, this makes `~Session()` safe even for an
//     adapter that calls back late.
//   - **Adapter exceptions.** Exceptions thrown by an adapter on a Core-owned path
//     (`capabilities()`, `frameCapabilities()`, `start()`, `stop()`, `requestFrame()`,
//     `enqueueFrame()`, `InputSink::post()`) never escape Core. Startup exceptions become a
//     deterministic `SessionError` and already-started components are cleaned up; runtime
//     exceptions are counted and escalate the Session to `Faulted`, except `InputSink::post()`,
//     which is reported as a recoverable error because remote input is not on the capture path.
//   - `stop()` must not be called from inside an adapter callback.

#include <cstddef>
#include <cstdint>
#include <functional>
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

    // A concurrent stop() claimed the run while start() was still establishing it. Nothing was
    // published as `Running`; the Session is `Stopped` as the stop requested.
    StartCancelled,
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
//
// Two kinds of field live here, and the difference is part of the contract:
//
//   * **Cumulative per run** - a counter or an observed maximum that describes the run that is
//     currently in progress or that most recently ran. It keeps its value after `stop()` so that
//     post-stop diagnostics and acceptance evidence can still read it, and it is reset by the next
//     `start()`. Counting starts when the run does, so a value of 0 after a stop means the run
//     genuinely did not produce that event, not that the number was lost during teardown.
//   * **Gauge** - a current instantaneous value. Reading 0 after `stop()` is the truthful answer,
//     because nothing is queued or in flight once the run has been torn down.
//
// `lastFrameRejection` follows the cumulative rule as a string: it holds the most recent rejection
// reason of the run, and is cleared by the next `start()`.
struct SessionStats
{
    // Capture scheduling.
    std::uint64_t captureRequestsIssued = 0;      // cumulative
    std::uint64_t captureRequestsRejected = 0;    // cumulative; requestFrame() returned false
    std::size_t inFlight = 0;                     // gauge: requests issued but not yet completed
    std::size_t maxInFlightObserved = 0;          // cumulative maximum

    // Events reported by the backends.
    std::uint64_t captureEvents = 0;              // cumulative
    std::uint64_t captureEventsNonRecoverable = 0;// cumulative
    std::uint64_t transportEvents = 0;            // cumulative
    std::uint64_t transportEventsFatal = 0;       // cumulative

    // Frame acceptance.
    std::uint64_t framesAccepted = 0;            // cumulative; assigned a FrameId and stored in the mailbox
    std::uint64_t framesRejectedInvalid = 0;     // cumulative; missing storage anchor / unusable geometry
    std::uint64_t framesRejectedTiming = 0;      // cumulative; no content PTS and none can be derived
    std::uint64_t framesArrivedAfterStop = 0;    // cumulative; completion delivered outside a running Session
    std::uint64_t lastFrameId = 0;               // cumulative: the run's last assigned id, kept after stop
    std::string lastFrameRejection;              // cumulative: the run's most recent rejection reason

    // Mailbox.
    std::size_t mailboxWaiting = 0;              // gauge: queued right now
    std::size_t mailboxDispatcherOwned = 0;      // gauge: held by the dispatch worker right now
    std::size_t maxMailboxWaitingObserved = 0;   // cumulative maximum
    std::size_t maxMailboxOwnedObserved = 0;     // cumulative maximum
    std::uint64_t framesDroppedByPolicy = 0;     // cumulative; DropOldest removed a waiting frame
    std::uint64_t framesRejectedOverflow = 0;    // cumulative; ProducerThrottle anomaly (backend over-produced)

    // Transport.
    std::uint64_t framesDispatched = 0;             // cumulative; handed to Transport::enqueueFrame()
    std::uint64_t transportEnqueueFailures = 0;     // cumulative; a transport threw from enqueueFrame()

    // Input.
    std::uint64_t inputEventsPosted = 0;         // cumulative; delivered to an InputSink without an exception
    std::uint64_t inputEventsDropped = 0;        // cumulative; no InputSink installed, or not Running
    std::uint64_t inputPostFailures = 0;         // cumulative; InputSink::post() threw

    // Callback gate: callbacks that arrived after stop() was entered and were therefore ignored.
    std::uint64_t callbacksIgnoredAfterStop = 0; // cumulative; also readable after stop

    // Core workers that have begun executing (scheduler + dispatch worker, so 2 in a running
    // Session). Diagnostic for the startup handshake: a worker that never appears here means thread
    // creation failed, and one that appears but issues nothing did not survive the transition to
    // `Running`.
    std::size_t workersStarted = 0;              // cumulative per run

    // Ordered teardowns actually performed for the current run. It must be exactly 1 for a run:
    // concurrent stop() callers wait for the owner and return without touching the run's components
    // or workers again.
    std::uint64_t teardownsPerformed = 0;        // cumulative per run
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

    // What an observer is being told about. Two values, and no payload: an adapter that compares the values it
    // derives cannot miss a change, and Core does not have to promise an atomic per-field event.
    enum class Change {
        State,       // state() changed
        Diagnostic,  // lastError() was written, including a recurrence of the same error
    };

    // Change notification for adapters that must react instead of polling.
    //
    // Core calls the installed callback when a change is caused by something other than the caller's own start()/
    // stop() - a component failing on its own thread, a transport event, a capture target disappearing - on the
    // thread that caused it, with Core's state lock held. Owner-driven transitions are deliberately not notified:
    // the caller is already inside the call that caused them and can re-read the state when it returns. Install
    // `{}` to remove the callback.
    //
    // The new state and diagnostic are passed in rather than left to be queried, because the callback runs with the
    // state lock held and every getter takes that non-recursive lock: an observer that called state() or
    // lastError() from here would deadlock instead of reporting anything. Reading anything else from the Session is
    // ruled out for the same reason, exactly as it already is for adapter callbacks.
    void setChangeCallback(std::function<void(Change, SessionState, std::optional<SessionError>)> callback);

    // Installs or replaces the capture source. Only allowed while the Session is `Stopped`:
    // returns false for every other state and leaves the active component untouched (the rejected
    // object is released). Components are owned by the Session.
    bool setCaptureSource(std::unique_ptr<CaptureSource> source);

    // Same contract as setCaptureSource().
    bool setTransport(std::unique_ptr<Transport> transport);

    // Optional: without a sink, remote input is counted and dropped instead of being delivered.
    //
    // The sink is held through `shared_ptr`, so unlike the capture source and transport it may be
    // replaced at runtime, in any state. An event that is already being routed may still reach the
    // previous sink, so implementations must tolerate being called after replacement. A following
    // call is delivered to the new sink.
    void setInputSink(std::shared_ptr<InputSink> sink);

    // Validates the configuration and components, checks capability compatibility, starts the
    // capture source, creates the Core workers (which wait for the Running publication), starts the
    // transport and finally publishes `Running`. Returns false and leaves the Session out of
    // `Running` on any failure: configuration and capability errors leave it `Stopped` (nothing was
    // started), while a component startup failure leaves it `Faulted` with the already-started
    // components cleaned up.
    //
    // Cancellation: start() claims a run generation before touching any component and re-checks it
    // after every startup boundary. If a concurrent stop() claims the run while start() is still in
    // progress, start() stops whatever it had already started, returns false with
    // `SessionErrorCode::StartCancelled` and never publishes `Running`; the Session is left
    // `Stopped` as the stop requested.
    bool start();

    // Deterministic stop: stop scheduling, stop the capture source, close the mailbox (the
    // dispatch worker drains accepted frames), join the workers, stop the transport. Idempotent and
    // safe from every state, including `Faulted` and `Starting`; it may be called from any thread,
    // including concurrently with an in-progress start() (which is then cancelled) or from several
    // threads at once (exactly one caller performs the ordered teardown, the others wait for it and
    // then return). It must not be called from inside an adapter callback.
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
