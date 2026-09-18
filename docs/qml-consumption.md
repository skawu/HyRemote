# Declarative QML consumption and deployment

Status: **V0.0.2.0 installed/source contract; executable dual-OS evidence pending #74**

Issues: #31, #69, #39, #41.

HyRemote's QML API is a thin declarative surface over the same `HyRemote::RemoteAccess` runtime used by Embedded C++. It does not introduce a second Session, capture stack, input implementation or transport, and its backing library is not a second C++ SDK target.

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

This is the absolute QML import root containing the installed `HyRemote/qmldir`. It is package metadata used by `hyremote_deploy(... QML)`, not a C++ link target or plugin filename that application developers should copy manually.

Normal application QML remains concise:

```qml
import QtQuick
import HyRemote

Item {
    RemoteAccess {
        target: someSupportedQtTarget
        enabled: true
    }
}
```

`enabled: true` is an explicit declarative start request. The wrapper defers the actual shared-runtime start until QML component completion so initial `target` and policy bindings can settle; object construction itself remains inert. Loopback and remote-input-disabled defaults are inherited from the same C++ facade.

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

The application does not link HyRemote Core, QML backing, Session, capture, input or transport implementation targets. `import HyRemote` is the declarative product boundary.

## 3. Install the application, then call the single HyRemote deploy hook

Qt's generated deployment script is an install-time rule, so register the executable install first:

```cmake
install(TARGETS MyQmlApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

hyremote_deploy(TARGET MyQmlApp QML)
```

`QML` is explicit and selects an already available HyRemote QML payload. It does not enable/build the product mode by itself. HyRemote then:

1. requires the selected installed/source acquisition to have published an absolute `HyRemote_QML_IMPORT_PATH`;
2. appends that import root to the target's existing `QT_QML_IMPORT_PATH` without replacing caller paths;
3. calls Qt's supported `qt_generate_deploy_qml_app_script()`;
4. never also calls `qt_generate_deploy_app_script()` for the same QML target;
5. lets Qt's `qmlimportscanner` and QML deployment machinery discover and deploy `import HyRemote` recursively;
6. deploys the same shared `HyRemoteRemoteAccess` runtime used by C++ and QPA modes.

If HyRemote was built/installed without the QML API, `hyremote_deploy(... QML)` fails during application configuration. It does not produce a partial package that only fails later at `import HyRemote`.

The developer does **not** name the HyRemote QML plugin, copy `qmldir`, discover transport libraries, or implement a second deployment scanner.

## 4. Why the helper owns the import-path bridge

A user could manually write an `IMPORT_PATH` into `qt_add_qml_module()`, but making every HyRemote consumer repeat an installed or build-tree module layout would violate the product deployment contract.

Each acquisition mode already knows its own QML root. `hyremote_deploy(... QML)` therefore connects that HyRemote-owned metadata to Qt's normal scanner target property while preserving any application-defined paths.

No hard-coded Qt installation path is used.

## 5. Runtime import layout

Qt's QML deployment machinery deploys imported QML modules into its normal application QML directory (for non-macOS V1 platforms, normally `qml` under the deployment prefix) and deploys the required plugin/runtime dependencies according to Qt's platform rules.

HyRemote does not require copying its QML module into the user's Qt SDK tree or setting a persistent QML environment variable.

## 6. Installed clean-consumer acceptance fixture

`tests/consumer-installed-qml` is first configured against an installed HyRemote package for package acceptance. It:

- uses `find_package(HyRemote CONFIG REQUIRED)` in installed mode;
- imports `HyRemote` from QML;
- calls only `hyremote_deploy(TARGET ... QML)` for the QML-only case;
- is configured a second time for the distinct `hyremote_deploy(TARGET ... QML QPA)` case rather than inferring the combination from separate payload tests;
- builds and installs independently from the HyRemote source targets;
- verifies the deployed `qml/HyRemote/qmldir`, QML payload and shared `HyRemoteRemoteAccess` runtime exist;
- loads the deployed application after SDK/import/runtime path assistance is removed.

The Windows/Linux Qt 6.8.3 workflows contain this sequence, but current hosted jobs are blocked before runner assignment by #74. Until those commands actually execute, this is an implemented acceptance gate, not a passing support claim.

## 7. Source/add_subdirectory applications

Source acquisition deliberately uses the same application QML and the same deployment helper. Enable the QML package before adding HyRemote:

```cmake
set(HYREMOTE_BUILD_QML_API ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/HyRemote EXCLUDE_FROM_ALL)
```

The HyRemote QML module publishes its absolute build-tree import root through the same `HyRemote_QML_IMPORT_PATH` abstraction used by the installed package. The helper appends that root before asking Qt to generate the QML deployment script.

Because `EXCLUDE_FROM_ALL` does not itself build optional payload targets, the helper also adds **build-only** dependencies on the source QML backing/plugin payload. These dependencies do not appear in the application's `LINK_LIBRARIES`; the application remains QML/import-driven rather than gaining another HyRemote C++ product target.

The SDK-consumption acceptance matrix configures the same clean fixture in source mode for both QML-only and QML+QPA. QML-only remains a separate required execution path because it uses the normal shared-runtime supplemental script instead of the QPA supplemental script.

See `docs/guide/install.md` for the complete source acquisition matrix.

## 8. Error and lifecycle semantics

The declarative `RemoteAccess` surface preserves product semantics rather than hiding failures:

- initial property order is not part of the product contract; an `enabled: true` request waits for component completion before the shared runtime starts;
- invalid address/port/policy mutations do not silently change the accepted value;
- configuration changes while Running are rejected rather than causing implicit restart;
- `enabled: true` is transactional: if the shared C++ facade cannot start, QML returns to `enabled: false` while exposing product-level error state;
- a non-recoverable runtime fault remains `Faulted` until explicit stop/`enabled: false` cleanup; `clearError()` does not hide an active fatal fault;
- state, connected-client count and diagnostics expose product-level values, not backend-specific objects/errors;
- destroying the QML wrapper stops its owned `RemoteAccess` runtime.

## 9. Security boundary

The current RFB correctness baseline uses SecurityType None. QML does not weaken or override the common safe defaults:

- loopback listener by default;
- remote input disabled by default;
- no Internet-safe authentication/encryption claim.

See the common security documentation before exposing a listener beyond a trusted/local test environment.

## 10. Evidence boundary

The clean installed/source QML deployment workflows must execute on both reference operating systems before their milestone/GA authorities can accept the corresponding claims. Current GitHub-hosted jobs fail before any runner steps execute under #74; those failures are infrastructure evidence, not code pass/fail evidence.

Hosted offscreen execution also does not prove physical local-visible + remote coexistence. The cross-mode physical acceptance envelope is tracked by #109 and remains separate from packaging/import correctness.

Governance mode: `transitional-explicit`.
