# HyRemote V1 Public API Stability Contract

Status: **V1.0.0.0 freeze candidate; release acceptance still pending #30/#31/#32/#39/#41.**

This document defines which HyRemote surfaces become application-facing compatibility commitments at `v1.0.0.0` and which remain implementation/version-coupled details.

The purpose is to freeze a simple product model before GA, not to expose internal composition.

## 1. Stable application-facing C++ surface

The normal C++ application contract is one shared library target:

```cmake
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE HyRemote::RemoteAccess)
```

and one installed header:

```cpp
#include <HyRemote/RemoteAccess.h>
```

Normal baseline use remains:

```cpp
HyRemote::RemoteAccess remote(&window);
remote.start();
```

`HyRemote::RemoteAccess` is the stable product facade for supported Qt Widgets and Qt Quick targets.

The V1 public C++ surface consists of:

- `HyRemote::RemoteAccess`;
- `HyRemote::RemoteAccessState`;
- `HyRemote::RemoteAccessErrorCode`;
- `HyRemote::RemoteAccessError`;
- the methods declared by `HyRemote/RemoteAccess.h` at the accepted V1 release commit.

Frozen behavioral invariants:

- construction is inert and opens no listener;
- one facade owns one product runtime composition;
- configuration is mutable only while Stopped;
- `start()` / `stop()` are explicit lifecycle operations;
- loopback is the default listener address;
- port 5900 is the default listener port;
- remote input is disabled by default;
- view and input policy remain independent;
- errors/diagnostics use HyRemote product types rather than backend objects;
- `connectedClientCount()` is backend-neutral and `Running` does not imply a connected viewer;
- replacing transport/capture/input internals must not require normal application-source changes.

### V1 binary/runtime shape

The normal V1 C++ artifact is the shared `HyRemoteRemoteAccess` library. Core is statically composed behind it and is not installed/exported as a second SDK product target. `BUILD_SHARED_LIBS` must not silently switch the normal application contract between static and shared products.

This artifact decision is a usability boundary: ordinary applications link one HyRemote library and deployment carries one HyRemote C++ runtime library.

### Ownership and value semantics

`RemoteAccess` remains non-copyable and movable. The PIMPL boundary is intentional: backend/session/capture/input/QPA implementation types do not enter its installed header.

A 1.x release must not silently make `RemoteAccess` copyable, remove move support, or change ownership semantics in a way that invalidates existing normal application use.

### Additive evolution

Within 1.x, new enum values, read-only diagnostics, overloads or optional capabilities may be added only when they preserve source compatibility and the frozen product model. Existing meanings may not be repurposed to expose backend-specific composition.

## 2. Declarative QML stable surface

The stable declarative application contract is:

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
- `clearError()` remains a product-level operation;
- state/error meanings track the C++ facade rather than protocol/backend state.

`HyRemote::Qml::QmlRemoteAccess` is not a consumer C++ API. The QML implementation remains a thin wrapper over the same shared `HyRemote::RemoteAccess`; 1.x must not create a second Session/transport/input implementation for declarative syntax.

## 3. Stable installed CMake/package contract

Application-facing package commitments are:

```cmake
find_package(HyRemote CONFIG REQUIRED)
HyRemote::RemoteAccess
hyremote_deploy(TARGET <app> [QML] [QPA])
```

The documented deployment shapes are:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

The helper owns HyRemote runtime/module placement. A normal developer must not discover/copy `HyRemoteRemoteAccess`, QML plugin files, `qmldir`, Core, transport libraries or `qhyremote` by filename.

Installed metadata such as `HyRemote_QML_IMPORT_PATH`, `HyRemote_QPA_AVAILABLE` and `HyRemote_QPA_QT_VERSION` exists to implement/document the package contract. New metadata may be added in 1.x, but existing documented meanings must not be silently redefined incompatibly.

## 4. Core remains an internal/source architecture boundary

The repository keeps the Qt-free Core implementation and its architecture contracts so capture, transport and platform backends remain separable and testable. In the V1 installed product, however, Core is statically composed into `HyRemoteRemoteAccess` and is not exported as `HyRemote::Core` to ordinary SDK consumers.

Normal getting-started material must not instruct developers to assemble `Session`, `RemoteFrame`, `CaptureSource`, `InputSink` or `Transport`. The existence of Core source code does not create a second application integration mode and does not replace `HyRemote::RemoteAccess` as the stable facade.

Future low-level developer packaging, if ever introduced, must be a deliberate separately reviewed product decision and must not silently expand the V1 normal application surface.

## 5. Transparent QPA compatibility category

Transparent QPA is a mandatory V1 product mode but not a generic stable Qt-private C++ ABI surface.

The V1 QPA package is deliberately coupled to exact Qt 6.8.3 private QPA APIs. Stable application-facing commitments are:

- the `-platform hyremote` launch concept;
- HyRemote-owned `hyremote-*` platform parameters and safe defaults;
- deployment through `hyremote_deploy(... QPA)`;
- preservation of the qualified native delegate path;
- no HyRemote application linkage required for an ordinary existing Qt application.

The product artifact is `qhyremote` as a Qt platform MODULE. It internally reuses the same shared `RemoteAccess` runtime. Private QPA classes, native handles, composite/provider/controller classes and Qt private headers are outside the 1.x stable application API.

A future Qt-private ABI change may require a separately qualified QPA package without changing the high-level user workflow. Support for another Qt line must be evidenced, not inferred.

## 6. Explicitly non-stable/internal surfaces

The following are not normal application-facing compatibility surfaces:

- `hyremote::Session` and Core composition internals;
- `CaptureSource`, `InputSink`, concrete `Transport` implementations and RFB protocol classes;
- Widgets/Quick target-adapter classes;
- QPA interception/composite/provider/controller classes;
- RFB test clients/servers and product-fit harness types;
- spike APIs and experimental GBM/DMA-BUF/RKMPP paths;
- `HyRemote::Qml::QmlRemoteAccess` as a C++ type.

Internal refactoring may change these provided the stable application contract and documented behavior remain intact.

## 7. Compatibility review rule for 1.x

Before merging a change that touches an installed header, QML type metadata, exported package target, artifact shape, package variable or `hyremote_deploy()` call shape, classify it as:

1. source/binary compatible additive change;
2. behavior clarification preserving existing semantics;
3. breaking change.

A breaking change is not allowed in a normal 1.x feature/hotfix merely because implementation would be simpler.

## 8. Release evidence boundary

This freeze document does not itself make any configuration Supported. `v1.0.0.0` still requires reference Windows/Linux acceptance, all three integration modes, clean installed/deployed consumers, examples/docs and the required physical local+remote coexistence gates.

#74 currently prevents hosted jobs from receiving runners. Unexecuted jobs cannot authorize a release branch or tag.

Milestone and GA tags are created only from accepted commits merged to `main` through the release process in `docs/git-flow-release.md`.

Governance mode: `transitional-explicit`.
