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

That is the normal source-consumer setup. When HyRemote is included as a subproject, its own tests, examples and architecture spikes default to **OFF** automatically; applications do not need to know or override those developer-only switches.

For Quick applications, request the application's normal Qt Quick components and link the same `HyRemote::RemoteAccess` target.

V1 keeps the artifact model identical to installed-SDK consumption: `HyRemote::RemoteAccess` is the shared application library and Core remains statically composed behind it. Source consumption does not create a second public runtime model.

## Application code

```cpp
HyRemote::RemoteAccess remote(&window);
remote.start();
```

No target-adapter, Session, transport or backend selection is required in normal application code. The default listener is loopback and remote input is disabled until the application explicitly enables it.

## Deployment

The same deployment helper is available from the source tree:

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

This installs the shared `HyRemoteRemoteAccess` runtime with the application. Source users do not manually copy the DLL/SO or Core/backend files.

## Optional product modes

The default source-consumer path is the C++ shared facade with Widgets/Quick adapters and the RFB correctness transport when the corresponding Qt modules are available.

Only opt into the additional V1 integration packages when the application actually needs them:

```cmake
set(HYREMOTE_BUILD_QML_API ON CACHE BOOL "" FORCE)       # import HyRemote
set(HYREMOTE_WITH_QPA_PROXY ON CACHE BOOL "" FORCE)      # exact Qt 6.8.3 qhyremote plugin
add_subdirectory(third_party/HyRemote)
```

These options do not create alternate Core/transport stacks. QML remains a thin wrapper and QPA remains a plugin entry point over the same shared runtime.

## Why this is separate from installed-SDK consumption

`find_package(HyRemote CONFIG REQUIRED)` consumes an **installed/exported** package. `add_subdirectory()` consumes HyRemote build targets directly. Their acquisition steps differ; their application API, artifact shape and deployment helper are deliberately the same.

## Cross-build intent

This path inherits the caller's compiler, sysroot, CMake toolchain file and target Qt SDK. Future Embedded Linux families extend this model; they must not introduce a second project-specific build system or a more complex application API.

Hardware/backend options remain internal development concerns. The portable correctness path remains the baseline until platform evidence justifies an optimized backend.

## Repository validation fixture

`tests/consumer-source` is the canonical source-consumer fixture. It deliberately does **not** pre-disable HyRemote tests/examples/spikes; instead it verifies that subproject defaults keep those developer assets out of the application build automatically. It then links only `HyRemote::RemoteAccess` and installs through the same `hyremote_deploy()` contract.

V1 GA requires the deployed fixture to run without build-tree or SDK runtime-path assistance.
