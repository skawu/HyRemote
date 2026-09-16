# HyRemote V1 Public API Stability Contract

Status: **V1.0.0.0 freeze candidate; release acceptance still pending #30/#31/#32/#39/#41.**

This document defines which HyRemote surfaces become application-facing compatibility commitments at `v1.0.0.0` and which surfaces remain implementation or version-coupled integration details.

The purpose is to freeze the product model before GA, not to add convenience APIs.

## 1. Stable application-facing C++ surface

The normal Embedded C++ application surface is:

```cmake
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE HyRemote::RemoteAccess)
```

and:

```cpp
#include <HyRemote/RemoteAccess.h>
```

`HyRemote::RemoteAccess` is the stable product facade for Qt Widgets and Qt Quick targets.

The V1 public C++ surface consists of:

- `HyRemote::RemoteAccess`;
- `HyRemote::RemoteAccessState`;
- `HyRemote::RemoteAccessErrorCode`;
- `HyRemote::RemoteAccessError`;
- the methods declared by `HyRemote/RemoteAccess.h` at the accepted V1 release commit.

Frozen behavioral invariants:

- construction is inert and does not open a listener;
- one facade owns one product runtime composition;
- configuration is mutable only while Stopped;
- `start()` / `stop()` are explicit lifecycle operations;
- loopback is the safe/default listener address;
- remote input is disabled by default;
- view and input policy remain independent;
- errors and diagnostics use HyRemote product types rather than transport/backend objects;
- `connectedClientCount()` is backend-neutral and `Running` does not imply a connected viewer;
- replacing the transport/capture implementation must not require normal application-source changes.

### Ownership and value semantics

`RemoteAccess` remains non-copyable and movable. The PIMPL boundary is intentional: backend/session/capture/input/QPA implementation types do not enter its installed header.

A 1.x release must not silently make `RemoteAccess` copyable, remove move support, or change ownership semantics in a way that invalidates existing normal application use.

### Additive evolution

Within 1.x, new enum values, read-only diagnostics, overloads or optional capabilities may be added only when they preserve source compatibility and the frozen product model. Existing documented meanings may not be repurposed to expose a backend-specific model.

## 2. Declarative QML stable surface

The stable declarative application contract is the installed QML module:

```qml
import HyRemote

RemoteAccess { }
```

V1 freezes:

- URI: `HyRemote`;
- module major version: `1`;
- creatable type: `RemoteAccess`;
- properties: `target`, `enabled`, `listenAddress`, `port`, `remoteInputEnabled`, `state`, `connectedClientCount`, `errorString`, `errorCode`, `recoverableError`;
- `connectedClientCount` remains read-only;
- `clearError()` remains an invokable product-level operation;
- state/error enum meanings track the C++ facade rather than protocol/backend state.

The QML implementation class `HyRemote::Qml::QmlRemoteAccess` is not an application C++ API. Consumers use the QML URI/type metadata, not that C++ class/header.

The QML runtime remains a thin wrapper over the same `HyRemote::RemoteAccess`; 1.x must not create a second Session/transport/input implementation to evolve declarative syntax.

## 3. Stable installed CMake/package contract

Application-facing package commitments are:

```cmake
find_package(HyRemote CONFIG REQUIRED)
HyRemote::RemoteAccess
hyremote_deploy(TARGET <app> [QML] [QPA])
```

The helper modes are orthogonal deployment options:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

The normal developer must not discover/copy transport libraries, QML plugin files, `qmldir`, or the QPA plugin by private filenames.

Installed metadata such as `HyRemote_QML_IMPORT_PATH`, `HyRemote_QPA_AVAILABLE` and `HyRemote_QPA_QT_VERSION` exists to implement/document the package contract. New metadata may be added in 1.x, but existing variables used by the documented helper must not be silently redefined incompatibly.

## 4. Core is not a second application integration mode

The build/install tree exposes `HyRemote::Core` because the framework has a reusable Qt-free low-level library and source/package composition needs it. That does **not** change the user-facing product decision:

- normal application getting-started documentation must not instruct developers to assemble `Session`, `RemoteFrame`, `CaptureSource`, `InputSink` or `Transport`;
- Core does not replace `HyRemote::RemoteAccess` as the stable application facade;
- transport/capture/input backend composition remains owned by HyRemote product integration layers.

Core contracts have their own ADRs and low-level semantics, but their presence in the SDK must not be used to bypass the application API freeze.

## 5. Transparent QPA compatibility category

Transparent QPA is a mandatory V1 **product mode** but it is not a generic stable C++ ABI surface.

The V1 QPA package is deliberately version-coupled to exact Qt 6.8.3 private QPA APIs. Stable application-facing commitments are:

- the documented `-platform hyremote` launch concept;
- HyRemote-owned `hyremote-*` platform parameters and safe defaults;
- deployment through `hyremote_deploy(... QPA)`;
- preservation of the qualified native delegate path;
- no HyRemote business/UI source integration required for the ordinary existing application.

Private QPA classes, native handles, internal composite-target/provider classes and Qt private headers are explicitly outside the 1.x stable application API.

A future Qt-private ABI change may require a separately qualified QPA package implementation without changing the high-level HyRemote product mode. Support for another Qt line must be evidenced, not inferred.

## 6. Explicitly non-stable/internal surfaces

The following are not normal application-facing compatibility surfaces:

- `hyremote::Session` and other Core composition internals used to build product integration layers;
- `CaptureSource`, `InputSink`, concrete `Transport` implementations and RFB protocol classes;
- Widgets/Quick target-adapter classes;
- QPA interception/composite/provider/controller classes;
- RFB test clients/servers and product-fit harness types;
- spike APIs and experimental GBM/DMA-BUF/RKMPP paths;
- `HyRemote::Qml::QmlRemoteAccess` as a C++ type.

Internal refactoring may change these as needed provided the stable application-facing contracts and documented behavior remain intact.

## 7. Compatibility review rule for 1.x

Before merging a change that touches an installed header, QML type metadata, exported package target, package variable or `hyremote_deploy()` call shape, review must classify it as one of:

1. source/binary compatible additive change;
2. behavior clarification preserving existing semantics;
3. breaking change.

A breaking change is not allowed in a normal 1.x feature/hotfix merely because implementation would be simpler. It requires the project's frozen major-version decision process.

## 8. Release evidence boundary

This freeze document does not itself make any configuration Supported. `v1.0.0.0` still requires the reference Windows/Linux acceptance, all three integration modes, deployment/examples/docs, and the physical local+remote coexistence gates required by #30/#32/#33.

#74 currently prevents hosted jobs from receiving runners. Unexecuted jobs cannot authorize a release branch or tag.

Milestone and GA tags are created only from accepted commits merged to `main` via the Git Flow release process in `docs/git-flow-release.md`.

Governance mode: `transitional-explicit`.
