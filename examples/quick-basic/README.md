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
- port `5900` unless overridden;
- construction alone is inert;
- remote input is disabled unless explicitly requested.

The current baseline RFB transport uses **SecurityType None**. It is a correctness/trusted-loopback baseline, not Internet-safe authentication/encryption. Keep the example on loopback or another explicitly trusted test network.

## Build

From the source tree with Qt 6.8.x Quick available:

```sh
cmake -S . -B build \
  -DHYREMOTE_BUILD_CORE=ON \
  -DHYREMOTE_BUILD_REMOTE_ACCESS=ON \
  -DHYREMOTE_BUILD_QUICK_ADAPTER=ON \
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

## Reconnect

Disconnect the viewer and reconnect to the same listener while the application remains running. Reconnect must not require recreating the Quick window or restarting the application.

The acceptance path also verifies `RemoteAccess::stop()` releases the listener.

## Input, resize and DPR semantics

The V1 product path covers:

- left/middle/right pointer buttons;
- vertical wheel delivery;
- keyboard press/release;
- Shift/Ctrl/Alt modifiers;
- text/IME commit separately from physical/logical key events;
- focus into the Quick text target;
- default view-only rejection;
- disconnect/reconnect;
- stop/listener release.

Pointer events carry the remote framebuffer viewport and are mapped to the target's logical geometry. Core deterministic tests pin edge and DPR normalization; Quick capture tests cover asynchronous public-API capture behavior. #90 separately tracks disconnect-time balancing releases for any held input state.

## Graphics boundary

The production Quick adapter uses the public `QQuickWindow::contentItem()->grabToImage()` scene-tree path. Historical and production classification for Quick 2D / Quick3D / custom FBO cases is recorded separately; do not infer those optional content-family support claims from this basic Quick 2D example.

## Local + remote coexistence boundary

HyRemote's Embedded C++ mode leaves the native Quick application as the normal local application. Hosted E2E currently runs with offscreen/software Quick for repeatability, so it proves remote protocol→Quick behavior but **does not prove a physical monitor/local input path**.

Physical local display and local input coexistence remains a final #30/#33 acceptance item.

## Connected-viewer status

`RemoteAccessState::Running` means the service is running, not that a viewer exists. #91 / PR #92 provides the backend-neutral connected-client diagnostic needed for the final local status display. Do not derive a fake connected indicator from lifecycle state.

## Related documentation

The broader Windows/Linux, SDK/source-consumption, deployment, security, viewer and compatibility guides converge under #41. Keep all support statements aligned with their recorded evidence.