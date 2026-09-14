# ADR-0003: Threading, Scheduling and Backpressure

Status: **Proposed for ARCH-01 (#5)**

## Context

HyRemote must never turn a slow remote client into a local UI/render stall. The host evidence also shows that capture APIs have thread affinity and that public Quick async capture can accumulate in-flight work if the caller does not bound it.

## Decision

HyRemote separates **capture scheduling**, **completed-frame buffering**, **transport dispatch** and **transport runtime**.

```text
Qt GUI / target thread
      │ request / completion adapter
      ▼
CaptureSource
      │ RemoteFrame completion
      ▼
Core completed-frame mailbox  (bounded)
      │
      ▼
Core dispatch worker
      │ enqueue / post
      ▼
Transport runtime thread/event loop
      │
      ├─ client A queues/codecs
      ├─ client B queues/codecs
      └─ input callbacks
                │
                ▼
        Input adapter -> Qt GUI thread
```

## Thread ownership

### Application / Qt GUI thread

Owns UI object lifetime and any capture operation required by Qt to run on that thread.

Widgets/Quick adapters are responsible for marshaling one-shot capture requests onto the correct Qt thread.

The GUI thread must not perform network I/O, client fan-out, compression or hardware encoding as part of the Core hand-off.

### Qt scene graph / render thread

Owned by Qt. HyRemote public-baseline code does not assume direct control of it.

A future optimized backend may execute callbacks there only when the relevant Qt contract permits it. Such a backend must hand off a stable storage object quickly and must not wait on network consumers.

### Capture callback thread

A `CaptureSource` may complete on a backend-defined thread. Core's frame-ready entry point is thread-safe and performs only bounded bookkeeping/enqueue work.

No capture backend may call a transport synchronously as part of frame production.

### Core dispatch worker

Consumes the completed-frame mailbox and passes frames toward the transport adapter. The transport hand-off itself is a **bounded/nonblocking enqueue contract**: the dispatch worker must never perform network I/O, compression, client fan-out or an unbounded wait inside `Transport::enqueueFrame()`.

This is required for deterministic shutdown as well as responsiveness. The Core mailbox protects capture from a slow transport runtime; the transport's own bounded runtime/client queues protect the dispatch worker from network peers.

### Transport runtime

Owned by the transport implementation. For NeatVNC this means one isolated AML/NeatVNC runtime; Core and capture modules never call AML directly.

Transport implementations **must** implement the Core hand-off by posting/enqueueing into their own runtime with bounded execution time. If their internal queue is full, they must apply an explicit local drop/reject policy rather than block the Core dispatch worker indefinitely.

### Input delivery

Remote input originates on the transport runtime thread. The transport normalizes protocol-specific events, then the input adapter marshals them to the target's required thread (normally the Qt GUI thread). `InputSink::post()` follows the same rule: it schedules/marshals work and must not make the transport runtime wait on synchronous GUI execution.

## Capture scheduling

Core treats capture as asynchronous one-shot work:

```text
requestFrame(requestDiagnostic)
        ... backend work ...
frameReady(RemoteFrame)
```

The Session scheduler owns:

- target capture cadence;
- max capture requests in flight;
- retry/fallback behavior for rejected requests;
- whether capture is paced per completed/rendered frame.

The scheduler is **independent of transport backlog by default**. A slow transport must not stop local capture/render unless the user explicitly selects producer-throttle behavior.

### Initial defaults

Host evidence suggests a conservative public-Quick starting point of:

- max capture in flight: **2**;
- completed mailbox capacity: **2**;
- completed-frame policy: **DropOldest / LatestFrameWins**.

These are v0.1 defaults/candidates, not universal performance claims. #18 may tune target defaults for RK3588/EGLFS without changing the policy API.

## Completed-frame mailbox

The Core mailbox capacity counts frames **waiting for dispatch**. A frame currently held by the dispatch worker is owned in addition to the queue, so maximum Core-side ownership is `capacity + 1` plus any frames retained downstream by the transport/clients.

### `DropOldest` — default

When the mailbox is full and a new frame arrives:

1. remove the oldest queued frame;
2. release its ownership;
3. enqueue the new frame.

Properties:

- capture remains decoupled from transport rate;
- queue depth stays bounded;
- viewer receives the newest available content under overload;
- dropping a frame is normal flow control, not an error.

### `ProducerThrottle` — optional

Capture admission is reduced/stopped while the bounded mailbox is full.

Properties:

- no intentional drops at the Core mailbox;
- capture becomes coupled to transport rate;
- useful only when every frame matters more than near-live behavior.

This policy must be explicit; it is never silently substituted for LatestFrameWins.

### `BlockProducer`

Not supported as a normal v0.1 policy. Blocking the GUI/render/capture producer on transport consumption violates HyRemote's local-responsiveness goal.

## Duplicate/coalescing rule

Capture backends may produce multiple completed requests carrying equivalent visual content. Core may later add duplicate detection/coalescing, but v0.1 must not infer duplicate identity from request sequence alone.

A transport is always allowed to drop superseded frames according to PTS/queue policy.

## Stop semantics

`Session::stop()` is coordinated but not allowed to deadlock on a remote client:

1. stop scheduling new capture requests;
2. ask capture source to stop/cancel where supported;
3. close the completed-frame mailbox;
4. stop/join dispatch worker (safe because `Transport::enqueueFrame()` is bounded/nonblocking by contract);
5. ask transport to stop its runtime/clients;
6. release adapters/storage.

Timeout/error handling may escalate a component to `Faulted`, but local Qt object destruction must not wait indefinitely for a remote peer.

## Error classes

### Recoverable capture event

Examples:

- target temporarily hidden/not capturable;
- individual capture request rejected;
- temporary transport disconnect;
- client authentication failure.

The Session can remain `Running` and report the event.

### Component failure

Examples:

- capture backend initialization failed;
- transport runtime cannot start;
- adapter loses its target permanently.

Session transitions to `Faulted` or attempts a configured fallback.

### Programmer/configuration error

Examples:

- null/expired target at start;
- incompatible capture/transport format with no converter;
- invalid queue capacity/policy.

`start()` fails before entering `Running`.

## State model

```text
Stopped -> Starting -> Running -> Stopping -> Stopped
              │          │
              └--------> Faulted
```

`Faulted -> Stopping -> Stopped` is always supported so resources can be reclaimed deterministically.

## Transport obligations

A transport adapter:

- must accept/retain `RemoteFrame` ownership safely;
- must not assume producer memory remains valid after its owning frame is released;
- owns client-specific queueing/backpressure beyond the Core mailbox;
- must not expose protocol-native input/frame types into Core;
- **must implement `enqueueFrame()` as a bounded/nonblocking post/enqueue operation**; no network I/O, encode wait, client fan-out or unbounded queue wait is permitted on the Core dispatch call;
- must choose an explicit internal overflow policy when its own runtime/client queue is full.

## Consequences

- a blocked/slow viewer cannot directly block Qt rendering;
- a blocked/slow peer also cannot indefinitely block Core shutdown;
- transport implementations can use their natural event loop/thread;
- capture and transport can evolve independently;
- queue ownership limits are explicit and testable;
- the same policy applies to Widgets and Quick capture sources.
