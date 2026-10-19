# HyRemoteTool

HyRemoteTool is HyRemote's **out-of-process companion client**. It runs outside the business application, connects to a remote HyRemote-enabled application by explicit IP and port, and provides access-plane viewing, input and telemetry. It does not host remote access, does not share its own window and does not link the HyRemote Runtime.

The source directory remains `examples/remote-support-showcase` as an implementation detail; the installed executable is `hyremote-tool`.

## V0.3.0.0 scope

- connect to `<remote-ip>:<port>` — the operator supplies the endpoint; there is no discovery;
- RFB 3.8 / `SecurityType None`;
- framebuffer view with aspect-preserving window scaling;
- view-only by default; pointer, wheel and keyboard are sent only when **Remote control** is checked;
- frame-rate (FPS), throughput (Mbit/s), frame count, byte count and resolution telemetry;
- explicit Connecting / Connected / Disconnected / Failed state and last error;
- disconnect and reconnect without restarting the tool.

## Not supported in V0.3.0.0 (stated, not faked)

- management-plane actions (remote start/stop, policy or host-password changes);
- VNC-auth connection passwords;
- clipboard, fullscreen, multi-monitor, TLS/VeNCrypt and file transfer.

The UI has no disabled placeholder buttons for those features. They are planned for later companion-tool phases; see [`docs/internal/hyremote-tool-plan.md`](../../docs/internal/hyremote-tool-plan.md).

## Run

Build with examples enabled, then:

```text
hyremote-tool
```

Enter the remote application's IP and port and press **Connect**. A HyRemote-enabled host example can be started with:

```text
hyremote-learning-01-widgets --port 5931 --remote-input
```

Optional helpers:

```text
hyremote-tool --connect 192.168.1.6:5921
hyremote-tool --verify-ui
```

`--verify-ui` constructs the client window offscreen and does not connect or open a listener. `--test-seconds N` bounds a run for automation.

## Security boundary

The V0.3.0.0 client accepts only unauthenticated `SecurityType None` endpoints. Use it on a trusted LAN or another explicitly protected path; it is not Internet-safe. Do not treat the client as a replacement for TLS, authentication or access control.
