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

- **Ctrl+I** toggles remote control (the "Allow remote control" checkbox) from anywhere in the window; the application reports the change as `REMOTE_INPUT enabled` / `REMOTE_INPUT disabled`.

- **`--toggle-input-at-ms <ms,ms,...>`** toggles remote control automatically at the given millisecond
  offsets from start (for example `--toggle-input-at-ms 8000,20000`). It drives the same checkbox as
  **Ctrl+I**, so scripted and headless runs exercise the same policy path as a person clicking it.
