# HyRemote Product Architecture

HyRemote uses one Core, one Shared Runtime, and four peer integration frontends. The architecture is designed so that applications can choose how to enter the product without creating separate Session, capture, transport, input, security, or performance implementations.

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

- **C++ API** — explicit application API through `HyRemote::RemoteAccess`;
- **QML API** — declarative frontend through `import HyRemote`;
- **Generic Plugin** — public-Qt zero-code frontend that preserves the native Qt platform;
- **QPA** — private-ABI zero-code frontend that enters through a Factory Trampoline and delegates to the native Qt platform integration.

Widgets and Qt Quick are Runtime target-adapter dimensions, not integration modes.

## 2. Core

Core owns product semantics that do not depend on Qt UI technology, Qt private APIs, a concrete transport backend, or a specific platform graphics stack.

Core responsibilities include:

- Session lifecycle/state semantics;
- frame lifetime and transport-neutral frame metadata;
- capture scheduling and bounded frame handoff;
- queue/drop/backpressure policy;
- normalized product errors and state;
- normalized input routing abstractions;
- transport-neutral capability description.

Core must not depend on:

- Qt Widgets / Qt Quick / QML;
- Qt private/QPA APIs;
- concrete RFB implementation types;
- OpenGL/RHI/EGLFS/DRM/GBM/DMA-BUF APIs;
- hardware encoder SDKs;
- integration-frontend policy.

This boundary keeps the product model stable while target, transport, platform, and acceleration implementations evolve.

## 3. Shared Runtime

The Shared Runtime is the one Qt-aware product implementation used by every frontend.

Runtime responsibilities include:

- Widgets and Qt Quick target adapters;
- concrete RFB transport integration;
- security-profile implementation and validation;
- automatic application-surface discovery/composition;
- application target/input routing;
- runtime component factories and services;
- cross-mode diagnostics and operational behavior;
- automatic performance policy such as viewer-aware capture demand, input-triggered freshness and adaptive pacing.

The normal shared artifact remains:

```text
HyRemote::RemoteAccess
HyRemoteRemoteAccess
```

There is no separate Widgets Runtime, Quick Runtime, Generic Runtime, QPA Runtime, Embedded Runtime, or Accelerated Runtime product personality.

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

The C++ frontend is a product facade into the Shared Runtime. It does not own the Runtime implementation used by the other frontends.

Construction is inert. Configuration is performed while stopped, followed by explicit `start()`.

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

Current product state: a peer route, on the same shared runtime.

> **TODO:** complete the final installed-SDK examples and qualification required for full productization.

## 6. Generic Plugin frontend

The Generic Plugin uses Qt public plugin APIs (`QGenericPlugin`). It is a zero-code integration route that keeps the application's normal native Qt platform authoritative.

```text
Qt application
    |
    +--> native Qt platform (qwindows/qxcb/...)
    |
    +--> HyRemote Generic Plugin -> Shared Runtime
```

The application itself remains Qt-only. Deployment uses:

```cmake
hyremote_deploy(TARGET MyApp GENERIC)
```

Activation uses Qt's generic-plugin mechanism, for example:

```text
MyApp -plugin hyremote
```

Generic must not inherit QPA private-ABI requirements. Its defining contract is that the application's native platform identity remains normal.

Current product state: a peer route.

## 7. QPA frontend

QPA is a specialized zero-code integration path for cases where process-level platform interception is required.

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

It does not reimplement a full platform integration and must not silently substitute an offscreen/minimal/qvnc-style backend.

Current reference delegates:

- Windows: `qwindows`;
- Linux/X11: `qxcb`.

QPA alone owns the Qt private-ABI dependency. Qualification is exact-Qt-patch specific; the current reference is Qt 6.8.3.

Current product state: a peer route, on the same shared runtime.

> **TODO:** broaden exact-version/platform qualification only where real compatibility evidence exists.

## 8. Widgets and Qt Quick target adapters

Widgets and Qt Quick are handled behind the Shared Runtime.

### Widgets

Supported QWidget targets use the Widgets target adapter and a CPU-readable correctness capture path.

### Qt Quick

Supported `QQuickWindow` targets use the Quick target adapter and the public asynchronous Quick capture path.

Applications use the same product entry points regardless of target family. They do not select capture backend classes directly.

Configuration-specific cases such as QOpenGLWidget, QQuickWidget, Quick3D, custom FBOs, or unusual native-window ownership require explicit compatibility qualification rather than being inferred from basic Widgets/Quick behavior.

Performance policy follows the same rule: adapters expose truthful capture/damage capability, while Shared Runtime and the transport decide how to use those capabilities. Applications do not choose internal capture/performance backends.

## 9. Automatic application surface model

Generic and QPA both need automatic access to application surfaces. That logic belongs once in Runtime rather than being duplicated in either frontend.

Runtime automatic access is responsible for:

- discovering supported application-owned top-level surfaces;
- composing them into the remote application view where required;
- routing normalized remote input back to the appropriate target;
- keeping local native platform behavior authoritative.

A frontend must not implement its own competing composition/controller stack.

## 10. Frame, freshness and backpressure model

`RemoteFrame` is transport-neutral and owns its storage lifetime.

The frame contract carries information such as:

- dimensions and pixel/storage format;
- owned storage lifetime;
- content timing;
- damage information;
- backend-neutral capabilities.

Frames cross a bounded Core handoff before transport dispatch. Slow clients must not create unbounded frame retention or block the Qt GUI/render path.

The near-live policy favors the newest relevant frame rather than building an ever-growing queue. Performance work therefore follows a **freshness-first** rule: stale intermediate work may be dropped or coalesced when a newer useful state exists.

Core owns boundedness and transport-neutral freshness semantics. Shared Runtime owns automatic capture demand/pacing. The RFB transport owns compression, incremental-update state, Continuous Updates/Fence and per-viewer network delivery behavior. None of those protocol/platform-specific mechanisms are exposed as normal application tuning knobs.

See [`performance-optimization.md`](performance-optimization.md) for the long-lived interaction-latency SLO, adaptive capture/delivery model and regression programme.

## 11. Input model

Remote input is normalized before it reaches a Qt target adapter.

The product distinguishes:

- pointer movement;
- pointer buttons;
- wheel/scroll;
- keys and modifiers;
- committed text when the protocol provides sufficient information.

Input is application-scoped rather than desktop-wide. HyRemote does not use OS-wide virtual HID/uinput injection as the normal path.

Held key/button state is cleaned up on disconnect and on explicit Runtime stop so a remote peer cannot leave the local application in a stuck input state.

Remote input is also a performance signal: after the application processes a remote action and has an opportunity to update/render, Runtime may prioritize a fresh capture/update so interactive response does not depend solely on a periodic capture timer.

## 12. Runtime state

The public Runtime state model is:

```text
Stopped -> Starting -> Running
                    \-> Faulted
Running  -> Faulted
Running/Faulted -> stop -> Stopped
```

`Running` means the Runtime/listener is active; it does not mean a viewer is authenticated or connected.

Operational information such as `connectedClientCount()` is diagnostic state, not an authorization identity.

Capture activity is not itself connection liveness: a static connected session may legitimately send no framebuffer payload until content changes while the TCP/RFB session remains alive.

## 13. Transport boundary

The current correctness transport is bounded RFB 3.8 behind a private Runtime/Core seam.

RFB is not part of the normal application API. A later transport may reuse the same integration frontends, Runtime target adapters, Core frame model, input model and freshness contract.

RFB-specific performance mechanisms stay private to the transport, including:

- encoding negotiation / Raw fallback;
- practical compression;
- incremental/non-incremental update state;
- Continuous Updates / Fence where interoperable;
- bounded per-viewer delivery flow;
- cursor pseudo-encoding where useful.

Current security behavior:

- listener on `0.0.0.0:5921` by default;
- remote input disabled by default;
- `Insecure` is unauthenticated and unencrypted: trusted LAN only, not Internet-safe;
- authenticated profile may use RFB VNC authentication;
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

The helper owns HyRemote payload placement and Runtime closure. Applications should not manually copy internal libraries/plugins or point runtime search paths back at an SDK/build tree.

Generic deployment carries both the HyRemote generic payload and the application's normal native Qt platform plugin. QPA deployment carries `qhyremote` plus its exact native delegate chain.

## 15. Compatibility boundary

The current desktop reference environment is:

- Windows x86_64;
- Linux x86_64;
- Qt 6.8.3 reference SDK.

Public-Qt integration paths and private-QPA compatibility are different claims:

- C++ / QML / Generic primarily rely on Qt public APIs;
- QPA is exact private-ABI qualified.

A working configuration on one OS, Qt patch, graphics backend, application type, or performance profile does not automatically qualify another.

See [`compatibility.md`](compatibility.md).

## 16. Performance extension seams

The architecture intentionally keeps performance-specific implementation choices behind internal seams:

- demand-driven/adaptive capture policy in Shared Runtime;
- more efficient graphics capture adapters;
- external/GPU-backed frames;
- DMA-BUF/GBM and embedded graphics paths;
- RKMPP/VAAPI/D3D hardware encoding;
- additional transports for high-motion workloads.

The mandatory pre-GA performance baseline is defined by [`performance-optimization.md`](performance-optimization.md) / #370 and includes practical compression/incremental delivery plus low-latency Runtime/RFB behavior. Hardware/vendor-specific acceleration is **not** automatically mandatory: it is activated only when accepted measurements show the portable/product baseline misses a declared latency/resource budget.

H.264/H.265/AV1-style media delivery is a separate end-to-end transport/client product decision, not an automatic replacement for the ordinary RFB GUI path.

These additions must preserve the normal product principle: ordinary applications choose an integration frontend, not an internal capture/transport/hardware implementation.

## 17. Other future extension seams

Other product capabilities intentionally remain behind internal seams as well, including richer session and programmable application-control APIs.

Those capabilities must preserve one Core + one Shared Runtime and must not create parallel performance semantics for different integration frontends.

## 18. Product testing and qualification architecture

HyRemote tests the **product promise and support claims**, not the current implementation topology or raw CTest count. The canonical product-test architecture is [`internal/test-strategy.md`](internal/test-strategy.md).

The strategy defines nine Product Acceptance Contracts covering low-intrusion integration, native non-interference, remote experience, frontend equivalence, deployability, reliability/boundedness, security truth, responsiveness/efficiency and compatibility truth. Core, Runtime, adapters, frontends, transport and deployment provide the cheapest sufficient evidence for those contracts; implementation-specific evidence such as RFB or a particular capture/backend path stays subordinate to the stable product contract.

Testing is intentionally staged into verification, product validation, qualification and exact-candidate release acceptance. Hosted/headless verification does not substitute for physical/native Windows/Linux evidence when a support claim includes local display/input, native platform, graphics or loader behavior, while physical evidence does not substitute for clean build/install/deploy evidence.

Development velocity is itself a test-system constraint. Ordinary edit/PR loops use risk-based selection and cheap deterministic evidence; expensive clean deployment, viewer, stress/soak, broad compatibility and physical qualification run only at the gates whose product risk requires them. The strategy defines explicit developer/PR/nightly/release execution budgets and rules for consolidating parameter variants instead of allowing every new case to become a permanent blocking test identity.

A support row is accepted only when it can be traced from the declared compatibility boundary to the required Product Acceptance Contracts and to executed evidence for the exact candidate/environment. CI status or test count alone is never a support claim.