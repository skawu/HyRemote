# Windows x86_64 Getting Started

Reference target for the current V1 line: Windows x86_64, Qt 6.8.x; current CI/product work uses Qt 6.8.3 with MSVC x64. Transparent QPA is qualified only for exact Qt 6.8.3.

## Build the normal product

Open an x64 MSVC developer environment, then configure with the matching Qt prefix:

```bat
cmake -S . -B build -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --parallel
```

A plain configure is intentionally product-only: it builds the standard C++ `HyRemote::RemoteAccess` path but does not build repository tests, examples or spikes. QML and Transparent QPA are opt-in integration packages.

To create an installed SDK, also provide `-DCMAKE_INSTALL_PREFIX=<prefix>` and run `cmake --install build`.

## Maintainer / acceptance test build

Repository validation is explicit rather than hidden in the normal product build:

```bat
cmake -S . -B build-test -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64 ^
  -DHYREMOTE_BUILD_TESTS=ON ^
  -DHYREMOTE_BUILD_EXAMPLES=ON
cmake --build build-test --parallel
```

When running Qt-linked build-tree tests, make the matching Qt and HyRemote build runtime directories discoverable:

```bat
set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%CD%\build-test\remoteaccess;%PATH%
ctest --test-dir build-test --output-on-failure
```

Those PATH additions are a **build-tree test concern**. A deployed application must obtain Qt/HyRemote runtime files through deployment and must not depend on the original SDK/build tree.

The hosted workflows use the same CMake graph but explicitly enable the gates they execute. A different successful local compiler/Qt combination is not automatically a support claim.

## Run the public examples

With `HYREMOTE_BUILD_EXAMPLES=ON`, the current V1 candidate contains `widgets-basic`, `quick-basic`, `qml-basic`, `qpa-proxy-existing-app`, and `remote-support-showcase` according to the enabled integration packages.

E1/E2 use the same public `HyRemote::RemoteAccess` facade. By default they are view-only; use their explicit remote-input option only for a controlled test environment.

## Viewer

The current RFB correctness baseline is intended for standard VNC clients and defaults to loopback. Connect the viewer to `127.0.0.1:5900` unless the application selected another port. See `docs/viewer-connection.md`.

## Security

Do not expose the current unauthenticated SecurityType None correctness transport directly to untrusted networks. Loopback is the safe default. Remote input is separately opt-in. See `docs/security.md`.

## Support boundary

A hosted/offscreen build is not by itself proof of locally visible display/input coexistence. Final Windows product acceptance must record the exact Qt/toolchain/viewer configuration and the required local+remote behavior. Physical E1/E2/E3/E4 evidence is tracked by #109; exact status remains in `docs/compatibility.md` and milestone issues #30/#31/#32.
