# HyRemote Product Overview

> Language / 语言: **English** | [中文](../product-overview.md)

HyRemote is a remote-access framework for existing Qt applications. It adds remote viewing and optional remote control without requiring the application to adopt a remote-desktop architecture, replace its UI stack, or expose HyRemote internals to business code.

The product uses one Shared Runtime and four peer integration frontends. Qt Widgets and Qt Quick are target types handled by the Runtime; they are not separate product editions.

## 1. Product positioning

HyRemote is for teams that already own a Qt application and want remote access to become an application capability instead of a separate desktop-remote-control product.

The product focuses on:

- low-intrusion integration into existing Qt applications;
- remote view plus optional remote input;
- preserving local rendering and local input behavior;
- Qt Widgets and Qt Quick support through one Runtime architecture;
- multiple integration styles without duplicating Session, capture, input, or transport implementations;
- SDK-style consumption through CMake and Qt conventions;
- a portable correctness and practical interactive-performance baseline before GA. Hardware or vendor-specific acceleration is added only when measurements justify it.

HyRemote does not replace a general-purpose operating-system remote desktop service or a dedicated video-streaming platform.

## 2. Four integration frontends

HyRemote exposes four peer entry points into the same Runtime:

| Frontend | Application integration | Product role | Current state |
| --- | --- | --- | --- |
| **C++ API** | Link `HyRemote::RemoteAccess` | Explicit lifecycle, policy and target control | peer route |
| **QML API** | `import HyRemote` | Declarative frontend over the same Runtime | peer route |
| **Generic Plugin** | Qt generic-plugin activation; no HyRemote application linkage | Zero-code path that preserves the native Qt platform | peer route |
| **QPA** | `-platform hyremote` | Specialized zero-code path using a Qt private-ABI Factory Trampoline | peer route |

The four frontends are peers. QML, Generic, and QPA do not depend on the C++ frontend as an implementation parent. They connect directly to the Shared Runtime.

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

Runtime activation uses Qt's generic-plugin mechanism:

```text
MyApp -plugin hyremote
```

The native platform remains authoritative. A Generic Plugin application continues to run on `qwindows`, `qxcb`, or another Qt-provided platform, not on a HyRemote platform plugin.

### QML API

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

The QML object is a thin layer over the same Runtime behavior as the C++ API.

### QPA

```text
MyApp -platform hyremote
```

QPA differs from a replacement-only `qvnc` or offscreen platform. `qhyremote` acts as a Factory Trampoline and delegates normal platform behavior to the qualified native Qt platform integration.

QPA uses the Qt private ABI, so support is exact-version qualified. The current reference is Qt 6.8.3.

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

The Shared Runtime owns product behavior that must stay consistent across integration modes:

- Qt target adapters;
- automatic surface discovery and composition;
- concrete RFB transport integration;
- security-profile handling;
- Runtime services and diagnostics;
- integration between Core, capture, input, and transport;
- viewer-aware capture and freshness policy shared by all frontends.

Core owns transport-neutral semantics such as frame lifetime, scheduling and backpressure, normalized input routing, state and error behavior, and capability abstractions.

The normal product exposes one shared Runtime artifact: `HyRemote::RemoteAccess` / `HyRemoteRemoteAccess`. Core remains internal.

## 4. Widgets and Qt Quick

Widgets and Qt Quick are both first-class Runtime targets:

- supported Widgets targets use the Widgets adapter;
- supported Qt Quick targets use the Quick adapter;
- automatic-access frontends such as Generic and QPA discover supported top-level application surfaces through shared Runtime composition.

Applications do not choose capture classes, protocol objects, input sinks, encoders, damage modes, or GPU performance backends directly.

Configuration-specific graphics cases such as QOpenGLWidget, QQuickWidget, Quick3D, custom FBOs, or unusual native-window ownership require explicit compatibility qualification. Basic Widgets or Quick support does not imply support for those cases.

## 5. SDK and deployment model

HyRemote is consumed as an SDK or as source. The installed SDK uses standard CMake package discovery:

```cmake
find_package(HyRemote CONFIG REQUIRED)
```

Deployment is centralized in one helper family:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
```

Combined payloads are allowed only when the selected integration semantics make sense. Generic and QPA are alternative zero-code platform strategies and are not combined as one mode.

Applications must not manually copy internal HyRemote libraries or plugins, and must not depend on source-tree or build-tree runtime paths.

## 6. Current product line: V0.3.0.0 Product Preview

V0.3.0.0 is the current released version. Its goal is self-service adoption: a Qt developer can integrate HyRemote and obtain a working remote-access path. The version includes:

- Windows x86_64 and Linux x86_64;
- the Qt 6.8.3 reference SDK;
- Widgets and Qt Quick target adapters;
- the RFB 3.8 correctness transport;
- the C++ API, QML API, Generic Plugin, and QPA as peer frontends;
- HyRemoteTool, a companion client that connects to a remote HyRemote-enabled application by explicit IP and port and provides viewing, view-only or control input, FPS/throughput telemetry, and connection diagnostics.

HyRemoteTool management-plane capabilities are outside V0.3.0.0. See [`../versioning.md`](../versioning.md) for version semantics and later product lines.

## 7. Security model

The current unauthenticated and unencrypted mode is not an Internet-facing security claim.

Current behavior:

- listener on `0.0.0.0:5921` by default;
- remote input disabled by default;
- `Insecure` is unauthenticated and unencrypted and must be deployed only where that exposure is acceptable;
- `Authenticated` can provide viewer-compatible VNC authentication when the required capability and configuration are present. It does not imply stream encryption;
- `AuthenticatedEncrypted` is not implemented and must fail closed while the encrypted backend is unavailable.

See [`../security.md`](../security.md) for the exact security contract.

## 8. Product roadmap

HyRemote evolves by user value, not by internal subsystem completion. Current release authority lives in the GitHub roadmap and release issues, [`../versioning.md`](../versioning.md), and `.github/release/release-trains.json`. Technology names do not reserve Feature versions.

Performance follows the same rule: the practical interactive baseline is required before RC or GA. Lower-level hardware or vendor acceleration is activated only by measured evidence.

## 9. Long-term technical direction

The architecture keeps several future capabilities behind internal extension points so they can evolve without changing normal application integration:

- alternative capture paths;
- external or GPU-backed frame storage;
- DMA-BUF/GBM and embedded graphics integration;
- hardware encoding such as RKMPP, VAAPI, or D3D;
- additional transports for high-motion or media-heavy workloads;
- richer session and application-control APIs.

These are product improvements. Ordinary applications do not need to understand HyRemote internals.

### Long-lived performance programme

Performance is a permanent product property, not a one-time optimization milestone. The accepted model:

- **Freshness first:** the newest useful state is preferred over queued historical frames;
- **Automatic by default:** users do not select encoders, compression levels, damage modes, capture depths, or flow windows;
- **Demand driven:** no-viewer and static workloads avoid unnecessary capture, encode, and network work;
- **Interaction focused:** remote input should prioritize a fresh visual response;
- **Adaptive delivery:** ordinary localized GUI may use region or damage delivery, while sustained high-motion Quick may use full-frame compression;
- **Bounded delivery:** capture, Core handoff, encoded pending work, and per-viewer network state stay bounded;
- **Portable first:** platform or GPU acceleration is admitted only when the portable baseline misses a measured latency or resource target.

For the initial ordinary direct-LAN reference profile, #370 defines an interaction-latency target of P50 at most 200 ms and P95 at most 500 ms under the stated qualified conditions. This is a bounded reference SLO, not a claim for every network or media-like workload.

See [`../performance-optimization.md`](../performance-optimization.md) for the detailed model.

## 10. Product principles

- **Existing applications first.** Remote access must not require rewriting the UI or business logic.
- **One Runtime.** Integration convenience must not create duplicate product stacks.
- **Public API first.** Stable application integration uses public contracts; Qt private ABI is isolated to QPA.
- **Peer integration technologies.** C++, QML, Generic, and QPA differ technically but share one product Runtime and are not ranked by implementation convenience.
- **Local behavior remains authoritative.** Remote access augments the application; it does not redefine its native platform behavior.
- **Freshness before throughput.** Showing the newest useful state is more important than delivering every stale intermediate frame.
- **Automatic performance.** Normal users should not need transport, capture, or network tuning knowledge to obtain the qualified experience.
- **Portable baseline before acceleration.** Hardware or private optimizations require a measured product blocker, not a theoretical opportunity.
- **Evidence-based compatibility and performance.** Similarity to another Qt version, OS, graphics path, SoC, or benchmark does not create a support or performance claim.
- **No protocol lock-in.** RFB is the current transport baseline, not the permanent product boundary.

## 11. Performance evidence and regression

Performance qualification records the exact candidate SHA, environment, workload, and distribution metrics. A green functional CI run is not sufficient if a change materially degrades an accepted interaction-latency, frame-freshness, or resource envelope.

GitHub Workflow is preferred for deterministic, synthetic, protocol, and x86 evidence. RK3588, EGLFS, Wayland, GPU, and local-HMI claims require the actual reference environment.

See #370 and #374 for the long-lived programme and machine-regression evidence plan.

## 12. Current TODOs

Current gaps are tracked through the active GitHub roadmap, not through invented fixed technology version slots. Performance-specific pre-GA work includes:

- practical compressed and incremental delivery (#261/#144);
- RFB Continuous Updates, Fence, and per-viewer delivery control (#371);
- demand-driven, input-triggered, and adaptive Runtime capture (#372);
- repeatable performance evidence and regression gates (#374);
- final production and physical reference qualification (#9 and the applicable platform issues).

For exact environment status, see [`../compatibility.md`](../compatibility.md), [`../known-limitations.md`](../known-limitations.md), and [`../performance-optimization.md`](../performance-optimization.md).
