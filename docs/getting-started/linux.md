# Linux x86_64 Getting Started

Reference target for the current V1 line: Linux x86_64. Current candidate qualification uses **Qt 6.8.3 + GCC x86_64**; no broader support claim is implied until the corresponding evidence is accepted.

## Build HyRemote

Configure against the target Qt installation:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.3/gcc_64 \
  -DHYREMOTE_BUILD_EXAMPLES=ON
cmake --build build --parallel
```

When running directly from the **build tree**, make the matching Qt and HyRemote shared libraries discoverable if the environment does not already provide a runtime path:

```bash
export LD_LIBRARY_PATH="$PWD/build/remoteaccess:/opt/Qt/6.8.3/gcc_64/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
ctest --test-dir build --output-on-failure
```

That environment variable is a developer build-tree convenience, not the installed deployment contract. A normal application installed through `hyremote_deploy()` must run from its deployed tree without the original HyRemote SDK/build directory on `LD_LIBRARY_PATH`.

Desktop Linux results do not imply Embedded Linux/EGLFS support.

## Embedded C++ examples

The convergence tree contains:

- `examples/widgets-basic`;
- `examples/quick-basic`;
- `examples/remote-support-showcase`.

They use the same public `HyRemote::RemoteAccess` facade. The default endpoint is loopback and remote input is disabled until explicitly enabled.

## Declarative QML

A QML application imports the same product runtime declaratively:

```qml
RemoteAccess {
    target: mainWindow
    enabled: true
}
```

The actual start is deferred until QML component completion so initial bindings can settle. See `docs/getting-started/qml.md`.

## Display backend

Normal local product acceptance must run through the intended native desktop Qt platform backend. Hosted protocol E2E may use `offscreen`, software rendering or Xvfb to validate the viewer-to-application path; those runs are not evidence that a physical local display/input path remained usable at the same time.

## Transparent QPA

V1 Transparent QPA is qualified against **exact Qt 6.8.3 private ABI** and the native `qxcb` delegate. Build the optional proxy payload with:

```text
-DHYREMOTE_WITH_QPA_PROXY=ON
```

A deployed ordinary Qt application then remains Qt-only and launches through:

```bash
./MyApp -platform hyremote
```

See `docs/getting-started/qpa-proxy.md`. Wayland, EGLFS and other native delegates are not implied by the current xcb qualification.

## Viewer and security

Connect a standard VNC client to the loopback listener, normally `127.0.0.1:5900`, as described in `docs/viewer-connection.md`.

The current correctness baseline uses unauthenticated and unencrypted RFB SecurityType None. Do not expose it directly to untrusted networks. See `docs/security.md`.

## Support and physical-evidence boundary

Final Linux V1 acceptance must identify the exact Qt/compiler/native-QPA/viewer configuration and demonstrate the required local + remote behavior independently from Windows.

The physical local-display/local-input + remote envelope is tracked by #109. `docs/compatibility.md` remains Candidate until both the hosted/reference executable evidence and the required physical evidence actually run and are accepted.
