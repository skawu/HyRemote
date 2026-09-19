# HyRemote Quick Basic

`quick-basic` is the minimum **Embedded C++ API + Qt Quick/QQuickWindow** product example for HyRemote V1.

The application uses the same public product facade as the Widgets path:

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&view);
remote.start();
```

There is no second Quick-specific session/transport API and no Core/backend object appears in application business code.

## Safety defaults

Normal launch preserves the product defaults:

- loopback listener (`127.0.0.1`);
- port `5921` unless overridden;
- construction alone is inert;
- remote input is disabled unless explicitly requested.

The current baseline RFB transport uses **SecurityType None**. It is a correctness/trusted-loopback baseline, not Internet-safe authentication/encryption. Keep the example on loopback or another explicitly trusted test network.

## Build

From the source tree with the matching Qt 6.8.3 Quick kit available:

```sh
cmake -S . -B build \
  -DHYREMOTE_BUILD_EXAMPLES=ON
cmake --build build --parallel
```

The V1 reference/acceptance line is exact Qt **6.8.3** on Windows x86_64 and Linux x86_64. Other combinations are not supported merely because they compile.

## Run view-only

Launch normally, optionally selecting a loopback port:

```sh
hyremote-quick-basic --port 5902
```

Connect a standard RFB/VNC viewer to:

```text
127.0.0.1:5902
```

The Quick scene should be visible while remote pointer/keyboard/wheel/text input remains blocked by the default view-only policy.

## Run with explicit remote control

For a trusted local test:

```sh
hyremote-quick-basic --port 5902 --remote-input
```

The scene contains a pointer target and `TextInput`. Product E2E verifies that normalized remote input reaches normal Qt Quick event/focus handling rather than a separate remote-only UI path.

`--remote-input` is explicit acceptance/example configuration, never the default.

## Stopped-runtime policy transition

Embedded C++ / Quick uses the same stopped-runtime contract as Widgets. The application and `QQuickWindow` remain alive while only HyRemote is restarted:

```cpp
remote.stop();
remote.setRemoteInputEnabled(true);
remote.start();
```

Repository product-fit exercises that sequence in one `quick-basic` process. The acceptance-only helper starts view-only; the first viewer disconnect triggers the transition and the timer is only a watchdog fallback:

```sh
hyremote-quick-basic --port 5902 --policy-transition-ms 15000 --test-seconds 24
```

This is an example/acceptance control surface only. It uses the frozen public facade and does not introduce a Quick-specific runtime, hidden live-policy channel or test-only product API.

## Reconnect

Disconnect the viewer and reconnect to the same listener while the application remains running. Reconnect must not require recreating the Quick window or restarting the application.

The V1 product-fit additionally reconnects after the same-process view-only -> control `stop -> configure -> start` transition. Final `RemoteAccess::stop()` must release the listener.

## Input, resize and DPR semantics

The current V1 candidate implements and tests:

- left/middle/right pointer buttons;
- vertical wheel delivery;
- keyboard press/release;
- Shift/Ctrl/Alt modifiers;
- text/IME commit separately from physical/logical key events;
- focus into the Quick text target;
- default view-only rejection;
- same-process stopped-runtime transition from view-only to explicit control;
- disconnect/reconnect before/after that transition;
- stop/listener release;
- balancing releases when a viewer disappears with recognized keys/buttons still held.

Pointer events carry the remote framebuffer viewport and are mapped to the target's logical geometry. Core deterministic tests pin edge and DPR normalization; Quick capture tests cover asynchronous public-API capture behavior. Dual-OS acceptance remains pending until the reference jobs actually execute; #74 no-runner failures are not pass evidence.

## Graphics boundary

The production Quick adapter uses the public `QQuickWindow::contentItem()->grabToImage()` scene-tree path. Historical and production classification for Quick 2D / Quick3D / custom FBO cases is recorded separately; do not infer those optional content-family support claims from this basic Quick 2D example.

## Local + remote coexistence boundary

HyRemote's Embedded C++ mode leaves the native Quick application as the normal local application. Hosted E2E currently runs with offscreen/software Quick for repeatability, so it proves remote protocol→Quick behavior but **does not prove a physical monitor/local input path**.

Physical local display and local input coexistence is tracked by #109 and remains a final #30/#33 acceptance item. The physical run reuses this same application/public facade lifecycle; it must not introduce a test-only product API.

## Connected-viewer status

`RemoteAccessState::Running` means the service is running, not that a viewer exists. The current V1 candidate exposes backend-neutral `RemoteAccess::connectedClientCount()` and this example displays the real client count rather than deriving a fake connected state from lifecycle status.

The connect/disconnect/reconnect count transitions are part of the product-fit gate; they remain acceptance-pending until the reference jobs execute.

## Related documentation

Use the V1 user guides under `docs/getting-started/`, `docs/viewer-connection.md`, `docs/security.md`, `docs/compatibility.md`, and `docs/known-limitations.md`. Keep all support statements aligned with recorded evidence.
