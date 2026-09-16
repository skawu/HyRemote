# Source Consumption

This guide is for vendored/cross-build/source-tree integration. It does **not** require or imply `find_package(HyRemote)` for the HyRemote source tree itself.

## Vendored `add_subdirectory()`

A normal application provides its Qt SDK/toolchain and adds HyRemote directly:

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)

add_subdirectory(third_party/HyRemote)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Qt6::Widgets HyRemote::RemoteAccess)
```

That is the normal Embedded C++ source-consumer setup. When HyRemote is included as a subproject, its own tests, examples and architecture spikes default to **OFF** automatically; applications do not need to know or override those developer-only switches.

For Quick applications, request the application's normal Qt Quick components and link the same `HyRemote::RemoteAccess` target.

V1 keeps the artifact model identical to installed-SDK consumption: `HyRemote::RemoteAccess` is the shared application library and Core remains statically composed behind it. Source consumption does not create a second public runtime model.

## Application code

```cpp
HyRemote::RemoteAccess remote(&window);
remote.start();
```

No target-adapter, Session, transport or backend selection is required in normal application code. The default listener is loopback and remote input is disabled until the application explicitly enables it.

## One deployment contract

The same deployment helper is available from the source tree. The four V1 call shapes are identical to installed-SDK consumption:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

For example:

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

The helper owns the selected HyRemote payload. Source users do not manually copy the shared runtime, Core/backend files, QML plugin files or qhyremote by private filename.

`add_subdirectory(... EXCLUDE_FROM_ALL)` is supported. HyRemote's source-tree product payloads remain internal build targets; the deployment helper adds only the local build dependencies needed to materialize the selected payload before installation. It does **not** turn those targets into application link dependencies. In particular, a Transparent QPA application remains Qt-only even when qhyremote and `HyRemoteRemoteAccess` are built from source for deployment.

## Optional product modes

The default source-consumer path is the C++ shared facade with Widgets/Quick adapters and the bounded C++ RFB correctness transport when the corresponding Qt modules are available.

Only opt into additional V1 integration packages when the application needs them:

```cmake
set(HYREMOTE_BUILD_QML_API ON CACHE BOOL "" FORCE)       # import HyRemote
set(HYREMOTE_WITH_QPA_PROXY ON CACHE BOOL "" FORCE)      # exact Qt 6.8.3 qhyremote plugin
add_subdirectory(third_party/HyRemote EXCLUDE_FROM_ALL)
```

For QML source consumption, HyRemote publishes its build-tree import root to the same deployment helper used by the installed package. The application still consumes the `HyRemote` URI rather than linking a second HyRemote C++ product target.

For QPA source consumption, the ordinary application still links Qt only and launches with `-platform hyremote`. QPA remains exact-Qt-private-ABI qualified to Qt 6.8.3 for the V1 reference line. Enabling the source option does not create a generic Qt-private compatibility promise.

These options do not create alternate Core/transport stacks. QML remains a thin wrapper and QPA remains a plugin entry point over the same shared runtime.

## Why source and installed acquisition remain separate

`find_package(HyRemote CONFIG REQUIRED)` consumes an **installed/exported** package. `add_subdirectory()` consumes HyRemote build targets directly. Their acquisition steps differ; their application API, artifact shape and deployment helper are deliberately the same.

Internal build-tree aliases/targets may exist during source consumption. They are implementation/build metadata and are not stable installed 1.x application APIs.

## Clean deployment requirement

A successful build is not sufficient evidence. The deployed application must run from its deployment prefix without depending on the HyRemote build tree, the original HyRemote SDK prefix, or manual `PATH`, `LD_LIBRARY_PATH`, `QT_PLUGIN_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH`, `QML2_IMPORT_PATH` or `QML_IMPORT_PATH` workarounds.

On Linux, the V1 fixtures additionally verify where the HyRemote/Qt shared objects are actually resolved from so a surviving SDK/build-tree RUNPATH cannot masquerade as a clean deployment. On Windows, clean runtime checks narrow `PATH` to the deployed application plus required system directories.

## Cross-build intent

This path inherits the caller's compiler, sysroot, CMake toolchain file and target Qt SDK. Future Embedded Linux families extend this model; they must not introduce a second project-specific build system or a more complex application API.

Hardware/backend options remain internal development concerns. The portable correctness path remains the baseline until platform evidence justifies an optimized backend.

## Repository validation fixtures

Source acceptance deliberately reuses the same product-facing fixtures rather than maintaining source-only look-alike applications:

- `tests/consumer-source` proves the normal Embedded C++ `HyRemote::RemoteAccess` source path and product-only subproject defaults;
- `tests/consumer-installed-qml` is configured in source mode to prove QML-only and combined QML+QPA deployment using the same declarative application used for installed-SDK acceptance;
- `tests/consumer-installed-qpa` is configured in source mode and compiles the real E4 ordinary Qt-only application source to prove Transparent QPA deployment without a HyRemote application link target.

The `SDK consumption` Windows/Linux matrix executes all four source deployment call shapes separately. QML-only is not inferred from QML+QPA because those paths use different deployment branches. These remain candidate repository/automation facts until the required hosted jobs actually execute and pass; a #74 no-runner result is not acceptance evidence.
