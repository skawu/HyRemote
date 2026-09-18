// SPDX-License-Identifier: Apache-2.0
#pragma once

// Transport: the consumer half of the Core contract.
//
// The single most important obligation (docs/adr/0003-threading-backpressure.md):
//
//   `enqueueFrame()` is a bounded/nonblocking post/enqueue operation.
//
// No network I/O, no encode wait, no client fan-out and no unbounded queue wait may happen on
// that call, because it runs on the Core dispatch worker. A transport that must buffer for slow
// clients owns its own bounded runtime/client queues and its own explicit overflow policy; Core
// never waits for a remote peer.

#include <functional>
#include <string>

#include "hyremote/core/capabilities.hpp"
#include "hyremote/core/frame.hpp"
#include "hyremote/core/input.hpp"

namespace hyremote {

enum class TransportEventCode {
    ClientConnected,
    ClientDisconnected,
    AuthenticationRejected,
    RecoverableFailure,
    FatalFailure,
};

struct TransportEvent
{
    TransportEventCode code = TransportEventCode::RecoverableFailure;
    std::string message;
};

using InputHandler = std::function<void(const InputEvent &)>;
using TransportEventHandler = std::function<void(const TransportEvent &)>;

class Transport
{
public:
    virtual ~Transport() = default;

    virtual FrameConsumerCapabilities frameCapabilities() const = 0;

    // Starts the transport runtime and installs the callbacks. Returning false fails
    // `Session::start()`.
    //
    // Exceptions from this call are caught by Core, converted into a `SessionError`, and the
    // capture source that already started is stopped again.
    virtual bool start(InputHandler onInput, TransportEventHandler onEvent) = 0;

    // Stops the runtime. Called after the dispatch worker has been joined, so no further
    // `enqueueFrame()` call can arrive afterwards.
    //
    // **Quiescence rule.** `stop()` must not return until the callbacks installed by `start()` can
    // no longer be invoked. Core additionally guards every callback with its own lifetime gate, so a
    // late callback is ignored rather than delivered, but an implementation that violates this rule
    // cannot rely on Core keeping the Session alive for it.
    //
    // `stop()` may be called after a failed or throwing `start()`; implementations must tolerate
    // that.
    virtual void stop() noexcept = 0;

    // Core dispatch calls this, never the capture callback. Must be a bounded/nonblocking
    // post/enqueue into the transport's own runtime; ownership of `frame` (and therefore of its
    // storage anchor) transfers to the transport.
    virtual void enqueueFrame(RemoteFrame frame) = 0;
};

}  // namespace hyremote
