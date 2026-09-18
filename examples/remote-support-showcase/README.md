# HyRemote remote support showcase

This example demonstrates the V1.0 production-facing remote-support workflow through the public `HyRemote::RemoteAccess` facade only.

## Product behavior

- The local Qt Widgets application starts normally and remains the authoritative UI.
- Remote access is **off initially**. The local operator must press **Start remote access**.
- The listener uses HyRemote's loopback-safe default and the configured port is displayed locally.
- Listener/runtime state and viewer connection state are distinct: **Running does not mean a viewer is connected**. The local status panel displays the backend-neutral `connectedClientCount()` from the public facade.
- Remote input is disabled by default, so the initial policy is view-only.
- The local operator can explicitly enable or disable remote control. With the current public facade, a policy change while running is applied by stopping and restarting the same `RemoteAccess` instance; the example does not create a second Session or transport stack.
- Stop releases the listener while the local application continues running.
- The local status panel shows stopped/running/faulted state, endpoint, connected-client count, current policy and the latest product-level error.
- Viewer disconnect/reconnect updates the client count without recreating the local application.
- The operator surface contains editable text and multiple controls so remote viewing/control is exercised against a meaningful application rather than a static capture target.

## Security boundary

The current bounded RFB correctness baseline uses `SecurityType None`. This example therefore does **not** present the connection as authenticated or encrypted. Keep the listener on loopback or another explicitly trusted/protected network path until production authentication/encryption capabilities exist.

The safe startup policy is therefore two-dimensional: the service is explicitly started, and remote control remains independently opt-in. A connected-client count is operational diagnostics only; it is not an authenticated identity count.

## Run

Build the normal examples graph with `HYREMOTE_BUILD_EXAMPLES=ON`, then run:

```text
hyremote-remote-support-showcase
```

Optional acceptance helpers are explicit and do not change normal safe defaults:

```text
hyremote-remote-support-showcase --auto-start --remote-input --port 5901 --test-seconds 30
```

`--auto-start` is intended for deterministic product-fit automation. Normal interactive launches remain stopped until the local operator starts remote access.

The hosted product-fit uses a standard viewer to require the observable client-count lifecycle `0 -> 1 -> 0 -> 1 -> 0` across connection, disconnect and reconnect. Hosted offscreen execution does not substitute for the final physical local-display/local-input coexistence evidence required by the V1 acceptance gate.

## Build

```text
cmake -S . -B build/ga -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<QtRoot> \
  -DHYREMOTE_BUILD_TESTS=ON -DHYREMOTE_BUILD_EXAMPLES=ON
cmake --build build/ga
```

The showcase links `HyRemote::RemoteAccess` and is part of the default examples graph (`HYREMOTE_BUILD_EXAMPLES`),
like the other Embedded C++ examples.

## Run on Windows and Linux

The same product path on both: the local window stays visible and interactive, a viewer connects to the selected
port, and remote input follows the explicit policy described above.

```text
# Windows (Developer PowerShell) - the runtime DLL directories must be on PATH
$env:PATH = "<QtRoot>\bin;<build>\remoteaccess;<build>;" + $env:PATH
<build>\examples\remote-support-showcase\hyremote-remote-support-showcase.exe

# Linux - the build tree carries the runtime RPATH, so no path overrides are needed
<build>/examples/remote-support-showcase/hyremote-remote-support-showcase
```

## Deployment

```text
cmake --install <build> --prefix <install-prefix>
```

A consumer links `HyRemote::RemoteAccess` and deploys through the SDK helper `hyremote_deploy(...)`, which owns the
shared runtime and its private Qt runtime closure. Details: `docs/deployment.md`, `docs/sdk-installation.md`.

## Troubleshooting

| Symptom | Cause / action |
| --- | --- |
| A viewer connects but cannot control anything | Expected by default: the example starts **view-only**. Enable control explicitly with `--remote-input` or through the operator policy. |
| The application exits immediately on Windows with no output | Add Qt's `bin` and the build's `remoteaccess` directory to `PATH`. |
| `--auto-start` runs but the service is unreachable from another machine | The listener binds loopback by default; an externally reachable address is an explicit policy decision, not a default. |
| The local window is unaffected while a viewer is connected | That is the documented local + remote coexistence model. |
| The listener never starts | The port is in use; choose another `--port`. |

More: `docs/troubleshooting.md`, `docs/known-limitations.md`.
