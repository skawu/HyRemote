# Source Consumption

This guide is for vendored/cross-build/source-tree integration. It does **not** require or imply `find_package(HyRemote)` for the HyRemote source tree itself.

## Vendored `add_subdirectory()`

The parent project provides its Qt SDK/toolchain, configures the desired HyRemote options, and adds HyRemote normally:

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network Widgets)

set(HYREMOTE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(HYREMOTE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(HYREMOTE_BUILD_SPIKES OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/HyRemote)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Qt6::Widgets HyRemote::RemoteAccess)
```

For Quick applications, provide the normal Qt Quick components and link the same `HyRemote::RemoteAccess` target.

V1 keeps the artifact model identical to installed-SDK consumption: `HyRemote::RemoteAccess` is the shared application library and Core remains statically composed behind it. Source consumption does not create a second public runtime model.

## Application code

```cpp
HyRemote::RemoteAccess remote(&window);
remote.start();
```

No target-adapter, Session, transport or backend selection is required in normal application code.

## Deployment

The same deployment helper is available from the source tree:

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

This installs the shared `HyRemoteRemoteAccess` runtime with the application. Source users do not manually copy the DLL/SO or Core/backend files.

## Why this is separate from installed-SDK consumption

`find_package(HyRemote CONFIG REQUIRED)` consumes an **installed/exported** package. `add_subdirectory()` consumes HyRemote build targets directly. Their acquisition steps differ; their application API, artifact shape and deployment helper are deliberately the same.

## Cross-build intent

This path inherits the caller's compiler, sysroot, CMake toolchain file and target Qt SDK. Future Embedded Linux families extend this model; they must not introduce a second project-specific build system or a more complex application API.

Hardware/backend options remain explicit and internal. The portable correctness path remains the baseline until platform evidence justifies an optimized backend.

## Repository validation fixture

`tests/consumer-source` is the canonical source-consumer fixture. It calls `add_subdirectory()`, links only `HyRemote::RemoteAccess` as the HyRemote application target, then installs through the same `hyremote_deploy()` contract. V1 GA requires the deployed fixture to run without build-tree or SDK runtime-path assistance.
