# HyRemote QML Basic

`qml-basic` is the minimum **Declarative QML API** example for HyRemote V1. It uses the same shared `HyRemote::RemoteAccess` runtime as the C++ examples; QML is a thin integration layer, not a second capture/transport/input stack.

## Minimal use

The normal application surface is intentionally small:

```qml
import HyRemote

RemoteAccess {
    target: window
    enabled: true
}
```

`enabled: true` is safe during QML construction: the wrapper records the declarative request and starts the shared runtime only after `componentComplete()`, when initial target/configuration bindings are available. Construction itself remains inert.

Defaults remain identical to C++:

- loopback listener (`127.0.0.1`);
- port `5900`;
- remote input disabled;
- no listener until `enabled` is explicitly requested.

## Build

Build the current V1 tree with the QML API and examples enabled:

```sh
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<toolchain> \
  -DHYREMOTE_BUILD_QML_API=ON \
  -DHYREMOTE_BUILD_EXAMPLES=ON
cmake --build build --parallel
```

The V1 reference matrix is Qt 6.8.3 on Windows x86_64 and Linux x86_64. Repository implementation does not by itself upgrade either row to Supported.

## Run view-only

Launch `hyremote-qml-basic` with its default acceptance policy. A VNC/RFB viewer can connect to the configured loopback port and view the Quick scene, while remote input remains blocked.

The local UI displays the actual runtime state and `connectedClientCount`; `Running` is not treated as equivalent to “viewer connected.”

## Run with explicit control

The example's acceptance launcher can explicitly enable the shared `remoteInputEnabled` policy for trusted control testing. This is opt-in and preserves the same view/control separation as the C++ facade.

QML does not expose RFB/backend-specific types or create a second input path. Pointer, key and committed-text delivery is performed by the same normalized runtime/Quick adapter path used by Embedded C++.

## Reconnect and stop

A viewer can disconnect and reconnect while the application remains running. The product-fit gate requires the client-count lifecycle `0 -> 1 -> 0 -> 1 -> 0` and verifies declarative stop/listener release.

Abrupt viewer loss also uses the shared transport's held-input balancing behavior; it is not implemented separately in QML.

## Deployment

For an installed SDK, the application imports the normal `HyRemote` QML module and uses the same single deployment helper:

```cmake
find_package(HyRemote CONFIG REQUIRED)

install(TARGETS MyQmlApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

hyremote_deploy(TARGET MyQmlApp QML)
```

The helper bridges the installed HyRemote QML import root into Qt's normal deployment scanner and carries the shared `HyRemoteRemoteAccess` runtime. The application does not link a `HyRemote::Qml` C++ target or manually copy `qmldir`/plugin files.

## Security

The current bounded RFB correctness baseline uses **SecurityType None**: no transport authentication and no transport encryption. Keep the default loopback/trusted boundary; do not expose it directly to an untrusted network or the public Internet.

See `docs/reference/security.md`.

## Evidence boundary

The repository contains product-fit coverage for view/input/text/reconnect/client-count behavior and clean installed-QML deployment. Windows/Linux hosted acceptance remains pending because #74 prevents runner assignment. Physical local-visible/local-input coexistence is separately tracked by #109 and cannot be inferred from offscreen execution.

Related guides:

- `docs/getting-started/qml.md`
- `docs/reference/qml-consumption.md`
- `docs/deployment.md`
- `docs/viewer-connection.md`
- `docs/reference/compatibility.md`
- `docs/reference/known-limitations.md`
