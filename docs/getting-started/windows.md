# Windows x86_64 Getting Started

Reference target for the current V1 line: Windows x86_64. Current candidate qualification uses **Qt 6.8.3 + MSVC x64**; no broader support claim is implied until the corresponding evidence is accepted.

## Build HyRemote

Open an x64 MSVC developer environment, then configure with the matching Qt prefix:

```bat
cmake -S . -B build -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64 ^
  -DHYREMOTE_BUILD_EXAMPLES=ON
cmake --build build --parallel
```

When running Qt-linked binaries directly from the **build tree**, make the matching Qt `bin` directory discoverable:

```bat
set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%CD%\build\remoteaccess;%PATH%
ctest --test-dir build --output-on-failure
```

That is a developer build-tree convenience, not the installed product deployment contract. A deployed application produced by `hyremote_deploy()` must run without the original HyRemote SDK/build directory on `PATH`.

The hosted workflows use the same project CMake graph. A different successful local compiler/Qt combination is not automatically a support claim.

## Embedded C++ examples

The convergence tree contains:

- `examples/widgets-basic`;
- `examples/quick-basic`;
- `examples/remote-support-showcase`.

The basic examples use the public `HyRemote::RemoteAccess` facade; they do not assemble Core/transport/capture/input objects. Construction is inert and remote access begins only through explicit product lifecycle policy.

The default listener is loopback and remote input is disabled. Enable remote control only deliberately in a controlled environment.

## Declarative QML

Use the same installed package plus the `HyRemote` QML import. The normal declarative shape is:

```qml
RemoteAccess {
    target: mainWindow
    enabled: true
}
```

The start request is applied after QML component completion so initial bindings can settle. See `docs/getting-started/qml.md`.

## Transparent QPA

V1 Transparent QPA is qualified against **exact Qt 6.8.3 private ABI**. Build the optional proxy payload with:

```text
-DHYREMOTE_WITH_QPA_PROXY=ON
```

An ordinary deployed Qt application then remains Qt-only and launches through:

```text
MyApp.exe -platform hyremote
```

See `docs/getting-started/qpa-proxy.md`; do not infer QPA compatibility with another Qt patch from the public API modes.

## Viewer

The current RFB correctness baseline is intended for standard VNC clients and defaults to loopback. Connect the viewer to `127.0.0.1:5900` unless the application selected another port. See `docs/viewer-connection.md`.

## Security

The current SecurityType None baseline is unauthenticated and unencrypted. Loopback and view-only defaults reduce accidental exposure but do not make the transport Internet-safe. See `docs/security.md`.

## Support and physical-evidence boundary

Hosted/offscreen execution can validate build, protocol, deployment and Qt-target behavior, but it cannot prove a real Windows desktop remains visible and locally interactive while a remote viewer is active.

The physical local-display/local-input + remote evidence envelope is tracked by #109 for the required V1 modes. `docs/compatibility.md` remains Candidate until the exact reference evidence executes and is accepted.
