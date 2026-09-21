# HyRemote V1 Public API Stability Contract

This document defines the **intended application-facing stability contract for HyRemote V1 GA**.

It is a product contract, not a release checklist. Before `V1.0.0.0`, a capability may still be marked **Preview** or **TODO** in the product documentation. V1 GA begins only when the published compatibility matrix and release package declare the corresponding surface stable.

The central rule is simple:

> HyRemote may evolve its Core, Runtime, capture, transport, input, platform, and acceleration internals without forcing normal applications to adopt a new integration model.

## 1. Stable product model

V1 keeps one Shared Runtime and four peer integration frontends:

```text
C++ API ---------\
QML API ----------\
Generic Plugin ----> Shared Runtime -> Core
QPA --------------/
```

Qt Widgets and Qt Quick are Runtime target types, not separate product editions.

The version number does not encode frontend, Qt version, UI technology, operating system, or transport backend.

## 2. C++ API stability

The normal C++ application contract is:

```cmake
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE HyRemote::RemoteAccess)
```

with the public header:

```cpp
#include <HyRemote/RemoteAccess.h>
```

Typical use remains:

```cpp
HyRemote::RemoteAccess remote(&window);
remote.start();
```

For V1, `HyRemote::RemoteAccess` is the stable programmable facade for qualified Qt Widgets and Qt Quick targets.

The V1 public C++ surface includes the types and methods explicitly installed as public API at the V1 GA release. Current candidate types include:

- `HyRemote::RemoteAccess`;
- `HyRemote::RemoteAccessState`;
- `HyRemote::RemoteAccessErrorCode`;
- `HyRemote::RemoteAccessError`.

Behavioral invariants intended for V1 stability:

- construction is inert and opens no listener;
- `start()` / `stop()` are explicit lifecycle operations;
- configuration changes are made while stopped;
- loopback is the default listener scope;
- remote input is disabled by default;
- remote viewing and remote-control policy remain distinct;
- `Running` means the Runtime/listener is active, not that a viewer is authenticated or connected;
- application-facing errors remain HyRemote product types rather than backend/protocol types;
- transport, capture, input, and acceleration implementations may change without requiring ordinary application-source changes.

`RemoteAccess` remains non-copyable and movable. Its private implementation boundary prevents backend/session/platform types from leaking into installed headers.

> **TODO before V1 GA:** freeze the final public method/property set and ABI policy from the accepted V1 headers.

## 3. QML API stability

The declarative product entry point is:

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

The QML frontend is a thin layer over the same Shared Runtime used by the C++ API.

The intended V1 stable QML contract includes:

- URI `HyRemote`;
- public `RemoteAccess` type;
- lifecycle, target, listener, input-policy, state, diagnostics, and error properties documented for the V1 package;
- behavior aligned with the C++ Runtime rather than a second QML-specific Session/transport/input implementation.

The C++ implementation class behind the QML type is not itself a consumer API.

Current product status: **Preview**.

> **TODO V0.3/V1.0:** complete the final QML API surface, installed-SDK examples, and compatibility qualification before declaring the QML contract stable.

## 4. Generic Plugin stability

The Generic Plugin is the preferred zero-code route when an application can preserve its normal Qt platform integration.

Application source remains Qt-only. Deployment and activation use:

```cmake
hyremote_deploy(TARGET MyApp GENERIC)
```

```text
MyApp -plugin hyremote
```

The high-level V1 contract is intended to preserve:

- no HyRemote application link dependency;
- use of Qt's public generic-plugin mechanism;
- preservation of the application's native platform identity;
- one Shared Runtime behind the plugin;
- product configuration through documented plugin specification fields;
- no dependency on QPA private ABI merely because Generic is used.

The exact internal plugin class, automatic-surface controller, and composition implementation are not stable application APIs.

## 5. QPA stability category

QPA is a specialized zero-code integration path, not a generic stable Qt-private ABI promise.

The application-facing workflow is:

```cmake
hyremote_deploy(TARGET MyApp QPA)
```

```text
MyApp -platform hyremote
```

The intended V1 high-level commitments are:

- `-platform hyremote` remains the product launch concept;
- HyRemote-owned QPA parameters keep documented meanings;
- the frontend delegates normal platform behavior to a qualified native Qt platform integration;
- an ordinary existing Qt application does not link a HyRemote QPA target;
- the QPA frontend uses the same Shared Runtime as the other frontends.

QPA itself uses Qt private ABI. Therefore its binary compatibility is qualified **per exact Qt patch/platform combination**. The current reference is Qt 6.8.3 on Windows x86_64 and Linux x86_64.

Private QPA classes, native handles, controller/composition/provider types, and Qt private headers are not part of HyRemote's stable application API.

Current product status: **Preview**.

> **TODO V0.3/V0.4:** complete QPA productization and exact-version qualification before including a QPA row in the final V1 supported matrix.

## 6. Installed CMake/package stability

The application-facing package contract is centered on:

```cmake
find_package(HyRemote CONFIG REQUIRED)
HyRemote::RemoteAccess
hyremote_deploy(...)
```

Documented deployment forms include:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

The deployment helper owns HyRemote Runtime/frontend placement. Normal applications should not discover or copy `HyRemoteRemoteAccess`, QML plugin files, `qmldir`, Core, transport libraries, Generic plugin binaries, or `qhyremote` by filename.

Generic and QPA are alternative zero-code strategies; they are not intended to be combined as one product mode.

Installed package metadata may grow additively, but documented meanings must not be silently redefined incompatibly within the V1 line.

## 7. Runtime and artifact stability

The normal V1 C++ Runtime artifact remains one shared product library:

```text
HyRemote::RemoteAccess
HyRemoteRemoteAccess
```

Core is internal composition and is not a second ordinary SDK Runtime target.

The QML, Generic, and QPA payloads are frontends over that Runtime, not parallel Runtime personalities.

A future implementation may physically slice private optional components for deployment size or platform reasons, provided the public product still behaves as one logical Runtime and does not require normal applications to select internal capture/transport/hardware stacks.

## 8. Explicitly non-stable implementation surfaces

The following are not normal application-facing compatibility surfaces:

- Core `Session` and composition internals;
- `RemoteFrame` storage implementation;
- `CaptureSource`, `InputSink`, and concrete `Transport` implementations;
- RFB protocol implementation classes;
- Widgets/Quick target-adapter classes;
- automatic-access composition/controller implementation;
- QPA private classes and native handles;
- test clients, product-fit fixtures, and acceptance harnesses;
- experimental graphics/embedded/acceleration backends;
- DMA-BUF/GBM/RKMPP/VAAPI/D3D implementation details.

These may evolve as long as the documented application-facing contract and compatibility promise remain intact.

## 9. Evolution within V1.x

Within the V1 generation, changes to stable application surfaces should be source-compatible and behavior-compatible unless a new major product generation explicitly introduces a breaking contract.

Normally acceptable V1.x changes include:

- additive enum values where callers can safely tolerate extension;
- additional read-only diagnostics;
- optional capabilities and overloads;
- support for additional qualified Qt/platform rows;
- private performance or transport improvements;
- packaging improvements that preserve documented consumer usage.

The following are not normal V1.x changes:

- removing or repurposing an established public API;
- turning a zero-code path into a required application link dependency;
- exposing backend/protocol objects as required business-code concepts;
- replacing one Shared Runtime with frontend-specific public Runtime personalities;
- broadening QPA private-ABI compatibility without exact qualification.

## 10. Compatibility claims

This document defines **what becomes stable**, not **where it is supported**.

Actual support is defined by [`compatibility.md`](compatibility.md). A public API can be stable while a particular Qt/OS/graphics combination remains Preview, Unverified, or Unsupported.

Windows evidence does not imply Linux support; Qt public-API compatibility does not imply QPA private-ABI compatibility; desktop qualification does not imply embedded support.

## 11. V1 GA completion items

The following product work remains before this document can be treated as the final V1 GA freeze:

- **TODO:** freeze the exact installed C++ API/ABI surface;
- **TODO:** promote the final QML contract from Preview after qualification;
- **TODO:** freeze Generic Plugin configuration fields intended for long-term support;
- **TODO:** freeze the documented QPA launch/configuration surface for each supported exact Qt row;
- **TODO:** freeze the V1 `hyremote_deploy()` argument contract and installed package metadata;
- **TODO:** publish the final V1 compatibility/support matrix.

Until then, current product status remains defined by the V0.x documentation and compatibility matrix rather than by assuming every future V1 commitment is already GA-stable.
