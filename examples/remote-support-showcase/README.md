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
- The local status panel shows stopped/running/faulted state, build/deployment identity, Qt/OS/architecture, active security profile, configured/effective listener, connected-client count, current policy and the latest product-level error. These facts come from the same public bounded diagnostic report rather than a Tool-owned model.
- The Connection Assistant lists current non-loopback IPv4 viewer endpoints separately from the listener bind address, exposes Waiting/Connected state, and can refresh/copy the current candidates. A wildcard bind such as `0.0.0.0:5921` is never presented as the remote host address to type into a viewer.
- The Diagnostics panel renders the same bounded, secret-safe public `diagnosticReport()` used by support/troubleshooting and can copy or save that exact report; HyRemoteTool does not invent a second diagnostic truth.
- The Activity panel keeps at most 100 in-memory entries for operator actions and public Runtime facts (start/stop/restart, state/listener, viewer connect/disconnect, remote-input policy and bounded product errors). It is an operational view, not a persistent session-audit database.
- Viewer disconnect/reconnect updates the client count without recreating the local application.
- The status panel shows whether HyRemoteTool is the active OS window. When remote input is enabled but the Tool is inactive, it displays the real application-scoped focus limitation from #362 and tells the local operator to reactivate this window; HyRemoteTool does not simulate desktop-wide focus stealing.
- The operator surface contains editable text and multiple controls so remote viewing/control is exercised against a meaningful application rather than a static capture target.
- The host console uses task-first navigation rather than a full-screen table of raw runtime facts. **Connection** is the opening page (service state, viewer status/count, selectable reachable IPv4 addresses and copy action); the **always-visible bottom operator dock** has explicit Start/Stop, opt-in remote control and an unencrypted/LAN safety reminder on every tab; **Controls** provides local/remote observable widgets including Notes; **Diagnostics** contains exact build/deployment identity, native platform, configured/effective listener, all addresses, errors and the original public export; **Activity** contains the bounded event log.
- Multiple NICs are **not** claimed equally reachable to every Viewer. Choose the host IP visible on the trusted target LAN; the Tool does not invent a "best" address. "Copy all addresses" and refresh remain available. The listener still binds to its configured address, not the selected display address.
- The endpoint picker explains empty, one-address, and multiple-address states. A unique address can be automatically selected; when a second address appears, that *automatic* selection is cleared and the operator must choose a LAN address. Only an explicit choice is preserved across multi-address refresh, while empty/unselected states disable single-address Copy. Offline UI tests use reserved TEST-NET examples; displayed addresses remain hints, not proof a Viewer can connect.
- The always-accessible Connection view prominently labels the unencrypted trusted-LAN security boundary without presenting a fake authentication or encryption control. The application-scoped focus warning appears when remote control is enabled but the Tool is not the active OS window.
- The native window remains bounded by the actual desktop geometry and navigates four pages on compact/high-DPI screens; the primary operator dock is vertically stacked so native Windows control widths cannot force an unusably wide window. The **Controls** page scrolls to a genuinely usable Notes editor even at 480×360; the other pages do not force users to read build identities before they can start or stop service. The Qt Widget changes are presentation-only: no extra Runtime, Viewer protocol, listener or remote-input semantics are introduced. Unchanged status text no longer triggers redundant QLabel writes on the 100ms state poll, avoiding unnecessary repaint work in the captured application.
- [#470](https://github.com/skawu/HyRemote/issues/470) is a separate product UX correction proposal on a feature branch. No automatic inclusion in a frozen release or physical Viewer acceptance is implied.

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
hyremote-tool --auto-start --diagnostic-report-file hyremote-diagnostic.txt --test-seconds 5
```

`--auto-start` is intended for deterministic product-fit automation. Normal interactive launches remain stopped until the local operator starts remote access. `--diagnostic-report-file` writes the same bounded public report as the GUI Save action after startup handling; it does not expose a second diagnostic surface.

The headless compact oracle also runs at 360×640 and 480×360 with `QT_SCALE_FACTOR=1.25` to exercise narrow and fractional-DPI Windows-like viewports. It cannot replace a real Windows monitor, Viewer, or DPI check.

The hosted product-fit uses a standard viewer to require the observable client-count lifecycle `0 -> 1 -> 0 -> 1 -> 0` across connection, disconnect and reconnect. Hosted offscreen execution does not substitute for the separate physical local-display/local-input coexistence gate.

## Agent-assisted physical pointer acceptance

For the maintained-Viewer #400 pointer gate, use `tests/physical_input_agent.py` with the exact candidate SHA and the executable built from that same clean checkout. The Agent verifies identity, launches this normal public-API HyRemoteTool application, observes its Qt event trace and writes the evidence bundle; the Human performs the requested RealVNC and local physical actions and confirms visible behavior.

See `docs/internal/q400-physical-input-acceptance.md` for the bounded procedure and scope. A generated Agent trace alone is not physical acceptance, and a remote transient popup visibility problem remains #404 rather than being folded into the #400 pointer ingress gate.
