# HyRemote Product Architecture

> Language / 语言: **English** | [中文](../architecture.md)

HyRemote uses one Core, one Shared Runtime, and four peer integration frontends. Applications can choose how to enter the product without creating separate Session, capture, transport, input, security, or performance implementations.

## 1. Product topology

```text
src/
├── core/
├── runtime/
└── integrations/
    ├── cpp/
    ├── qml/
    ├── generic/
    └── qpa/
```

Dependency direction:

```text
integrations/cpp --------\
integrations/qml ---------\
integrations/generic ------> runtime -> core
integrations/qpa ---------/
```

The four integration frontends are peers:

- **C++ API**: explicit application API through `HyRemote::RemoteAccess`;
- **QML API**: declarative frontend through `import HyRemote`;
- **Generic Plugin**: public-Qt zero-code frontend that preserves the native Qt platform;
- **QPA**: private-ABI zero-code frontend that enters through a Factory Trampoline and delegates to the native Qt platform integration.

Widgets and Qt Quick are Runtime target-adapter dimensions, not integration modes.

## 2. Core

Core owns product semantics that do not depend on Qt UI technology, Qt private APIs, a concrete transport backend, or a specific platform graphics stack.

Core responsibilities:

- Session lifecycle and state semantics;
- frame lifetime and transport-neutral frame metadata;
- capture scheduling and bounded frame handoff;
- queue, drop, and backpressure policy;
- normalized product errors and state;
- normalized input-routing abstractions;
- transport-neutral capability description.

Core does not depend on:

- Qt Widgets, Qt Quick, or QML;
- Qt private or QPA APIs;
- concrete RFB implementation types;
- OpenGL, RHI, EGLFS, DRM, GBM, or DMA-BUF APIs;
- hardware encoder SDKs;
- integration-frontend policy.

This boundary keeps the product model stable while target, transport, platform, and acceleration implementations evolve.

## 3. Shared Runtime

The Shared Runtime is the one Qt-aware product implementation used by every frontend.

Runtime responsibilities:

- Widgets and Qt Quick target adapters;
- concrete RFB transport integration;
- security-profile implementation and validation;
- automatic application-surface discovery and composition;
- application target and input routing;
- runtime component factories and services;
- cross-mode diagnostics and operational behavior;
- automatic performance policy such as viewer-aware capture demand, input-triggered freshness, and adaptive pacing.

The normal shared artifact remains:

```text
HyRemote::RemoteAccess
HyRemoteRemoteAccess
```

There is no separate Widgets Runtime, Quick Runtime, Generic Runtime, QPA Runtime, Embedded Runtime, or Accelerated Runtime product variant.

## 4. C++ API frontend

The C++ API is the explicit programmable entry point:

```cmake
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE HyRemote::RemoteAccess)
```

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&window);
remote.start();
```

The C++ frontend is a wrapper entry point into the Shared Runtime. It does not own the Runtime implementation used by the other frontends.

Construction is inert. Configuration is performed while stopped, followed by an explicit `start()`.

Current product state: a peer route.

## 5. QML API frontend

The QML frontend is a thin declarative layer over the same Shared Runtime:

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

It does not create a second Session, transport, capture, state, error, or performance model.

Current product state: a peer route, on the same shared Runtime.

## 6. Generic Plugin frontend

The Generic Plugin uses Qt public plugin APIs (`QGenericPlugin`). It is a zero-code integration route that keeps the application's normal native Qt platform authoritative.

```text
Qt application
    |
    +--> native Qt platform (qwindows/qxcb/...)
    |
    +--> HyRemote Generic Plugin -> Shared Runtime
```

The application remains Qt-only. Deployment uses:

```cmake
hyremote_deploy(TARGET MyApp GENERIC)
```

Activation uses Qt's generic-plugin mechanism:

```text
MyApp -plugin hyremote
```

Generic does not inherit QPA private-ABI requirements. Its core contract is that the application's native platform identity remains normal.

Current product state: a peer route.

## 7. QPA frontend

QPA is a specialized zero-code integration path for cases that require process-level platform interception:

```text
MyApp -platform hyremote
```

The QPA frontend uses a Factory Trampoline:

```text
qhyremote plugin
    -> QPlatformIntegrationFactory::create(native delegate)
    -> actual native QPlatformIntegration
    -> Shared Runtime attached alongside native behavior
```

It does not reimplement a full platform integration and never silently substitutes an offscreen, minimal, or qvnc-style backend.

Current reference delegates:

- Windows: `qwindows`;
- Linux/X11: `qxcb`.

QPA alone owns the Qt private-ABI dependency. Qualification is exact-Qt-patch specific; the current reference is Qt 6.8.3.

Current product state: a peer route, on the same shared Runtime.

## 8. Widgets and Qt Quick target adapters

Widgets and Qt Quick are handled behind the Shared Runtime.

### Widgets

Supported QWidget targets use the Widgets target adapter and a CPU-readable correctness capture path.

### Qt Quick

Supported `QQuickWindow` targets use the Quick target adapter and the public asynchronous Quick capture path.

Applications use the same product entry points regardless of target family. They do not select capture backend classes directly.

Configuration-specific cases such as QOpenGLWidget, QQuickWidget, Quick3D, custom FBOs, or unusual native-window ownership require explicit compatibility qualification. Basic Widgets or Quick behavior does not imply support for those cases.

Performance policy follows the same rule: adapters expose capture and damage capability, while the Shared Runtime and transport decide how to use it. Applications do not choose internal capture or performance backends.

## 9. Automatic application surface model

Generic and QPA both need automatic access to application surfaces. That logic lives once in Runtime, not separately in either frontend.

Runtime automatic access is responsible for:

- discovering supported application-owned top-level surfaces;
- composing them into the remote application view where required;
- routing normalized remote input back to the appropriate target;
- keeping local native platform behavior authoritative.

A frontend must not implement its own competing composition or controller stack.

## 10. Frame, freshness, and backpressure model

`RemoteFrame` is transport-neutral and owns its storage lifetime.

The frame contract carries:

- dimensions and pixel or storage format;
- owned storage lifetime;
- content timing;
- damage information;
- backend-neutral capabilities.

Frames cross a bounded Core handoff before transport dispatch. Slow clients must not create unbounded frame retention or block the Qt GUI or render path.

The near-live policy keeps the newest relevant frame instead of building an ever-growing queue. Performance work follows a **freshness-first** rule: stale intermediate work may be dropped or coalesced when a newer useful state exists.

Core owns boundedness and transport-neutral freshness semantics. Shared Runtime owns automatic capture demand and pacing. The RFB transport owns compression, incremental-update state, Continuous Updates and Fence, and per-viewer network delivery behavior. None of those protocol or platform mechanisms are exposed as normal application tuning knobs.

See [`../performance-optimization.md`](../performance-optimization.md) for the interaction-latency SLO, adaptive capture and delivery model, and regression programme.

## 11. Input model

Remote input is normalized before it reaches a Qt target adapter.

The product distinguishes:

- pointer movement;
- pointer buttons;
- wheel or scroll;
- keys and modifiers;
- committed text when the protocol provides sufficient information.

Input is application-scoped, not desktop-wide. HyRemote does not use OS-wide virtual HID or uinput injection as the normal path.

Held key and button state is cleaned up on disconnect and on explicit Runtime stop, so a remote peer cannot leave the local application in a stuck input state.

Remote input is also a performance signal. After the application processes a remote action and has an opportunity to update or render, Runtime may prioritize a fresh capture and update so interactive response does not depend only on a periodic capture timer.

## 12. Runtime state

The public Runtime state model:

```text
Stopped -> Starting -> Running
                    \-> Faulted
Running  -> Faulted
Running/Faulted -> stop -> Stopped
```

`Running` means the Runtime and listener are active. It does not mean a viewer is authenticated or connected.

Operational information such as `connectedClientCount()` is diagnostic state, not an authorization identity.

Capture activity is not connection liveness. A static connected session may send no framebuffer payload until content changes while the TCP and RFB session remain alive.

## 13. Transport boundary

The current correctness transport is bounded RFB 3.8 behind a private Runtime and Core boundary.

RFB is not part of the normal application API. A later transport may reuse the same integration frontends, Runtime target adapters, Core frame model, input model, and freshness contract.

RFB-specific performance mechanisms stay private to the transport:

- encoding negotiation and Raw fallback;
- practical compression;
- incremental and non-incremental update state;
- Continuous Updates and Fence where interoperable;
- bounded per-viewer delivery flow;
- cursor pseudo-encoding where useful.

Current security behavior:

- listener on `0.0.0.0:5921` by default;
- remote input disabled by default;
- `Insecure` is unauthenticated and unencrypted: trusted LAN only, not Internet-safe;
- the authenticated profile may use RFB VNC authentication;
- stream encryption is not implemented in the current baseline;
- `AuthenticatedEncrypted` fails closed before a listener is opened while the encrypted backend is unavailable.

## 14. Packaging and deployment

`hyremote_deploy()` is the one product deployment entry point:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
```

The helper owns HyRemote payload placement and Runtime closure. Applications must not manually copy internal libraries or plugins, and must not point runtime search paths back at an SDK or build tree.

Generic deployment carries both the HyRemote generic payload and the application's normal native Qt platform plugin. QPA deployment carries `qhyremote` plus its exact native delegate chain.

## 15. Compatibility boundary

The current desktop reference environment:

- Windows x86_64;
- Linux x86_64;
- Qt 6.8.3 reference SDK.

Public-Qt integration paths and private-QPA compatibility are different claims:

- C++, QML, and Generic primarily rely on Qt public APIs;
- QPA is qualified against an exact private ABI.

A working configuration on one OS, Qt patch, graphics backend, application type, or performance profile does not automatically qualify another.

See [`../compatibility.md`](../compatibility.md).

## 16. Performance extension points

The architecture keeps performance-specific implementation choices behind internal extension points:

- demand-driven or adaptive capture policy in Shared Runtime;
- more efficient graphics capture adapters;
- external or GPU-backed frames;
- DMA-BUF/GBM and embedded graphics paths;
- RKMPP, VAAPI, or D3D hardware encoding;
- additional transports for high-motion workloads.

The mandatory pre-GA performance baseline is defined by [`../performance-optimization.md`](../performance-optimization.md) and #370. It includes practical compression and incremental delivery plus low-latency Runtime and RFB behavior. Hardware or vendor-specific acceleration is not automatically mandatory; it is activated only when accepted measurements show the portable baseline misses a declared latency or resource budget.

H.264, H.265, or AV1-style media delivery is a separate end-to-end transport and client product decision, not an automatic replacement for the ordinary RFB GUI path.

These additions must preserve the normal product principle: applications choose an integration frontend, not an internal capture, transport, or hardware implementation.

## 17. Other future extension points

Richer session and programmable application-control APIs also remain behind internal extension points.

Those capabilities must preserve one Core and one Shared Runtime, and must not create parallel performance semantics for different integration frontends.
