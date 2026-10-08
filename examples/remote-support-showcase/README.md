# HyRemoteTool

HyRemoteTool is HyRemote's first-party host-side product experience and validation application. The source directory remains `examples/remote-support-showcase` as an implementation detail, but the installed/runnable product name is **HyRemoteTool**. It uses only the public `HyRemote::RemoteAccess` facade.

## Product behavior

- The local Qt Widgets application starts normally and remains the authoritative UI.
- Remote access is **off initially**. The local operator must press **Start remote access**.
- The listener uses HyRemote's LAN-capable default (`0.0.0.0`) and the effective port is displayed locally.
- Listener/runtime state and viewer connection state are distinct: **Running does not mean a viewer is connected**. The local status panel displays the backend-neutral `connectedClientCount()` from the public facade.
- Remote input is disabled by default, so the initial policy is view-only.
- The local operator can explicitly enable or disable remote control. With the current public facade, a policy change while running is applied by stopping and restarting the same `RemoteAccess` instance; the example does not create a second Session or transport stack.
- Stop releases the listener while the local application continues running.
- The local status panel shows stopped/running/faulted state, endpoint, connected-client count, current policy and the latest product-level error.
- The Diagnostics panel renders the same bounded, secret-safe public `diagnosticReport()` used by support/troubleshooting and can copy it to the clipboard; HyRemoteTool does not invent a second diagnostic truth.
- Viewer disconnect/reconnect updates the client count without recreating the local application.
- The status panel shows whether HyRemoteTool is the active OS window. When remote input is enabled but the Tool is inactive, it displays the real application-scoped focus limitation from #362 and tells the local operator to reactivate this window; HyRemoteTool does not simulate desktop-wide focus stealing.
- The operator surface contains editable text and multiple controls so remote viewing/control is exercised against a meaningful application rather than a static capture target.

## Security boundary

The default `Insecure` profile is unauthenticated and unencrypted. Keep that mode on a trusted LAN or another explicitly protected network path. `Authenticated` is a conditional installed capability; `AuthenticatedEncrypted` fails closed while an encrypted transport backend is unavailable. HyRemoteTool must not imply stronger security than the active Runtime policy actually provides.

The safe startup policy is therefore two-dimensional: the service is explicitly started, and remote control remains independently opt-in. A connected-client count is operational diagnostics only; it is not an authenticated identity count.

## Run

Build the normal examples graph with `HYREMOTE_BUILD_EXAMPLES=ON`, then run:

```text
hyremote-tool
```

Optional acceptance helpers are explicit and do not change normal safe defaults:

```text
hyremote-tool --auto-start --remote-input --port 5901 --test-seconds 30
```

`--auto-start` is intended for deterministic product-fit automation. Normal interactive launches remain stopped until the local operator starts remote access.

The hosted product-fit uses a standard viewer to require the observable client-count lifecycle `0 -> 1 -> 0 -> 1 -> 0` across connection, disconnect and reconnect. Hosted offscreen execution does not substitute for the separate physical local-display/local-input coexistence gate.

## Agent-assisted physical pointer acceptance

For the maintained-Viewer #400 pointer gate, use `tests/physical_input_agent.py` with the exact candidate SHA and the executable built from that same clean checkout. The Agent verifies identity, launches this normal public-API HyRemoteTool application, observes its Qt event trace and writes the evidence bundle; the Human performs the requested RealVNC and local physical actions and confirms visible behavior.

See `docs/internal/q400-physical-input-acceptance.md` for the bounded procedure and scope. A generated Agent trace alone is not physical acceptance, and a remote transient popup visibility problem remains #404 rather than being folded into the #400 pointer ingress gate.
