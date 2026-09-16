# Source Consumption

This guide is for vendored/cross-build/source-tree integration. It does **not** require or imply `find_package(HyRemote)` for the HyRemote source tree itself.

## Vendored `add_subdirectory()`

The parent project provides its Qt SDK/toolchain, configures the desired HyRemote options, and adds HyRemote normally:

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network Widgets)

set(HYREMOTE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(HYREMOTE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(HYREMOTE_BUILD_SPIKES OFF CACHE BOOL "" FORCE)
set(HYREMOTE_BUILD_CORE ON CACHE BOOL "" FORCE)
set(HYREMOTE_BUILD_REMOTE_ACCESS ON CACHE BOOL "" FORCE)

add_subdirectory(third_party/HyRemote)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Qt6::Widgets HyRemote::RemoteAccess)
```

For Quick applications, provide the normal Qt Quick components and link the same `HyRemote::RemoteAccess` target.

## Why this is separate from installed-SDK consumption

`find_package(HyRemote CONFIG REQUIRED)` consumes an **installed/exported** HyRemote package. `add_subdirectory()` consumes the HyRemote build targets directly. Both paths converge on the same public product target and C++ facade, but their CMake acquisition step is different.

## Cross-build intent

This path inherits the caller's compiler, sysroot, CMake toolchain file, and target Qt SDK. Future Embedded Linux platform families extend this model; they must not introduce a second project-specific build system.

Hardware/backend options remain explicit and must not be enabled merely because a target SoC might support them. The portable correctness path remains the baseline until platform evidence justifies an optimized backend.

## Repository validation fixture

`tests/consumer-source` is the canonical source-consumer fixture. It deliberately sets `HYREMOTE_SOURCE_DIR`, calls `add_subdirectory()`, and links `HyRemote::RemoteAccess` without using an installed HyRemote package.
