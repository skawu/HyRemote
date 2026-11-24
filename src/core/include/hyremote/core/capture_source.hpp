#pragma once

// CaptureSource: the backend-facing half of the Core contract.
//
// Threading (docs/adr/0003-threading-backpressure.md):
//   - `requestFrame()` is called by the Core scheduler thread;
//   - `frameReady()` may fire on a backend-defined thread, so the Core entry point is
//     thread-safe and only performs bounded bookkeeping/enqueue work;
//   - `stop()` is called from the thread that called `Session::stop()`.

#include <cstdint>
#include <functional>
#include <string>

#include "hyremote/core/capabilities.hpp"
#include "hyremote/core/frame.hpp"
#include "hyremote/core/types.hpp"

namespace hyremote {

struct CaptureRequest
{
    CaptureRequestId id = 0;

    // Diagnostic only: it lets Core compute request-to-completion latency and is never used as
    // a content timestamp.
    TimePoint requestTime;
};

enum class CaptureEventCode {
    TemporarilyUnavailable,
    RequestRejected,
    TargetLost,
    BackendFailure,
};

struct CaptureEvent
{
    CaptureEventCode code = CaptureEventCode::BackendFailure;
    std::string message;

    // A recoverable event (a hidden target, a rejected single request) leaves the Session
    // `Running`; a non-recoverable one escalates the Session to `Faulted`.
    bool recoverable = true;
};

using FrameReadyHandler = std::function<void(RemoteFrame)>;
using CaptureEventHandler = std::function<void(const CaptureEvent &)>;

class CaptureSource
{
public:
    virtual ~CaptureSource() = default;

    virtual CaptureCapabilities capabilities() const = 0;

    // Starts the backend and installs the Core callbacks. Returning false fails `Session::start()`.
    //
    // Exceptions from this call are caught by Core, converted into a `SessionError` and followed by
    // a `stop()` attempt (see below), because a throwing start may have left partial state.
    virtual bool start(FrameReadyHandler onFrame, CaptureEventHandler onEvent) = 0;

    // Must not block on any consumer; called during deterministic stop.
    //
    // **Quiescence rule.** `stop()` must not return until the callbacks installed by `start()` can
    // no longer be invoked. Core additionally guards every callback with its own lifetime gate, so a
    // late callback is ignored rather than delivered, but an implementation that violates this rule
    // cannot rely on Core keeping the Session alive for it.
    //
    // `stop()` may be called after a failed or throwing `start()`; implementations must tolerate
    // that (stopping a backend that never fully started is a no-op).
    virtual void stop() noexcept = 0;

    // Issues one asynchronous capture request. Implementations should return promptly: Core
    // bounds concurrency with its in-flight limit and never waits for the pixels here.
    //
    // Returning false means the request was rejected before any work was scheduled; Core counts
    // it, keeps the Session running and retries after a bounded delay.
    virtual bool requestFrame(const CaptureRequest &request) = 0;

    // #790 idle-resource contract. One optional hook for EVERY capture technology, so a
    // zero-viewer edge is not re-invented per backend:
    //
    //   - the Shared Runtime calls this on the capture scheduler thread after the last
    //     Viewer disconnected, before the next `requestFrame()`, and only while the
    //     source is still started;
    //   - implementations release pixel/GPU storage that only exists to serve a capture
    //     (framebuffers, staging textures, cached full-frame images/QImages) and keep
    //     geometry, capabilities and the started state; the next `requestFrame()` must
    //     allocate from scratch;
    //   - this must not block on a GPU fence, a Qt render or a worker join. It may be
    //     called while a previously requested callback is still in flight; in that case
    //     defer the release until that work finishes instead of waiting;
    //   - it is idempotent and may be called at any zero-viewer edge, including one with
    //     nothing cached.
    //
    // Default is a no-op: a source with no retained pixel/GPU allocation is already
    // compliant (#790 distinguishes SDK-owned retained bytes from an allocator high-water
    // mark that the OS may keep after a real free).
    virtual void releaseIdleResources() noexcept {}
};

}  // namespace hyremote
