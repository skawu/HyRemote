# HyRemote Product Overview

HyRemote is a remote-access framework for existing Qt applications. It adds remote viewing and optional remote control without requiring the application to adopt a remote-desktop architecture, replace its UI stack, or expose HyRemote internals to business code.

The product is built around one Shared Runtime and four peer integration frontends. Qt Widgets and Qt Quick are target types handled by the Runtime; they are not separate product editions.

## 1. Product positioning

HyRemote is designed for teams that already own a Qt application and want remote access to become an application capability rather than a separate desktop-remote-control product.

The product focuses on:

- low-intrusion integration into existing Qt applications;
- remote view plus optional remote input;
- preserving local rendering and local input behavior;
- Qt Widgets and Qt Quick support through one Runtime architecture;
- multiple integration styles without duplicating Session/capture/input/transport implementations;
- SDK-style consumption through CMake and Qt conventions;
- a portable correctness baseline first, with embedded and hardware-specific acceleration added behind private seams later.

HyRemote is not intended to replace a general-purpose operating-system remote desktop service.

## 2. Four integration frontends

HyRemote exposes four peer entry points into the same Runtime:

| Frontend | Application integration | Product role | Current maturity |
| --- | --- | --- | --- |
| **C++ API** | Link `HyRemote::RemoteAccess` | Explicit lifecycle, policy and target control | **V0.1 primary** |
| **QML API** | `import HyRemote` | Declarative frontend over the same Runtime | **Preview** |
| **Generic Plugin** | Qt generic-plugin activation; no HyRemote app linkage | Zero-code path that preserves the native Qt platform | **V0.1 primary** |
| **QPA** | `-platform hyremote` | Specialized zero-code path using a Qt private-ABI Factory Trampoline | **Preview** |

The frontends are peers. QML, Generic, and QPA do not depend on the C++ frontend as an implementation parent. They converge on the Shared Runtime directly.

### C++ API

```cmake
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE HyRemote::RemoteAccess)
```

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&window);
remote.start();
```

Use the C++ API when the application owns remote-access policy and needs explicit start/stop, target, or configuration control.

### Generic Plugin

The application remains an ordinary Qt application:

```cmake
target_link_libraries(MyApp PRIVATE Qt6::Widgets)

find_package(HyRemote CONFIG REQUIRED)
hyremote_deploy(TARGET MyApp GENERIC)
```

Runtime activation uses Qt's generic-plugin mechanism, for example:

```text
MyApp -plugin hyremote
```

The native platform remains authoritative. A Generic Plugin application should still run on `qwindows`, `qxcb`, or another Qt-provided platform rather than on a HyRemote platform plugin.

### QML API

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

The QML object is intentionally thin and reuses the same Runtime behavior as the C++ API.

> **TODO:** complete the final installed-SDK examples and qualification required to promote the QML API from Preview.

### QPA

```text
MyApp -platform hyremote
```

QPA is different from a replacement-only `qvnc`/offscreen platform. `qhyremote` acts as a Factory Trampoline and delegates normal platform behavior to the qualified native Qt platform integration.

Because QPA uses Qt private ABI, support is exact-version qualified. The current reference is Qt 6.8.3.

> **TODO:** complete broader exact-version and physical local+remote qualification before promoting QPA from Preview.

## 3. One product Runtime

```text
C++ API ---------\
QML API ----------\
Generic Plugin ----> Shared Runtime -> Core -> transport -> viewer
QPA --------------/        |            ^
                           |            |
                    Widgets / Quick   normalized input
                    target adapters
```

The Shared Runtime owns product behavior that must remain consistent across integration modes, including:

- Qt target adapters;
- automatic surface discovery/composition;
- concrete RFB transport integration;
- security-profile handling;
- Runtime services and diagnostics;
- integration between Core, capture, input, and transport.

Core owns transport-neutral semantics such as frame lifetime, scheduling/backpressure, normalized input routing, state/error behavior, and capability abstractions.

The normal product exposes one shared Runtime artifact: `HyRemote::RemoteAccess` / `HyRemoteRemoteAccess`. Core remains internal.

## 4. Widgets and Qt Quick

Widgets and Qt Quick are both first-class Runtime targets.

- Supported Widgets targets use the Widgets adapter path.
- Supported Qt Quick targets use the Quick adapter path.
- Automatic-access frontends such as Generic and QPA discover supported top-level application surfaces through shared Runtime composition logic.

Applications do not choose capture classes, protocol objects, or input sinks directly.

Configuration-specific graphics cases such as QOpenGLWidget, QQuickWidget, Quick3D, custom FBOs, or unusual native-window ownership require explicit compatibility qualification rather than being inferred from basic Widgets/Quick support.

## 5. SDK and deployment model

HyRemote is consumed as an SDK or as source. The intended installed-SDK experience is standard CMake package discovery:

```cmake
find_package(HyRemote CONFIG REQUIRED)
```

Deployment is centralized through one helper family:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
```

Combined payloads are allowed only when the selected integration semantics make sense. Generic and QPA are alternative zero-code platform strategies and are not combined as one mode.

Applications should not manually copy internal HyRemote libraries/plugins or depend on source/build-tree runtime paths.

## 6. Current product line — V0.1 Developer Preview

V0.1 is the first usable product slice. Its goal is to let a Qt developer integrate HyRemote and obtain a working remote-access path before the full production feature set is complete.

Current reference environment:

- Windows x86_64;
- Linux x86_64;
- Qt 6.8.3 reference SDK;
- Widgets and Qt Quick target adapters;
- RFB 3.8 correctness transport;
- C++ API and Generic Plugin as the primary product paths;
- QML API and QPA available as Preview paths.

Security defaults are conservative: loopback bind by default and remote input disabled by default.

## 7. Security model

V0.1 is not an Internet-facing remote-access server.

Current behavior includes:

- loopback listener by default;
- remote input disabled by default;
- `Insecure` is loopback-only and non-loopback startup is rejected;
- `Authenticated` is a conditional capability that requires a transport-security-enabled build and a valid security descriptor; when available it provides VNC authentication without stream encryption;
- the default V0.1 build/profile does not imply authenticated transport is present;
- `AuthenticatedEncrypted` provides VeNCrypt 0.2 + X509Vnc 261 + TLS >= 1.2 with VNC Authentication inside the tunnel on Qt's OpenSSL TLS backend, and fails closed before target, transport, or listener composition when that backend or the certificate/private key is unusable - never falling back to a weaker profile.

> **TODO (V0.2):** certificate issuance/rotation policy, authenticated session management (#170), and production network policy.

See [`security.md`](security.md).

## 8. Product roadmap

HyRemote evolves by user value rather than by internal subsystem completion:

| Product line | Product goal |
| --- | --- |
| **V0.1 — Use It** | A developer can integrate and use the product through C++ API or Generic Plugin |
| **V0.2 — Trust It** | Security, authenticated sessions, and production network behavior |
| **V0.3 — Productize It** | All four frontends fully packaged, deployed, documented, and taught |
| **V0.4 — Qualify It** | Compatibility, real-world application, performance, and release-candidate qualification |
| **V1.0 — Stabilize It** | First GA compatibility/support contract |
| **V1.1 — Embed It** | Embedded Linux/platform deployment and feature slicing |
| **Later performance line — Accelerate It** | Measured low-copy/hardware acceleration where evidence justifies it |
| **Later programmable line — Differentiate It** | Advanced application policy, observability, privacy, sessions, and business integration |

Version numbers never encode C++/QML/Generic/QPA, platform, Qt version, or UI family.

## 9. Long-term technical direction

The architecture keeps several future capabilities behind private seams so they can evolve without changing normal application integration:

- alternative capture paths;
- external/GPU-backed frame storage;
- DMA-BUF/GBM and embedded graphics integration;
- hardware video encoding such as RKMPP/VAAPI/D3D paths;
- additional transports for high-motion or media-heavy workloads;
- advanced session and application-control APIs.

These are product improvements, not requirements for ordinary applications to understand HyRemote internals.

## 10. Product principles

- **Existing applications first.** Remote access should not require rewriting the UI or business logic.
- **One Runtime.** Integration convenience must not create duplicate product stacks.
- **Public API first.** Stable application integration uses public contracts; Qt private ABI is isolated to QPA.
- **Generic before private ABI when sufficient.** Zero-code integration should prefer the public Qt Generic Plugin path when it satisfies the application.
- **Local behavior remains authoritative.** Remote access augments the application rather than redefining its native platform behavior.
- **Correctness before optimization.** Portable, bounded behavior is established before hardware-specific performance work.
- **Evidence-based compatibility.** Similarity to another Qt version, OS, graphics path, or SoC does not automatically create a support claim.
- **No protocol lock-in.** RFB is the current transport baseline, not the permanent product boundary.

## 11. Current TODOs

The following are intentionally visible product gaps rather than hidden process notes:

- **TODO V0.1:** finalize polished C++/Generic learning examples and clean installed-SDK C++ Widgets/Quick product fixtures.
- **TODO V0.2:** encrypted transport, authenticated sessions, production network policy.
- **TODO V0.3:** full four-frontend productization, deployment matrix, and complete example curriculum.
- **TODO V0.4:** Qt compatibility expansion, real-world applications, performance qualification, and release-candidate hardening.
- **TODO V1.1+:** embedded Linux packaging/deployment, then measured hardware acceleration and advanced programmable control.

For exact current environment status, see [`compatibility.md`](compatibility.md) and [`known-limitations.md`](known-limitations.md).
