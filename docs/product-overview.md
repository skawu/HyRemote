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
- a portable correctness and practical interactive-performance baseline before GA, with hardware/vendor-specific acceleration added only behind private seams when measurements justify it.

HyRemote is not intended to replace a general-purpose operating-system remote desktop service or a dedicated video-streaming platform.

## 2. Four integration frontends

HyRemote exposes four peer entry points into the same Runtime:

| Frontend | Application integration | Product role | Current maturity |
| --- | --- | --- | --- |
| **C++ API** | Link `HyRemote::RemoteAccess` | Explicit lifecycle, policy and target control | peer route |
| **QML API** | `import HyRemote` | Declarative frontend over the same Runtime | **Preview** |
| **Generic Plugin** | Qt generic-plugin activation; no HyRemote app linkage | Zero-code path that preserves the native Qt platform | peer route |
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
- integration between Core, capture, input, and transport;
- automatic viewer-aware capture/freshness policy used by all frontends.

Core owns transport-neutral semantics such as frame lifetime, scheduling/backpressure, normalized input routing, state/error behavior, and capability abstractions.

The normal product exposes one shared Runtime artifact: `HyRemote::RemoteAccess` / `HyRemoteRemoteAccess`. Core remains internal.

## 4. Widgets and Qt Quick

Widgets and Qt Quick are both first-class Runtime targets.

- Supported Widgets targets use the Widgets adapter path.
- Supported Qt Quick targets use the Quick adapter path.
- Automatic-access frontends such as Generic and QPA discover supported top-level application surfaces through shared Runtime composition logic.

Applications do not choose capture classes, protocol objects, input sinks, encoders, damage modes, or GPU performance backends directly.

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

## 6. Current product line — V0.2 LAN trial

V0.1 is the first usable product slice. Its goal is to let a Qt developer integrate HyRemote and obtain a working remote-access path before the full production feature set is complete.

Current reference environment:

- Windows x86_64;
- Linux x86_64;
- Qt 6.8.3 reference SDK;
- Widgets and Qt Quick target adapters;
- RFB 3.8 correctness transport;
- C++ API and Generic Plugin as the primary product paths;
- QML API and QPA available as Preview paths.

## 7. Security model

HyRemote's current unauthenticated/unencrypted mode is not an Internet-facing security claim.

Current behavior includes:

- listener on `0.0.0.0:5921` by default;
- remote input disabled by default;
- `Insecure` is unauthenticated and unencrypted and must be deployed only where that exposure is acceptable;
- `Authenticated` may provide viewer-compatible VNC authentication when the required capability/configuration is present, still without implying stream encryption;
- `AuthenticatedEncrypted` is not implemented and must fail closed while the encrypted backend is unavailable.

See [`security.md`](security.md) for the exact current security contract.

## 8. Product roadmap

HyRemote evolves by user value rather than by internal subsystem completion. Exact current release authority lives in GitHub roadmap/release issues and `versioning.md`; technology names must not reserve Feature versions.

Performance follows the same rule: the practical interactive baseline is required before RC/GA, while lower-level hardware/vendor acceleration is activated only by measured evidence.

## 9. Long-term technical direction

The architecture keeps several future capabilities behind private seams so they can evolve without changing normal application integration:

- alternative capture paths;
- external/GPU-backed frame storage;
- DMA-BUF/GBM and embedded graphics integration;
- hardware encoding such as RKMPP/VAAPI/D3D paths;
- additional transports for high-motion or media-heavy workloads;
- advanced session and application-control APIs.

These are product improvements, not requirements for ordinary applications to understand HyRemote internals.

### Long-lived performance programme

HyRemote treats performance as a permanent product property rather than a one-time optimization milestone.

The accepted model is:

- **Freshness first** — newest useful state is preferred over queued historical frames;
- **automatic by default** — users do not select encoders, compression levels, damage modes, capture depths or flow windows;
- **demand driven** — no-viewer/static workloads should avoid unnecessary capture/encode/network work;
- **interaction focused** — remote input should prioritize a fresh visual response;
- **adaptive delivery** — ordinary localized GUI may use region/damage delivery while sustained high-motion Quick may use full-frame compression;
- **bounded delivery** — capture, Core handoff, encoded pending work and per-viewer network state stay bounded;
- **portable first** — platform/GPU acceleration is admitted only when the portable/product baseline misses a measured latency/resource target.

For the initial ordinary direct-LAN reference profile, #370 defines an interaction-latency target of P50 <= 200 ms and P95 <= 500 ms under the stated qualified conditions. This is a bounded reference SLO, not a claim for every network or media-like workload.

See [`performance-optimization.md`](performance-optimization.md) for the detailed latency model, workloads, Runtime/Core/RFB responsibility split, compression/damage/Continuous/Fence/flow-control route, platform-acceleration gate and long-term regression policy.

## 10. Product principles

- **Existing applications first.** Remote access should not require rewriting the UI or business logic.
- **One Runtime.** Integration convenience must not create duplicate product stacks.
- **Public API first.** Stable application integration uses public contracts; Qt private ABI is isolated to QPA.
- **Peer integration technologies.** C++ / QML / Generic / QPA differ technically but share one product Runtime and are not ranked by implementation convenience.
- **Local behavior remains authoritative.** Remote access augments the application rather than redefining its native platform behavior.
- **Freshness before throughput.** Showing the newest useful state is more important than delivering every stale intermediate frame.
- **Automatic performance.** Normal users should not need transport/capture/network tuning knowledge to obtain the qualified experience.
- **Portable baseline before acceleration.** Hardware/private optimizations require a measured product blocker, not theoretical opportunity.
- **Evidence-based compatibility and performance.** Similarity to another Qt version, OS, graphics path, SoC or benchmark does not automatically create a support/performance claim.
- **No protocol lock-in.** RFB is the current transport baseline, not the permanent product boundary.

## 11. Performance evidence and regression

Performance qualification records exact candidate SHA, environment, workload and distribution metrics. Functional CI being green is not sufficient if a change materially degrades an accepted interaction-latency, frame-freshness or resource envelope.

GitHub Workflow is preferred for deterministic/synthetic/protocol/x86 evidence. Physical RK3588/EGLFS/Wayland/GPU/local-HMI claims require the actual reference environment.

See #370 and #374 for the long-lived programme and machine-regression evidence plan.

## 12. Current TODOs

Current gaps are tracked through the active GitHub roadmap rather than by inventing fixed technology version slots. Performance-specific pre-GA work includes:

- practical compressed + incremental delivery (#261/#144);
- RFB Continuous Updates/Fence/per-viewer delivery control (#371);
- demand-driven/input-triggered/adaptive Runtime capture (#372);
- repeatable performance evidence/regression gates (#374);
- final production and physical reference qualification (#9 and the applicable platform issues).

For exact current environment status, see [`compatibility.md`](compatibility.md), [`known-limitations.md`](known-limitations.md), and [`performance-optimization.md`](performance-optimization.md).
