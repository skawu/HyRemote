# Declarative QML consumption and deployment

Status: **V0.0.2.0 installed-SDK contract; executable dual-OS evidence pending #74**

Issues: #31, #69, #41.

HyRemote's QML API is a thin declarative surface over the same `HyRemote::RemoteAccess` runtime used by Embedded C++. It does not introduce a second Session, capture stack, input implementation or transport.

## 1. Installed SDK

Use the exact Qt line supported by the HyRemote package and locate the standalone HyRemote prefix normally:

```cmake
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(HyRemote CONFIG REQUIRED)
```

When the installed package contains the QML API, `HyRemoteConfig.cmake` publishes:

```cmake
HyRemote_QML_IMPORT_PATH
```

This is the absolute QML import root containing the installed `HyRemote/qmldir`. It is package metadata, not a plugin filename that application developers should copy manually.

Normal application QML remains concise:

```qml
import QtQuick
import HyRemote

Item {
    RemoteAccess {
        id: remote
        target: someSupportedQtTarget
        enabled: false
    }
}
```

Construction is inert. `enabled` must be set/requested explicitly before a listener starts. Loopback and remote-input-disabled defaults are inherited from the same C++ facade.

## 2. Define the application QML module normally

For example:

```cmake
qt_add_executable(MyQmlApp
    main.cpp
)

qt_policy(SET QTP0001 NEW)
qt_add_qml_module(MyQmlApp
    URI MyApplication
    VERSION 1.0
    QML_FILES Main.qml
)

target_link_libraries(MyQmlApp PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)
```

The application does not link or instantiate Core, Session, capture, input or transport implementation targets.

## 3. Install the application, then call the single HyRemote deploy hook

Qt's generated deployment script is an install-time rule, so register the executable install first:

```cmake
install(TARGETS MyQmlApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

hyremote_deploy(TARGET MyQmlApp QML)
```

`QML` is explicit. HyRemote then:

1. appends its installed `HyRemote_QML_IMPORT_PATH` to the target's existing `QT_QML_IMPORT_PATH` without replacing caller paths;
2. calls Qt's supported `qt_generate_deploy_qml_app_script()`;
3. never also calls `qt_generate_deploy_app_script()` for the same QML target;
4. lets Qt's `qmlimportscanner` and QML deployment machinery discover and deploy `import HyRemote` recursively.

The developer does **not** name the HyRemote QML plugin, copy `qmldir`, discover transport libraries, or implement a second deployment scanner.

This import-path bridge is important for a standalone SDK prefix: Qt 6.8.3's QML build/deployment tooling reads the application's `QT_QML_IMPORT_PATH` when scanning imports. Merely exporting the package path as an unused CMake variable would not constitute a complete deployment workflow.

## 4. Why the helper owns the import-path bridge

A user could manually write an `IMPORT_PATH` into `qt_add_qml_module()`, but making every installed-SDK consumer repeat HyRemote's private install layout would violate the product deployment contract.

The installed package already knows its QML root. `hyremote_deploy(... QML)` therefore connects that package metadata to Qt's normal scanner target property while preserving any application-defined paths.

No hard-coded Qt installation path is used.

## 5. Runtime import layout

Qt's QML deployment machinery deploys imported QML modules into its normal application QML directory (for non-macOS platforms, normally `qml` under the deployment prefix) and deploys the required plugin/runtime dependencies according to Qt's platform rules.

HyRemote does not require copying its QML module into the user's Qt SDK tree.

## 6. Clean-consumer acceptance fixture

`tests/consumer-installed-qml` is intentionally configured only after HyRemote has been installed to a staging prefix. It:

- uses `find_package(HyRemote CONFIG REQUIRED)`;
- imports `HyRemote` from QML;
- calls only `hyremote_deploy(TARGET ... QML)` for HyRemote-owned deployment integration;
- builds and installs independently from the HyRemote source targets;
- verifies the deployed `qml/HyRemote/qmldir` and plugin exist;
- loads the installed/deployed application with `QT_QPA_PLATFORM=offscreen` as a packaging/import smoke.

The Windows/Linux Qt 6.8.3 workflow contains this sequence, but current hosted jobs are blocked before runner assignment by #74. Until those commands actually execute, this is an implemented acceptance gate, not a passing support claim.

## 7. Source/build-tree applications

The package variable above is specifically the installed-SDK import root. A source-integrated application may already have HyRemote's generated QML module in its project build graph/import paths.

`hyremote_deploy(... QML)` only appends `HyRemote_QML_IMPORT_PATH` when that installed-package variable exists and is non-empty. It does not erase or replace source-project import paths.

## 8. Error and lifecycle semantics

The declarative `RemoteAccess` surface preserves product semantics rather than hiding failures:

- invalid address/port/policy mutations do not silently change the accepted value;
- configuration changes while Running are rejected rather than causing implicit restart;
- `enabled: true` is transactional: if the shared C++ facade cannot start, QML does not remain `enabled` while the runtime is Stopped;
- state and diagnostics expose product-level values, not backend-specific errors;
- destroying the QML wrapper stops its owned `RemoteAccess` runtime.

## 9. Security boundary

The current RFB correctness baseline uses SecurityType None. QML does not weaken or override the common safe defaults:

- loopback listener by default;
- remote input disabled by default;
- no Internet-safe authentication/encryption claim.

See the common security documentation before exposing a listener beyond a trusted/local test environment.

## 10. Evidence boundary

The clean installed-SDK consumer and deployment workflow must execute on both reference operating systems before #69/#31 can close. Current GitHub-hosted jobs fail before any runner steps execute under #74; those failures are infrastructure evidence, not code pass/fail evidence.

Hosted offscreen execution also does not prove physical local-visible + remote coexistence. That product-level evidence remains separate from packaging/import correctness.

Governance mode: `transitional-explicit`.
