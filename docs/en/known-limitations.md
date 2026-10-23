# Known Limitations

> Language / 语言: **English** | [中文](../known-limitations.md)

This page lists product boundaries that matter to HyRemote users. It does not track development history, CI state, or acceptance workflow.

See [`../compatibility.md`](../compatibility.md) for the exact current matrix.

## Current product line

The current product line is **V0.3.0.0 Product Preview**. It references:

- Windows x86_64;
- Linux x86_64;
- Qt 6.8.3;
- the C++ API, QML API, Generic Plugin, and QPA as peer integration technologies, with no primary or preview hierarchy.

Qt 5.15 LTS, embedded Linux, ARM64, and additional QPA private-ABI combinations are outside the V0.3.0.0 support statement.

## Security

V0.3.0.0 is not an Internet-facing remote-access server.

Current limitations:

- the default listener is `0.0.0.0:5921`, reachable on the host's IPv4 interfaces;
- remote input is disabled by default;
- `Insecure` is unauthenticated and unencrypted, for a trusted LAN only, and not Internet-safe;
- `Authenticated` is conditional and requires a transport-security-enabled build plus a valid security descriptor;
- when `Authenticated` is available it uses RFB VNC authentication, but the stream remains unencrypted;
- the default build does not imply authenticated transport is compiled in;
- a requested `Authenticated` profile without the required build capability fails with `SecurityUnavailable` before target, transport, or listener creation;
- `AuthenticatedEncrypted` is not implemented and always fails with `SecurityUnavailable` before listener creation;
- `AuthenticatedEncrypted` never falls back to `Authenticated` or `Insecure`, even when a certificate or private-key descriptor exists;
- there is no production TLS or certificate policy yet;
- there is no authenticated Session Registry or per-session role model yet.

Stopping the runtime is explicit and complete: `stop()` releases the held input state the runtime maintained for connected viewers and returns the listener and target adapter to `Stopped`, so an application does not have to undo synthetic remote held state itself.

> **Later:** encrypted transport, certificate policy, authenticated sessions, admission and termination controls, and production network policy.

See [`../security.md`](../security.md) and [`../security-model.md`](../security-model.md).

## Listener addresses

HyRemote currently accepts numeric IP addresses, not hostnames or DNS names.

Important behavior:

- default `0.0.0.0`, every IPv4 interface; one exact local IPv4 address or one named interface is also accepted;
- an invalid or unassigned bind address fails instead of silently widening to a wildcard;
- an occupied port fails startup;
- `0.0.0.0` is the shipped default and widens IPv4 exposure to the host's interfaces; it is paired with `Insecure`, so the listener is for a trusted LAN only and is not Internet-safe;
- IPv6 wildcard behavior is platform-specific and must not be assumed dual-stack.

On the Windows/Qt 6.8.3 reference path, `::` is an IPv6-only listener and does not accept IPv4 through the same socket.

> **TODO V0.4:** complete explicit address-family qualification across the final Windows and Linux compatibility matrix.

Changing the bind address changes the security boundary. See [`../security.md`](../security.md).

## Capture scope

The portable correctness baseline supports qualified Qt application surfaces, not every graphics configuration.

Current baseline:

- QWidget top-level targets through the Widgets adapter;
- QQuickWindow targets through the Qt Quick adapter;
- automatic composition for supported application-owned top-level surfaces in Generic and QPA paths.

Do not automatically infer support for:

- every `QOpenGLWidget` configuration;
- every `QQuickWidget` composition;
- Quick3D;
- custom FBO or render-node paths;
- arbitrary `QOpenGLWindow` or native OS windows;
- foreign windows not owned by the Qt application.

> **TODO V0.4:** qualify representative real-world complex rendering paths and document them as explicit compatibility rows.

## Input scope

Remote input currently covers normalized, application-scoped pointer, button, wheel, key and modifier, and committed-text behavior where the transport provides enough information.

Limitations:

- HyRemote does not provide desktop-wide virtual HID or `uinput` control as the normal product path;
- full IME composition, dead-key, and international-layout parity is not guaranteed for every platform and viewer combination;
- printable text is usable from a maintained VNC viewer on the qualified path: a real LAN trial entered Latin characters, digits, and Chinese text from a second machine;
- remote input is **application-scoped**. If the host locally activates another application or the desktop, the remote pointer cannot reliably reclaim OS activation for the target application, so remote typing can stop until the target application is activated locally again;
- unsupported input semantics are not reconstructed by guesswork;
- multiple viewers contribute to one logical application input device, not separate independent cursor or focus domains.

Held supported key and button state is cleaned up across disconnect and Runtime stop, so a remote peer cannot leave the target stuck in a pressed state.

## Session model

V0.3.0.0 exposes an operational connection count, not a full authenticated session-management API.

`connectedClientCount()` tells the application how many clients are currently connected. It does not provide:

- authenticated user identity;
- roles;
- per-session admission policy;
- per-session termination or ban controls;
- independent per-viewer focus or cursor policy.

> **Later:** authenticated Session Registry and bounded admission and termination APIs.

## QML API

The QML API is a peer integration technology in the V0.3.0.0 line and uses the same Shared Runtime as the C++ API.

It does not create a second Runtime. The installed-SDK learning and deployment experience, plus broader qualification, are still being completed.

> **Later:** further QML productization and example/deployment qualification.

## Generic Plugin

Generic Plugin is a peer zero-code integration technology in the V0.3.0.0 line.

Its contract is that the application's native Qt platform remains authoritative. A deployment that unexpectedly changes the application platform identity to `hyremote` is not a valid Generic configuration.

Generic does not inherit QPA private-ABI compatibility claims.

## QPA

QPA is a peer integration technology in the V0.3.0.0 line. It is narrower than Generic because it depends on the exact Qt private ABI.

Limitations:

- Qt private ABI is involved;
- the current reference is **Qt 6.8.3 exact**;
- Windows native delegate: `qwindows`;
- Linux/X11 native delegate: `qxcb`;
- another Qt patch or minor version is not automatically compatible;
- Wayland, EGLFS, macOS, and other delegates are not implied by the reference pairs;
- arbitrary foreign or native windows are outside the current QPA claim.

Prefer Generic when it satisfies the application.

> **Current product-line work:** complete formal QPA productization; add exact compatibility rows only for qualified Qt and OS pairs.

## Multi-window behavior

HyRemote targets one Qt application, not a complete operating-system desktop.

Supported application-owned top-level surfaces may be represented in one logical remote application session. Opening or closing a supported dialog or tool window should not require a new listener.

This does not create a blanket claim for arbitrary native or foreign windows.

## Performance

V0.3.0.0 is a correctness-first, CPU-readable baseline.

It does not promise:

- universal zero-copy capture;
- DMA-BUF or GBM production paths;
- hardware H.264 or H.265 encoding;
- RKMPP, VAAPI, or D3D accelerated transport;
- optimal per-surface damage for every Qt application type;
- high-motion video or Quick3D performance equivalent to a media-streaming protocol.

Remote interaction may be noticeably slower than local input while remaining usable: the first real LAN trial reported that the host reacts immediately while the remote view lags visibly. Performance work is driven by measured product bottlenecks, not by platform API availability alone.

> **Later acceleration line:** add low-copy or hardware paths only where measurements justify them.

## Deployment

A valid product deployment runs from its own application tree.

It does not depend on:

- the original HyRemote build directory;
- the original HyRemote SDK path;
- a Qt SDK plugin path used as a runtime workaround;
- manually copied internal Core, transport, or capture files.

Use `hyremote_deploy()` for product payload placement. See [`../guide/deployment.md`](../guide/deployment.md).

## Physical local and remote coexistence

Headless or offscreen execution can demonstrate remote-protocol-to-application behavior, but it does not by itself prove that every physical display and input configuration behaves correctly while remote access is active.

The product goal is to preserve native local rendering and input while adding remote access.

> **TODO V0.4:** complete physical Windows and Linux qualification for the final compatibility matrix, especially QPA and complex multi-window paths.

## Qt versions

Current reference: **Qt 6.8.3 / Qt 6.8 LTS family**.

Qt 5.15 LTS is a planned compatibility line but not a current product claim because the current Runtime requires Qt 6.8+.

> **TODO V0.4:** qualify Qt 5.15 LTS and update the product matrix only after build, deployment, runtime, and the applicable integration paths actually work.

## Embedded platforms

Desktop x86 results do not imply embedded Linux support.

Later product directions:

- ARM64 embedded Linux;
- RK3588;
- EGLFS or Wayland;
- NXP i.MX class;
- DMA-BUF or GBM external-buffer paths;
- RKMPP and other hardware encoders.

> **TODO:** embedded Linux, ARM64, and EGLFS reference cells are mandatory pre-GA V0.3-family baseline work and must converge before V0.4 RC entry. Dependency slicing and additional BSP or SoC expansion beyond the declared GA cells are admitted later as separate outcomes, with no version number preassigned by this convergence.
>
> **Later acceleration line:** hardware-accelerated capture and encode after embedded correctness is established.
