# ADR-0002: RemoteFrame Ownership, Damage and Timestamp Contract

Status: **Proposed for ARCH-01 (#5)**

## Evidence

#3/PR #15 and #16/PR #19 establish four requirements that the frame model must encode explicitly:

1. publishing a borrowed pointer into producer-reused storage is unsafe;
2. `QImage`/snapshot semantics can be safe but may trigger copy-on-write or allocation cost;
3. damage can be known, full-frame, or unavailable depending on capture family;
4. capture **request time is not content PTS**: pipelined `grabToImage()` requests can be serviced by a later render carrying a newer scene state.

## Decision

`RemoteFrame` is an **immutable metadata value plus shared ownership of immutable-for-the-frame storage**.

Conceptually:

```text
RemoteFrame
 ├─ FrameId                   monotonic completion id
 ├─ FrameGeometry             width/height/format/planes
 ├─ shared_ptr<FrameStorage>  lifetime anchor
 ├─ FrameTiming               content PTS + diagnostics
 ├─ Damage                    Unknown | Full | Regions
 └─ metadata/capabilities     backend-neutral flags only
```

A `RemoteFrame` may be copied between queues/consumers; the underlying storage remains valid until the final owning reference is released.

## Storage contract

`FrameStorage` is the Core lifetime anchor.

Required semantics:

- storage referenced by a published frame must remain readable/usable for the frame lifetime;
- producer-owned reusable memory cannot be published without a lifetime/recycle object that prevents overwrite while referenced;
- the storage implementation owns any recycle callback and any thread-affinity needed to release a buffer;
- Core is allowed to destroy the last `shared_ptr` on any Core/transport thread; a storage implementation whose resource must be returned on another thread must marshal that release itself;
- no consumer may retain a naked pointer/handle beyond the `RemoteFrame`/`FrameStorage` lifetime.

### Initial storage forms

1. **CPU-readable storage** — exposes one or more read-only mapped planes while the storage object is alive.
2. **External/platform storage** — opaque to generic Core; exposes a generic extension boundary and may optionally provide a CPU mapping.

The exact DMA-BUF/GBM/native-handle descriptor is deliberately deferred to #17. #5 freezes only the fact that external storage is a first-class storage kind, not its Linux-specific shape.

## Immutability rule

Once submitted to Core, the pixels represented by a `RemoteFrame` are immutable from the perspective of all consumers.

A producer may implement this by:

- transferring ownership;
- publishing a deep immutable snapshot;
- taking a buffer from a pool and recycling it only after the final reference drops;
- publishing external storage whose reuse is protected by its storage/fence/recycle contract.

## Geometry and pixel description

A frame describes logical image size and storage layout independently:

- width/height;
- pixel format identifier;
- plane count;
- per-plane offset/stride/size where CPU mapping exists;
- orientation/transform if the capture backend cannot normalize it cheaply.

v0.1 CPU baselines should normalize to a small set of packed RGB(A) formats. YUV/external formats remain legal extensions for optimized paths.

## Damage model

Damage is tri-state:

```text
Unknown     backend cannot provide trustworthy region geometry
FullFrame   backend declares the entire frame changed
Regions     zero or more rectangles in frame/root coordinates
```

Rules:

- `Unknown` is not the same as an empty region;
- transports must fall back to full-frame behavior for `Unknown` unless they have independent damage detection;
- Widgets paint regions may be supplied only after mapping/clipping into the shared root coordinate system;
- observed QWidget paint regions are an input signal, not automatically a protocol-ready guarantee;
- public Qt Quick async capture may legitimately report `Unknown`.

## Timestamp contract

### One content PTS

Every frame carries one **content PTS** in HyRemote's monotonic time domain plus a `PtsSource` describing where it came from.

Allowed source quality, from least to most precise:

1. `Completion` — capture completion/`ready()` timestamp;
2. `Render` — backend-observed render timestamp;
3. `Presentation` — backend-observed presentation/scanout timestamp.

A backend may add more precise source detail later, but consumers compare the normalized PTS, not backend-native clocks.

### Request time is diagnostic only

Capture request time is never automatically promoted to content PTS.

It may be retained separately as optional diagnostics to calculate request-to-completion latency.

For the public `QQuickItem::grabToImage()` v0.1 candidate, completion/`ready()` time is the safe fallback PTS because the request may be fulfilled by a later render with a newer scene state.

### Completion time

`completionTime` is also recorded when available. When `PtsSource::Completion`, `pts == completionTime`. A lower-level backend may provide an earlier/more accurate render/presentation PTS while still recording completion time for latency analysis.

## Frame identity

`FrameId` is assigned at Core acceptance/completion order and is independent of capture-request sequence numbers.

Request sequence may be retained in diagnostics but must not be used as a proxy for visual age.

## Consumer freshness

Transport/drop decisions should use:

- content PTS / wall-clock age;
- queue depth;
- transport state;
- optional duplicate/content hints supplied by a backend.

They must not infer visual freshness from request index arithmetic.

## Multi-client consequence

A transport may fan one `RemoteFrame` to multiple clients. Shared storage ownership is therefore intentional: the buffer is recycled only after all downstream users release it.

Transport implementations must still bound per-client queues; Core's capture-to-transport mailbox only protects the application/capture side from a slow transport ingestion path.

## Failure behavior

A capture request that produces no frame (for example `grabToImage()` returning a null result for a hidden window) does not create a placeholder `RemoteFrame`. It produces a capture event/error and allows the capture policy to retry or select a fallback.

## Consequences

- CPU and future DMA-BUF paths share one lifetime model.
- slow clients can drop frame objects safely without producer-specific cleanup knowledge.
- transport/encoder modules can retain a frame asynchronously.
- content PTS remains meaningful across public and future optimized capture backends.
