# Known Limitations

This page lists product boundaries that matter to HyRemote users. It does not track development history, CI state, or acceptance workflow.

For the exact current matrix, see [`compatibility.md`](compatibility.md).

## Current product line

The current product line is **V0.2.0.0 (First User Trial)**, and it references:

- Windows x86_64;
- Linux x86_64;
- Qt 6.8.3;
- C++ API, QML API, Generic Plugin and QPA as **peer** integration technologies, with no primary or
  preview hierarchy between them.

Qt 5.15 LTS, Embedded Linux, ARM64, and additional QPA private-ABI combinations are not part of the current V0.2.0.0 support statement.

## Security

V0.2.0.0 is not an Internet-facing remote-access server.

Current limitations:

- default listener is `0.0.0.0:5921`, reachable on the host's IPv4 interfaces;
- remote input is disabled by default;
- `Insecure` is unauthenticated and unencrypted: the listener is for a **trusted LAN only** and is **not Internet-safe**;
- `Authenticated` is conditional and requires a transport-security-enabled build plus a valid security descriptor;
- when `Authenticated` is available, it uses RFB VNC authentication but the stream remains unencrypted;
- the default V0.2.0.0 build/profile does not imply authenticated transport is compiled in;
- a requested `Authenticated` profile with no required build capability fails with `SecurityUnavailable` before target/transport/listener creation;
- `AuthenticatedEncrypted` is not implemented and always fails with `SecurityUnavailable` before listener creation;
- `AuthenticatedEncrypted` never falls back to `Authenticated` or `Insecure`, even if a certificate/private-key descriptor exists;
- there is no production TLS/certificate policy yet;
- there is no authenticated Session Registry or per-session role model yet.

Stopping the runtime is explicit and complete: `stop()` releases the held input state the runtime was maintaining for connected viewers and returns the listener and the target adapter to `Stopped`, so an application never has to undo synthetic remote held state itself.

> **Later line:** encrypted transport, certificate policy, authenticated sessions, admission/termination controls, and production network policy.

See [`security.md`](security.md).

## Listener addresses

HyRemote currently accepts numeric IP addresses rather than hostnames/DNS names.

Important behavior:

- default: `0.0.0.0` (every IPv4 interface), or one exact local IPv4, or a named interface;
- an invalid/unassigned bind address fails instead of silently widening to a wildcard;
- an already occupied port fails startup;
- `0.0.0.0` is the shipped default and widens IPv4 exposure to the host's interfaces; it is paired with
  `Insecure` (unauthenticated and unencrypted), which is why the listener is for a **trusted LAN only**
  and is **not Internet-safe**;
- IPv6 wildcard behavior is platform-specific and must not be assumed to be dual-stack.

On the current Windows/Qt 6.8.3 reference path, `::` behaves as an IPv6-only listener rather than accepting IPv4 through the same socket.

> **TODO V0.4:** complete the same explicit address-family qualification across the final Windows/Linux compatibility matrix.

Changing the bind address changes the security boundary; see [`security.md`](security.md).

## Capture scope

The portable correctness baseline supports qualified Qt application surfaces, not every possible graphics configuration.

Current baseline:

- QWidget top-level targets through the Widgets adapter;
- QQuickWindow targets through the Qt Quick adapter;
- automatic composition for supported application-owned top-level surfaces in Generic/QPA paths.

Do not automatically infer support for:

- every `QOpenGLWidget` configuration;
- every `QQuickWidget` composition;
- Quick3D;
- custom FBO/render-node paths;
- arbitrary `QOpenGLWindow`/native OS windows;
- foreign windows not owned by the Qt application.

> **TODO V0.4:** qualify representative real-world complex rendering paths and document them as explicit compatibility rows.

## Input scope

Remote input currently covers normalized application-scoped pointer, button, wheel, key/modifier, and committed-text behavior where the transport provides sufficient information.

Limitations:

- HyRemote does not provide desktop-wide virtual HID / `uinput` control as the normal product path;
- full IME composition/dead-key/international-layout parity is not guaranteed for every platform/viewer combination;
- printable text is genuinely usable from a maintained VNC viewer on the qualified path: a real LAN user trial
  entered Latin characters and digits, and Chinese text, from a second machine;
- remote input is **application-scoped**. If the host locally activates another application or the desktop, the
  remote pointer does not reliably reclaim OS activation for the target application, so remote typing can stop
  working until the target application is activated locally again;
- unsupported input semantics are not reconstructed by guesswork;
- multiple viewers contribute to one logical application input device rather than separate independent cursors/focus domains.

Held supported key/button state is cleaned up across disconnect/Runtime stop so a remote peer should not intentionally leave the target stuck in a pressed state.

## Session model

V0.2.0.0 exposes operational connection count, not a full authenticated session-management API.

`connectedClientCount()` tells the application how many clients are currently connected. It does not provide:

- authenticated user identity;
- roles;
- per-session admission policy;
- per-session termination/ban controls;
- independent per-viewer focus/cursor policy.

> **Later line:** authenticated Session Registry and bounded admission/termination APIs.

## QML API

QML API is a peer integration technology of the V0.2.0.0 line, on the same Shared Runtime as the C++ API.

It uses the same Shared Runtime as the C++ API and does not create a second Runtime. However, the final installed-SDK learning/deployment experience and broader qualification are still being completed.

> **Later line:** further QML productization and example/deployment qualification.

## Generic Plugin

Generic Plugin is a peer zero-code integration technology of the V0.2.0.0 line.

Its contract is that the application's native Qt platform remains authoritative. A deployment that unexpectedly changes the application platform identity to `hyremote` is not a valid Generic configuration.

Generic does not inherit QPA private-ABI compatibility claims.

## QPA

QPA is a peer integration technology of the V0.2.0.0 line, and is intentionally narrower than Generic
because it depends on Qt's exact private ABI.

Limitations:

- Qt private ABI is involved;
- current reference is **Qt 6.8.3 exact**;
- Windows native delegate: `qwindows`;
- Linux/X11 native delegate: `qxcb`;
- another Qt patch/minor is not automatically compatible;
- Wayland/EGLFS/macOS and other delegates are not implied by the reference pairs;
- arbitrary foreign/native windows are outside the current QPA claim.

Prefer Generic when it satisfies the application.

> **TODO V0.3/V0.4:** complete formal QPA productization and add exact compatibility rows only for qualified Qt/OS pairs.

## Multi-window behavior

HyRemote targets one Qt application, not a complete operating-system desktop.

Supported application-owned top-level surfaces may be represented in one logical remote application session. Opening or closing a supported dialog/tool window should not require a new listener.

This does not create a blanket claim for arbitrary native/foreign windows.

## Performance

V0.2.0.0 is a correctness-first, CPU-readable baseline.

It does not promise:

- universal zero-copy capture;
- DMA-BUF/GBM production paths;
- hardware H.264/H.265 encoding;
- RKMPP/VAAPI/D3D accelerated transport;
- optimal per-surface damage for every Qt application type;
- high-motion video/Quick3D performance equivalent to a media-streaming protocol.

Remote interaction may be noticeably slower than local input while remaining usable: the first real LAN trial
reported that the host reacts immediately while the remote view lags visibly. Performance work is driven by
measured product bottlenecks rather than by platform API availability alone.

> **TODO later acceleration line:** add low-copy/hardware paths only where measurements justify them.

## Deployment

A valid product deployment should run from its own application tree.

It should not depend on:

- the original HyRemote build directory;
- the original HyRemote SDK path;
- a Qt SDK plugin path used as a runtime workaround;
- manually copied internal Core/transport/capture files.

Use `hyremote_deploy()` for product payload placement. See [`guide/deployment.md`](guide/deployment.md).

## Physical local + remote coexistence

Headless/offscreen execution can demonstrate remote-protocol-to-application behavior, but it does not by itself prove that every physical display/input configuration behaves correctly while remote access is active.

The product goal is to preserve native local rendering/input while adding remote access.

> **TODO V0.4:** complete physical Windows/Linux qualification for the final compatibility matrix, especially QPA and complex multi-window paths.

## Qt versions

Current reference: **Qt 6.8.3 / Qt 6.8 LTS family**.

Qt 5.15 LTS is a planned compatibility line but is not yet a current product claim because the present Runtime requires Qt 6.8+.

> **TODO V0.4:** qualify Qt 5.15 LTS and update the product matrix only after build, deployment, runtime, and applicable integration paths actually work.

## Embedded platforms

Desktop x86 results do not imply Embedded Linux support.

The following are later product directions:

- ARM64 Embedded Linux;
- RK3588;
- EGLFS / Wayland;
- NXP i.MX class;
- DMA-BUF/GBM external-buffer paths;
- RKMPP and other hardware encoders.

> **TODO V1.1:** embedded deployment and dependency slicing.
>
> **TODO later acceleration line:** hardware-accelerated capture/encode after embedded correctness is established.
