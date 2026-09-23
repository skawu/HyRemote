# HyRemote Product Architecture

HyRemote uses one Core, one Shared Runtime, and four peer integration frontends. The architecture is designed so that applications can choose how to enter the product without creating separate Session, capture, transport, input, or security implementations.

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
- cross-mode diagnostics and operational behavior.

The normal shared artifact remains:

```text
HyRemote::RemoteAccess
HyRemoteRemoteAccess
```

There is no separate Widgets Runtime, Quick Runtime, Generic Runtime, or QPA Runtime product personality.

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

Current product state: **V0.1 primary path**.

## 5. QML API frontend

The QML frontend is a thin declarative layer over the same Shared Runtime:

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

It does not create a second Session, transport, capture, state, or error model.

Current product state: **Preview**.

> **TODO:** complete the final installed-SDK examples and qualification required for full productization.

## 6. Generic Plugin frontend

The Generic Plugin uses Qt public plugin APIs (`QGenericPlugin`). It is the preferred zero-code integration route when an application can keep its normal native Qt platform.

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

Current product state: **V0.1 primary path**.

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

Current product state: **Preview**.

> **TODO:** broaden exact-version/platform qualification only where real compatibility evidence exists.

## 8. Widgets and Qt Quick target adapters

Widgets and Qt Quick are handled behind the Shared Runtime.

### Widgets

Supported QWidget targets use the Widgets target adapter and a CPU-readable correctness capture path.

### Qt Quick

Supported `QQuickWindow` targets use the Quick target adapter and the public asynchronous Quick capture path.

Applications use the same product entry points regardless of target family. They do not select capture backend classes directly.

Configuration-specific cases such as QOpenGLWidget, QQuickWidget, Quick3D, custom FBOs, or unusual native-window ownership require explicit compatibility qualification rather than being inferred from basic Widgets/Quick behavior.

## 9. Automatic application surface model

Generic and QPA both need automatic access to application surfaces. That logic belongs once in Runtime rather than being duplicated in either frontend.

Runtime automatic access is responsible for:

- discovering supported application-owned top-level surfaces;
- composing them into the remote application view where required;
- routing normalized remote input back to the appropriate target;
- keeping local native platform behavior authoritative.

A frontend must not implement its own competing composition/controller stack.

## 10. Frame and backpressure model

`RemoteFrame` is transport-neutral and owns its storage lifetime.

The frame contract carries information such as:

- dimensions and pixel/storage format;
- owned storage lifetime;
- content timing;
- damage information;
- backend-neutral capabilities.

Frames cross a bounded Core handoff before transport dispatch. Slow clients must not create unbounded frame retention or block the Qt GUI/render path.

The near-live policy favors the newest relevant frame rather than building an ever-growing queue.

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

## 13. Transport boundary

The current correctness transport is bounded RFB 3.8 behind a private Runtime/Core seam.

RFB is not part of the normal application API. A later transport may reuse the same integration frontends, Runtime target adapters, Core frame model, and input model.

Current security behavior:

- listener on `0.0.0.0:5921` by default;
- remote input disabled by default;
- `Insecure` is unauthenticated and unencrypted: trusted LAN only, not Internet-safe;
- authenticated profile may use RFB VNC authentication;
- stream encryption is not implemented in the current baseline;
- `AuthenticatedEncrypted` fails closed before a listener is opened while the encrypted backend is unavailable.

> **TODO V0.2:** VeNCrypt/TLS, certificate policy, authenticated sessions, and production network policy.

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

A working configuration on one OS, Qt patch, graphics backend, or application type does not automatically qualify another.

See [`compatibility.md`](compatibility.md).

## 16. Future extension seams

The architecture intentionally keeps the following behind internal seams:

- more efficient graphics capture;
- external/GPU-backed frames;
- DMA-BUF/GBM and embedded graphics paths;
- RKMPP/VAAPI/D3D hardware encoding;
- additional transports for high-motion workloads;
- richer session and programmable application-control APIs.

> **TODO V1.1+:** embedded-platform feature slicing and deployment.
>
> **TODO later performance line:** introduce low-copy/hardware paths only where measurement shows a real product blocker.
>
> **TODO later programmable line:** advanced session policy, observability, privacy/exclusion, target switching, and business integration.

These additions must preserve the normal product principle: ordinary applications choose an integration frontend, not an internal capture/transport/hardware implementation.