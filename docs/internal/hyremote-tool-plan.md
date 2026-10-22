> **Current product direction, 2026-10-22 — D 综合工作台 (Product Owner selected):** The full forward-looking feature list, page flows, UI state machine, SDK-assistant boundaries, diagnostics model, portable-delivery investigation, secure-management deferral and phased acceptance now live in [HyRemoteTool 综合工作台 RFC](hyremote-tool-workbench-prd-2026-10-22.md) (tracking #579). This document remains the **historical V0.3.0.0 out-of-process RFB access-client contract** and records that release's limits; it is not sufficient as the integrated workbench roadmap. D1–D5 open details in the RFC require further Product Owner decisions before implementation claims. Nothing here retroactively changes the V0.3.0.0 release or authorizes host management over RFB.

# HyRemoteTool companion plan

Status: active companion-tool plan for the V0.3 line. Product authority: #378. V0.3.0.0 release authority: #234.

## Position

HyRemoteTool is an **out-of-process companion client** shipped with HyRemote. It runs outside the business application, connects to a remote HyRemote-enabled application by an explicit IP and port, and provides access-plane observation and debugging. It is not a host-side control panel, not a business demo, and not a second Runtime.

The tool must not define or duplicate Runtime semantics. It consumes the existing RFB access plane; any host-management action requires a separately defined HyRemote management capability.

## Two planes, two ports

| Plane | Purpose | Port policy | V0.3.0.0 |
|---|---|---|---|
| Access | RFB 3.8 framebuffer and input | Per application; configurable port, address and interface | Shipped |
| Management | Start/stop access, policy, password, host diagnostics | Separate listener; per application; configurable port; **default disabled**, loopback by default, authenticated | Not shipped |

Rules:

- The two planes are never multiplexed on the RFB port.
- The access port remains application-owned and configurable (`0.0.0.0:5921` default).
- The management port is configurable per application. A fixed global management port is only defensible for a future per-host daemon, not for the embedded per-application model.
- A management listener that can start a stopped access listener must be independent of `start()/stop()` and must have its own authentication and threat model.
- The Tool must not guess or scan ports. The operator supplies access and (later) management endpoints explicitly.

## V0.3.0.0: access-plane client

The V0.3.0.0 tool is a pure RFB client. It does not link `HyRemote::RemoteAccess` and does not host remote access.

| Capability | Behaviour |
|---|---|
| Remote endpoint | Operator enters IPv4/hostname and port; default port 5921; no discovery |
| Connect / disconnect | Explicit connect, disconnect and reconnect; no listener is opened locally |
| Handshake | RFB 3.8; SecurityType None only in this version |
| Framebuffer | Raw and DesktopSize; window scaling preserves aspect ratio |
| Input | View-only by default; pointer, wheel and keyboard only when remote control is enabled |
| Frame-rate telemetry | Framebuffer updates per second, measured client-side |
| Throughput telemetry | bytes/s, Mbit/s, cumulative frames and bytes |
| Connection state | Connecting / Connected / Disconnected / Failed plus the last error |
| Diagnostics | Handshake/security/encoding facts, disconnect reason and a bounded client log |

### Not supported in V0.3.0.0 (stated, not faked)

- VNC-auth connection passwords; the default access plane is `SecurityType None`.
- Management-plane actions: remote start/stop, policy changes, host password changes.
- Clipboard synchronization, fullscreen mode, multi-monitor selection, TLS/VeNCrypt and file transfer.

The UI must not show disabled placeholder buttons for these. The README and release notes state the limitation.

## Later phases

### V0.3.x: management plane

Prerequisites:

1. A HyRemote management API with an explicit threat model.
2. A separate authenticated listener, disabled by default, loopback by default, with an explicit LAN opt-in.
3. Configuration for management port and credentials; no fixed global port.

Tool changes:

- Optional `Management IP:Port` and credential fields, separate from the access connection.
- Actions: start/stop access, remote-input policy, access password, bounded host diagnostics.
- If no management endpoint is configured, management controls are absent; they are not silently emulated over RFB.

### V0.4: private-viewer parity

Clipboard, scaling/fullscreen, multi-monitor selection, TLS/VeNCrypt and other viewer capabilities are activated only when the corresponding HyRemote capability is qualified.

## Testing and evidence

- Headless unit tests with a fake RFB server: handshake, None security, Raw/DesktopSize, pointer/key encoding, FPS/byte accounting, error paths.
- Integration test against a real HyRemote host example: first frame, input, telemetry, disconnect/reconnect.
- Installed-artifact physical acceptance with a maintained viewer and a real remote application.
- The old host-side tool tests (compact layout, listener editor, host diagnostic export, host product-fit) are replaced, not retained as evidence for the client.

## Release integration

- The tool is built from the exact accepted tag and installed under `bin/`.
- A Tool implementation change changes the candidate SHA: the release branch must be re-frozen and the same-SHA L2 re-run before any tag.
- The tool is part of V0.3.0.0's companion surface, but a Tool change never authorizes a release by itself.
- The tool is **not one of the release-trains required-evidence cells**. Its own client core/UI tests and the installed-client smoke are companion evidence, not a substitute for the four-frontend SDK/deploy evidence.
