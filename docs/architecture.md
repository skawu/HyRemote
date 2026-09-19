# HyRemote V1 Architecture

Status: **V1.0.0.0 architecture frozen; release acceptance pending**

HyRemote is a Qt remote-access framework for existing Qt Widgets and Qt Quick applications. Its V1 reference platforms are **Windows x86_64 and Linux x86_64**; embedded Linux (EGLFS/OpenGL ES) is the post-V1 platform expansion and is not a V1 support claim. V1 deliberately keeps the application-facing model small while isolating capture, input, transport and platform-specific implementation details behind one runtime architecture.

This document is the canonical top-level architecture for V1. Detailed contracts are refined by:

- [`core-architecture.md`](core-architecture.md);
- [`ADR-0001 Core Boundaries`](adr/0001-core-boundaries.md);
- [`ADR-0002 RemoteFrame Ownership, Damage and Timestamp Contract`](adr/0002-remoteframe-lifetime-timestamps.md);
- [`ADR-0003 Threading, Scheduling and Backpressure`](adr/0003-threading-backpressure.md);
- [`ADR-0006 Authenticated and Encrypted Transport Design`](adr/0006-authenticated-transport-design.md);
- [`input-model.md`](input-model.md);
- [`deployment.md`](deployment.md);
- [`security-model.md`](security-model.md).

## 1. Frozen V1 product model

V1 has exactly three first-class application integration modes:

1. **Embedded C++ API** — applications link the one shared `HyRemote::RemoteAccess` library;
2. **Declarative QML API** — applications `import HyRemote`; the QML `RemoteAccess` item is a thin wrapper over that same C++ runtime;
3. **Transparent QPA Proxy** — existing applications remain Qt-only and launch through `-platform hyremote`; the package-owned `qhyremote` module decorates the qualified native platform and uses the same shared runtime.

Qt Widgets and Qt Quick are first-class peers. One mode is not implemented by adapting the application into another UI framework.

V1 does **not** expose the internal Core/Session/transport graph as the normal application API.

## 2. Runtime and artifact ownership

```text
Embedded C++ app -----------+
                            |
Declarative QML wrapper ----+----> HyRemoteRemoteAccess (one SHARED runtime)
                            |                 |
Transparent qhyremote ------+                 v
                                      internal target adapters
                                               |
                                      internal Core Session
                                       /               \
                               CaptureSource        InputSink
                                      \               /
                                       bounded RFB 3.8
                                             |
                                           viewer
```

The installed V1 artifact rules are fixed:

- `HyRemote::RemoteAccess` / `HyRemoteRemoteAccess` is the **one normal shared C++ product runtime**;
- `hyremote-core` is a **static internal/source component** and is not installed/exported as an application SDK target;
- the QML module is declarative package payload over the same shared runtime, not a second C++ runtime;
- `qhyremote` is a Qt platform **MODULE payload**, not an application link target;
- the installed SDK does not export `HyRemote::Core` or `HyRemote::QpaPlatform`;
- `BUILD_SHARED_LIBS` does not create alternate V1 product personalities.

## 3. Embedded C++ contract

The normal C++ consumer contract is:

```cmake
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE HyRemote::RemoteAccess)
```

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&window);
remote.start();
```

Construction is inert. The default listener is `127.0.0.1:5921`; remote input is disabled by default.

Configuration is mutable while the runtime is `Stopped`. Policy changes use an explicit:

```text
stop -> configure -> start
```

There is no hidden live authorization/control channel in V1.

## 4. Declarative QML contract

The QML layer is intentionally thin:

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

`enabled: true` is applied only after component completion. QML does not create a second Core, Session, transport or error state machine. Target, state, error and client-count observations reflect the same underlying C++ facade.

## 5. Transparent QPA contract

Transparent QPA is selected at process start:

```text
MyApp -platform hyremote
```

Applications remain Qt-only. Deployment uses:

```cmake
find_package(HyRemote CONFIG REQUIRED)
hyremote_deploy(TARGET MyApp QPA)
```

The V1 reference native delegates are:

- Windows: `qwindows`;
- Linux x86_64: `qxcb`.

`qhyremote` creates/decorates native platform objects through the matching Qt private ABI and preserves native local display/input as authoritative. It must not silently fall back to an offscreen/minimal/qvnc-style replacement platform.

V1 QPA is qualified against **Qt 6.8.3 exactly**. Public C++/QML API compatibility with another 6.8.x patch does not broaden the private-ABI QPA support claim.

Remote-input policy for QPA is startup/relaunch policy (for example `hyremote-input=true`). Returning to view-only may require relaunch. V1 does not add a hidden live QPA authorization service.

## 6. Target adapters and capture

The shared runtime selects internal target components for supported Qt targets. The stable application API does not expose capture backend classes.

V1 correctness paths are:

- qualified QWidget top levels through Qt public widget rendering into owned CPU-readable storage;
- qualified QQuickWindow targets through public asynchronous Quick capture (`contentItem()->grabToImage()`).

Transparent QPA composes qualified application-owned top-level QWidget/QQuickWindow surfaces into the remote application view while preserving the native delegate.

Arbitrary foreign/native `QWindow` capture is not a V1 support claim.

## 7. RemoteFrame and Core boundary

`RemoteFrame` is transport-neutral immutable frame metadata plus owned storage lifetime. The contract includes:

- geometry and pixel/storage description;
- owned CPU/external storage lifetime;
- explicit content timing;
- request/completion diagnostics where available;
- damage represented as `Unknown`, `FullFrame` or regions;
- backend-neutral capabilities.

Borrowed raw memory is not a standalone frame contract.

`hyremote-core` remains ordinary C++17 and must not depend on Qt Widgets/Quick/QML, Qt private/QPA, RFB implementation types, graphics-platform APIs or SoC-specific acceleration libraries. Concrete adapters and integrations stay outside Core.

## 8. Scheduling and backpressure

Capture completion does not call the network transport directly. Frames cross a bounded Core mailbox before dispatch.

The near-live default is `DropOldest / LatestFrameWins`. Producer throttling is an explicit separate policy. A slow viewer must not block the Qt GUI/render path or create unbounded frame ownership.

Transport implementations own their own bounded client/protocol state in addition to Core frame backpressure.

## 9. V1 transport boundary

The V1 Windows/Linux correctness transport is HyRemote's **bounded internal C++ RFB 3.8 implementation** behind the private transport seam.

RFB is not an application-facing API. A future accepted transport can reuse the stable facade/Core boundaries without changing normal C++/QML/QPA integration.

Historical NeatVNC/rustvncserver work is research evidence only. Rust/Cargo is not a normal V1 build or consumer dependency. LibVNCServer is not the default V1 backend.

The RFB baseline negotiates **SecurityType None** unless an authenticated profile and a credential are configured, in which case the viewer is authenticated with **RFB VNC authentication** (security type 2). No transport encryption is provided yet - TLS is a separate, later step - so a listener beyond loopback must not be exposed directly to the public Internet.

## 10. Input model and lifecycle

Transport input is normalized before it reaches the Qt target adapter. The V1 model distinguishes pointer motion/buttons, wheel, key/modifier state and committed text where protocol information is sufficient.

Recognized held state has two cleanup boundaries:

1. **abrupt viewer disconnect** — transport-owned per-viewer state is released without affecting another viewer's still-held contribution;
2. **explicit HyRemote stop/policy transition** — Session/transport callbacks are first quiesced, then the target `InputSink` performs terminal cleanup, dropping undelivered input and balancing supported held state already delivered to Qt.

Simultaneous viewers contribute to one shared logical Qt input device. Per-viewer held state remains private, while the transport reference-counts normalized key/button holders so one viewer cannot release another viewer's hold.

Widgets and Quick use the same bounded protected-release mailbox admission. Input overload may reject new ordinary input, but it must not discard the final release for a hold the adapter already accepted.

V1 does not use desktop-wide `uinput`/virtual-HID injection as the normal path.

## 11. Runtime state and diagnostics

The public facade state model is:

```text
Stopped -> Starting -> Running
                    \-> Faulted
Running  -> Faulted
Running/Faulted -> explicit stop -> Stopped
```

`Running` means the listener/runtime is active, not that a viewer is authenticated or connected.

`connectedClientCount()` is operational diagnostics only.

A non-recoverable runtime failure remains observable as `Faulted` until explicit `stop()` performs cleanup. Configuration remains immutable while active/Faulted and reopens only after `Stopped`.

`clearError()` may acknowledge a recoverable runtime diagnostic. Unrelated viewer/capture/transport activity must not resurrect the same acknowledged occurrence; a genuinely new occurrence becomes visible again. `clearError()` cannot hide an active fatal Faulted diagnostic.

## 12. Threading and callback ownership

HyRemote does not assume capture, transport and Qt GUI work execute on one thread.

Frozen rules include:

- Qt-affine target operations are marshalled to the required Qt thread;
- capture completion performs bounded handoff work;
- Core scheduler/dispatch workers are independent from transport/client work;
- transport owns its runtime/client callbacks;
- remote input is marshalled to the target Qt thread;
- Session callback gates are closed and drained before owned runtime objects are torn down;
- component/runtime stop is idempotent and must not depend on a remote peer responding.

## 13. Packaging and deployment

`hyremote_deploy()` is the single HyRemote-owned deployment entry point:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

Source/add-subdirectory and installed-SDK acquisition expose the same product model. A consumer configure uses one HyRemote acquisition source; source and installed metadata must not be mixed.

Optional QML/QPA payload selection is fail-closed. Deployment must not silently recover missing/corrupt optional payloads from another SDK/build tree.

## 14. Compatibility and release evidence

The V1 GA reference target is Windows x86_64 + Linux x86_64, exact Qt 6.8.3 for the integrated all-modes qualification. Compatibility is evidence-based; Candidate/Experimental rows are not Supported claims.

Hosted/Xvfb automation proves correctness that can be automated. It does not replace the separate physical/native local-display/local-input plus remote coexistence gate required by #109.

No release branch/tag is authorized until the release authorities recorded by #33 are accepted.

## 15. Extension seams: what is V1 and what is genuinely post-V1

The architecture intentionally leaves this work behind private/internal seams. The boundary is platform dependency, not topic: work that can be done on x86 belongs to **V1.0.0.0**, and only work that must run on an embedded platform stays post-V1.

- optimized GL/PBO capture (#9) - x86 work, so a **V1.0.0.0** requirement;
- authenticated/encrypted transports and richer per-client authorization/control (#143) - x86 work, so a **V1.0.0.0**
  requirement;
- the RFB encoding strategy and damage-aware incremental delivery (#144) - x86 work, so a **V1.0.0.0** requirement;
- DMA-BUF/GBM/external-buffer paths (#17) - needs the embedded graphics stack, post-V1;
- hardware encoding such as RKMPP/V4L2 on the embedded target, and VA-API where a platform provides it (#10) -
  hardware-bound, post-V1;
- additional platform delegates/targets - embedded platform-family expansion, post-V1.

Such work must preserve the V1 application model unless a later release deliberately changes the public contract. Platform optimization must not force ordinary applications to understand Core, capture, transport, graphics-backend or SoC-specific implementation details.

## 16. Repository ownership mapping

The canonical repository layout mirrors these responsibilities:

```text
src/core/                 internal Core
src/remoteaccess/         one shared public C++ runtime/facade
integrations/qml/         declarative payload
integrations/qpa/         exact-Qt Transparent QPA payload
tests/                    cross-module/consumer/release evidence
examples/                 product examples
research/                 non-product architecture evidence
assets/branding/          non-build branding assets
cmake/                    package/deployment/build modules
docs/                     product and maintainer documentation
```

The V1 release-readiness repository-layout gate prevents the historical root-level module layout or a second runtime dependency graph from returning.
