# HyRemote V1 Public API Stability Contract

> Language / 语言: **English** | [中文](../v1-api-stability.md)

This document defines the intended application-facing stability contract for HyRemote V1 GA.

It is a product contract, not a release checklist. Before `V1.0.0.0`, a capability may still be described as open work. V1 GA begins only when the published compatibility matrix and release package declare the corresponding surface stable.

Core rule:

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

Public header:

```cpp
#include <HyRemote/RemoteAccess.h>
```

Typical use:

```cpp
HyRemote::RemoteAccess remote(&window);
remote.start();
```

For V1, `HyRemote::RemoteAccess` is the stable programmable entry point for qualified Qt Widgets and Qt Quick targets.

The V1 public C++ surface includes the types and methods explicitly installed as public API at V1 GA. Current candidate types:

- `HyRemote::RemoteAccess`;
- `HyRemote::RemoteAccessState`;
- `HyRemote::RemoteAccessErrorCode`;
- `HyRemote::RemoteAccessError`.

Behavioral invariants intended for V1 stability:

- construction is inert and opens no listener;
- `start()` and `stop()` are explicit lifecycle operations;
- configuration changes are made while stopped;
- the default listener address and remote-input policy follow current product documentation and the compatibility matrix; remote input is disabled by default;
- remote viewing and remote-control policy remain distinct;
- `Running` means the Runtime or listener is active, not that a viewer is authenticated or connected;
- application-facing errors remain HyRemote product types, not backend or protocol types;
- a non-recoverable runtime failure stays observable as `Faulted` until the owner calls `stop()`;
- acknowledging an error does not resurrect the session that produced it: unrelated viewer, capture or transport activity must not make the same acknowledged failure current again;
- if the same condition occurs again after it was acknowledged, that later occurrence must become visible again;
- transport, capture, input, and acceleration implementations may change without requiring ordinary application-source changes.

`RemoteAccess` remains non-copyable and movable. Its private implementation boundary prevents backend, session, and platform types from leaking into installed headers.

> **TODO before V1 GA:** freeze the final public method and property set plus the ABI policy from the accepted V1 headers.

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
- behavior aligned with the C++ Runtime, with no second QML-specific Session, transport, or input implementation.

The C++ implementation class behind the QML type is not a consumer API.

Current product status: a peer route backed by the same Shared Runtime.

> **Current product-line work:** complete the final QML API surface, installed-SDK examples, and compatibility qualification before declaring the QML contract stable.

## 4. Generic Plugin stability

The Generic Plugin is the preferred zero-code route when an application can preserve its normal Qt platform integration.

Application source remains Qt-only. Deployment and activation use:

```cmake
hyremote_deploy(TARGET MyApp GENERIC)
```

```text
MyApp -plugin hyremote
```

The intended V1 high-level contract preserves:

- no HyRemote application link dependency;
- use of Qt's public generic-plugin mechanism;
- preservation of the application's native platform identity;
- one Shared Runtime behind the plugin;
- product configuration through documented plugin specification fields;
- no dependency on the QPA private ABI merely because Generic is used.

The exact internal plugin class, automatic-surface controller, and composition implementation are not stable application APIs.

## 5. QPA stability category

QPA is a specialized zero-code integration path, not a generic stable Qt private-ABI promise.

Application-facing workflow:

```cmake
hyremote_deploy(TARGET MyApp QPA)
```

```text
MyApp -platform hyremote
```

Intended V1 high-level commitments:

- `-platform hyremote` remains the product launch concept;
- HyRemote-owned QPA parameters keep documented meanings;
- the frontend delegates normal platform behavior to a qualified native Qt platform integration;
- an ordinary existing Qt application does not link a HyRemote QPA target;
- the QPA frontend uses the same Shared Runtime as the other frontends.

QPA itself uses the Qt private ABI, so binary compatibility is qualified per exact Qt patch and platform combination. The current reference is Qt 6.8.3 on Windows x86_64 and Linux x86_64.

Private QPA classes, native handles, controller, composition, and provider types, and Qt private headers are not part of HyRemote's stable application API.

Current product status: a peer route backed by the same Shared Runtime.

> **Current product-line work:** complete QPA productization and exact-version qualification before including a QPA row in the final V1 supported matrix.

## 6. Installed CMake and package stability

The application-facing package contract centers on:

```cmake
find_package(HyRemote CONFIG REQUIRED)
HyRemote::RemoteAccess
hyremote_deploy(...)
```

Documented deployment forms:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

The deployment helper owns HyRemote Runtime and frontend placement. Normal applications should not discover or copy `HyRemoteRemoteAccess`, QML plugin files, `qmldir`, Core, transport libraries, Generic plugin binaries, or `qhyremote` by filename.

Generic and QPA are alternative zero-code strategies, not one combined product mode.

Installed package metadata may grow additively, but documented meanings must not be silently redefined incompatibly within the V1 line.

## 7. Runtime and artifact stability

The normal V1 C++ Runtime artifact remains one shared product library:

```text
HyRemote::RemoteAccess
HyRemoteRemoteAccess
```

Core is internal composition, not a second ordinary SDK Runtime target.

QML, Generic, and QPA payloads are frontends over that Runtime, not parallel Runtime variants.

A future implementation may physically slice private optional components for deployment size or platform reasons, provided the public product still behaves as one logical Runtime and does not require normal applications to select internal capture, transport, or hardware stacks.

## 8. Explicitly non-stable implementation surfaces

The following are not normal application compatibility surfaces:

- Core `Session` and composition internals;
- `RemoteFrame` storage implementation;
- `CaptureSource`, `InputSink`, and concrete `Transport` implementations;
- RFB protocol implementation classes;
- Widgets and Quick target-adapter classes;
- automatic-access composition and controller implementation;
- QPA private classes and native handles;
- test clients, product-fit fixtures, and acceptance harnesses;
- experimental graphics, embedded, or acceleration backends;
- DMA-BUF, GBM, RKMPP, VAAPI, or D3D implementation details.

These may evolve while the documented application contract and compatibility promise remain intact.

## 9. Evolution within V1.x

Within the V1 generation, changes to stable application surfaces should remain source-compatible and behavior-compatible unless a new major product generation explicitly introduces a breaking contract.

Normally acceptable V1.x changes:

- additive enum values where callers can tolerate extension;
- additional read-only diagnostics;
- optional capabilities and overloads;
- support for additional qualified Qt or platform rows;
- private performance or transport improvements;
- packaging improvements that preserve documented consumer usage.

Normal V1.x changes do not include:

- removing or repurposing an established public API;
- turning a zero-code path into a required application link dependency;
- exposing backend or protocol objects as required business-code concepts;
- replacing one Shared Runtime with frontend-specific public Runtime variants;
- broadening QPA private-ABI compatibility without exact qualification.

## 10. Compatibility claims

This document defines what becomes stable, not where it is supported.

Actual support is defined by [`../compatibility.md`](../compatibility.md). A public API can be stable while a particular Qt, OS, or graphics combination remains Preview, Unverified, or Unsupported.

Windows evidence does not imply Linux support. Qt public-API compatibility does not imply QPA private-ABI compatibility. Desktop qualification does not imply embedded support.

## 11. V1 GA completion items

The following product work remains before this document can be treated as the final V1 GA freeze:

- **TODO:** freeze the exact installed C++ API and ABI surface;
- **TODO:** promote the final QML contract from Preview after qualification;
- **TODO:** freeze Generic Plugin configuration fields intended for long-term support;
- **TODO:** freeze the documented QPA launch and configuration surface for each supported exact Qt row;
- **TODO:** freeze the V1 `hyremote_deploy()` argument contract and installed package metadata;
- **TODO:** publish the final V1 compatibility and support matrix.

Until then, current product status is defined by the V0.x documentation and compatibility matrix. Do not assume every future V1 commitment is already GA-stable.
