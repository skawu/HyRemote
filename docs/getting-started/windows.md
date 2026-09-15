# Windows x86_64 Getting Started

Reference target for the current V1 line: Windows x86_64, Qt 6.8.x; current CI/product work uses Qt 6.8.3 with MSVC x64.

## Build HyRemote

Open an x64 MSVC developer environment, then configure with the matching Qt prefix:

```bat
cmake -S . -B build -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --parallel
```

When running Qt-linked tests or examples, ensure the matching Qt `bin` directory is on `PATH`:

```bat
set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH%
ctest --test-dir build --output-on-failure
```

The current hosted workflows use the same project CMake graph; a different successful local compiler/Qt combination is not automatically a support claim.

## Run the public examples

Once #60 / PR #61 lands, build with `HYREMOTE_BUILD_EXAMPLES=ON` and run either `widgets-basic` or `quick-basic`. The application opens remote access only when its public `RemoteAccess::start()` path executes.

By default the examples are view-only. Use their explicit remote-input option only for a controlled test environment.

## Viewer

The current RFB correctness baseline is intended for standard VNC clients and defaults to loopback. Connect the viewer to `127.0.0.1:5900` unless the application selected another port. See `docs/viewer-connection.md`.

## Security

Do not expose the current unauthenticated SecurityType None correctness transport directly to untrusted networks. Loopback is the safe default. Remote input is separately opt-in. See `docs/security-model.md`.

## Support boundary

A CI build is not by itself proof of locally visible display/input coexistence. Final Windows product acceptance must record the exact Qt/toolchain/viewer configuration and local+remote behavior in `docs/compatibility.md` / #30.
