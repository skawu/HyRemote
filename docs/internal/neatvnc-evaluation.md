# NeatVNC Evaluation for HyRemote

Status: **Conditional GO**

Evaluated upstream: `any1/neatvnc`

Initial recommended baseline: **v1.0.1**

## 1. Decision

HyRemote should use NeatVNC as the **first VNC/RFB transport candidate**, behind a thin `VncTransport` adapter.

NeatVNC must **not** become part of `hyremote-core` public types and HyRemote should not fork or reimplement RFB as its default strategy.

The recommendation is conditional on:

1. a reproducible cross-build in the first supported Embedded Linux toolchain;
2. successful interoperability tests with supported viewers;
3. keeping AML/NeatVNC event-loop ownership isolated behind the transport adapter;
4. validating all cross-thread calls instead of assuming the C API is thread-safe;
5. keeping H.264 and GBM optional rather than v0.1 requirements.

## 2. Upstream maturity

NeatVNC v1.0.0 was published as the first stable release and explicitly declared the API and ABI stable from that point.

v1.0.1 is a subsequent stability/bug-fix release and is the initial version recommended for HyRemote evaluation and pinning.

The project describes itself as a liberally licensed VNC server library focused on:

- speed;
- a clean interface;
- interoperability with the Freedesktop ecosystem.

## 3. License

NeatVNC uses the **ISC license**.

The license is permissive and technically suitable for an optional/required dependency of a broadly reusable library such as HyRemote, subject to final HyRemote project-license review.

HyRemote should preserve the relevant copyright/license notices in dependency documentation and binary/source distribution as required.

## 4. API fit

The v1.0.1 public API already provides most primitives HyRemote needs from a transport layer.

### 4.1 Server and client lifecycle

Available APIs include:

- create/delete server;
- listen on TCP, Unix socket, or pre-bound file descriptor;
- enumerate clients;
- obtain client address/authenticated username;
- close individual clients;
- add/remove displays.

This is sufficient for HyRemote to expose transport-neutral session events without leaking NeatVNC types into core APIs.

### 4.2 Frame model

NeatVNC distinguishes **buffer** from **frame metadata**, which aligns well with HyRemote's planned `RemoteFrame` abstraction.

Supported frame/buffer forms include:

- internally allocated simple buffers;
- externally supplied raw memory;
- GBM buffer objects;
- custom buffer pools;
- reference-counted frame/buffer lifetime.

Frame metadata includes:

- width/height;
- DRM FourCC format;
- stride;
- logical size;
- transform;
- presentation timestamp (PTS);
- damage region.

The frame API therefore does not force HyRemote to convert everything into one CPU image representation.

### 4.3 Damage handling

`nvnc_frame_set_damage()` allows the producer to narrow the changed region; a new frame defaults to fully damaged.

This matches the intended HyRemote design:

```text
Capture Backend -> RemoteFrame.damage -> VncTransport -> NeatVNC frame damage
```

Damage discovery remains the responsibility of the appropriate HyRemote capture backend.

### 4.4 GBM / external-buffer path

The API can wrap a `gbm_bo` directly and documents ownership: the NeatVNC wrapper does not take ownership of the underlying GBM object, so producer-side cleanup/lifetime must be explicitly coordinated.

This is a good fit for a future low-copy backend, but GBM is **not** required for v0.1.

### 4.5 Input

The public API provides callbacks for:

- keysym events;
- keycode events;
- absolute pointer/button/scroll events;
- normalized pointer events;
- clipboard text;
- desktop layout change requests;
- new-client events.

HyRemote can therefore implement:

```text
NeatVNC callbacks -> HyRemote Input Adapter -> Qt event delivery
```

without using Linux `uinput` for the normal embedded-application mode.

## 5. Security capabilities

NeatVNC v1.0.1 provides more than legacy VNC password authentication.

The API supports policy flags for:

- authentication required;
- encryption required;
- username required;
- explicitly allowing broken/legacy crypto for trusted private networks.

It also exposes:

- asynchronous authentication callbacks;
- TLS certificate/private-key configuration;
- RSA credential configuration;
- username/password credential access/verification helpers.

Build option `tls` controls encryption/authentication support and uses GnuTLS when available.

### HyRemote implication

Transport security must remain transport-specific. HyRemote core should expose generic controls/events such as:

- enabled/disabled;
- viewing allowed;
- remote input allowed;
- client connected/disconnected;
- transport authentication configuration hooks.

HyRemote must not claim that a legacy VNC password alone is safe for direct Internet exposure.

## 6. Encodings and H.264

NeatVNC contains standard RFB encoders including Raw, Tight and ZRLE.

v1.0.1 also has an optional H.264 path. The build detects H.264 support when GBM/libdrm and either:

- FFmpeg components; or
- Linux V4L2 support

are available.

The RFB protocol implementation includes the Open H.264 encoding (`50`).

TigerVNC 1.16.2 contains optional H.264 decoder support, including Libav and Windows decoder backends when its build enables H.264.

### HyRemote implication

H.264-over-RFB is a viable **candidate**, but it is not a v0.1 compatibility guarantee because viewer binaries may be built without H.264.

HyRemote should:

- keep standard VNC encodings as the universal baseline;
- publish viewer capability requirements for H.264;
- validate actual client/server negotiation before advertising the feature;
- decide later whether RKMPP should be integrated upstream into NeatVNC, adapted through an existing backend, or kept behind a separate HyRemote transport/encoder path.

## 7. Build and dependency shape

v1.0.1 uses Meson and C11.

Core/runtime dependencies include AML, Pixman and zlib, with optional support for JPEG, TLS/crypto, GBM, DRM and FFmpeg/H.264-related components.

NeatVNC accepts AML 1.x (`>=1.0.0`, `<2.0.0`).

### HyRemote build strategy

HyRemote itself should remain CMake-based.

Do not copy NeatVNC sources into `hyremote-core`.

Preferred order:

1. system/package-provided NeatVNC when suitable;
2. reproducible external dependency build/pinned source mechanism;
3. temporary small patch only when unavoidable and documented;
4. upstream contribution instead of a permanent fork.

The exact CMake integration mechanism should be selected under BUILD-01 after the first cross-build test.

## 8. Event-loop integration

NeatVNC relies heavily on AML's default event loop.

AML explicitly supports interoperability with other event loops and exposes its loop file descriptor. Its example demonstrates polling that fd from an outer event loop and then calling:

```text
aml_poll(loop, 0)
aml_dispatch(loop)
```

This makes Qt integration feasible using a `QSocketNotifier` around the AML loop fd.

### Important constraint

AML's default loop is process-global (`aml_set_default` / `aml_get_default`), and NeatVNC internals frequently use `aml_get_default()`.

HyRemote should therefore own an isolated **VNC transport runtime**:

```text
Qt event loop
    |
QSocketNotifier(AML fd)
    |
VncTransportRuntime
    |- owns AML loop/default registration
    |- owns NeatVNC server/displays/clients
    |- serializes transport operations
    `- emits transport-neutral HyRemote events
```

Alternative dedicated-thread integration may be evaluated, but it must not be adopted by assumption: the global default AML loop and NeatVNC thread-safety expectations must be respected.

### Threading rule for initial implementation

Until upstream documentation or tests prove otherwise:

- treat NeatVNC/AML transport objects as single-runtime-thread objects;
- marshal frames from GUI/render/capture threads into the transport runtime;
- marshal input callbacks back into the Qt application thread;
- never let network encoding/backpressure block the Qt render thread.

## 9. Required HyRemote adapter boundary

Recommended conceptual dependency direction:

```text
hyremote-core
    ^
    |
Transport interface
    ^
    |
hyremote-vnc
    |- NeatVNC
    `- AML
```

`hyremote-core` public headers must not expose:

- `struct nvnc*`;
- `nvnc_frame`;
- `pixman_region16`;
- `gbm_bo`;
- AML types.

The adapter translates HyRemote-neutral frame/damage/input/session concepts to and from NeatVNC.

## 10. Risks

### R1 — event-loop/global AML default

Mitigation: one owned VNC transport runtime; explicit integration tests.

### R2 — cross-thread assumptions

Mitigation: serialize NeatVNC calls through its runtime until proven thread-safe.

### R3 — optional dependency explosion

Mitigation: v0.1 enables only required baseline features; TLS/GBM/H.264 stay explicit build features.

### R4 — H.264 viewer compatibility

Mitigation: standard RFB encodings remain baseline; H.264 requires capability-tested viewer builds.

### R5 — upstream coupling

Mitigation: pin stable release, thin adapter, no NeatVNC types in core, prefer upstream contributions.

## 11. Final recommendation

**CONDITIONAL GO — use NeatVNC v1.0.1 as HyRemote's first VNC/RFB transport candidate.**

It is significantly better than implementing RFB from scratch because it already supplies a stable C API/ABI, frame and buffer lifetime management, damage, GBM support, multiple displays, input callbacks, security hooks, standard encodings and an H.264-capable architecture.

The remaining risks are integration risks rather than reasons to reject the library. They should be handled by the HyRemote adapter boundary and verified in the first real build/MVP work.
