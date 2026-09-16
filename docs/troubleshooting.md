# Troubleshooting

This guide covers the V0.0.1 Embedded C++ path. Start with the product-level error/behavior before investigating internal implementation layers.

## `find_package(HyRemote)` cannot find the package

`find_package(HyRemote CONFIG REQUIRED)` is for an installed HyRemote prefix. Add that prefix to `CMAKE_PREFIX_PATH` or use the source-consumption path from `docs/source-consumption.md`.

Do not combine `add_subdirectory(HyRemote)` with an assumption that an installed `HyRemoteConfig.cmake` has been generated.

## `RemoteAccess::start()` returns false

Check `lastError()` and verify:

- the target is a live supported `QWidget` or `QQuickWindow`;
- configuration was completed while stopped;
- the selected port is valid and not already occupied;
- the target adapter required by the built SDK is present;
- the internal VNC transport was not explicitly disabled at build time.

The public error surface intentionally does not expose RFB backend types.

## Viewer cannot connect

Verify that:

- `start()` actually succeeded;
- the viewer uses the configured loopback address/port;
- another process is not occupying the port;
- the process is still running;
- a firewall/security product is not interfering with the intended local/trusted connection.

Construction alone never opens the listener.

## Viewer sees the application but input does nothing

Remote input is disabled by default. Enable it explicitly before `start()` only for an authorized control session.

If only specific keys/compositions fail, consult `docs/input-model.md` and `docs/known-limitations.md`; the first milestone does not claim full IME/dead-key parity.

## Pointer coordinates are wrong

Record target logical size, captured framebuffer size, device-pixel ratio and resize state. Coordinate mapping is based on the remote frame/target geometry; stale or unsupported geometry changes must not be papered over as viewer behavior.

## Quick capture is blank or stops while hidden

The public async Quick correctness path depends on a capturable Qt Quick scene/window. Hidden/minimized/occluded behavior has explicit limitations and must be checked against `docs/compatibility.md`; it is not equivalent to a compositor-level desktop capture service.

## Qt-linked tests fail to launch on Windows

Ensure the matching Qt `bin` directory is on `PATH`. A missing Qt runtime can produce a process launch/DLL-not-found failure before CTest reaches application assertions.

## Linux test works under offscreen/Xvfb but local desktop behavior is unknown

Headless CI proves only the behavior it actually executes. It does not upgrade local-visible coexistence or a desktop QPA/graphics combination to Supported. Record those separately in the compatibility matrix.

## Slow or malicious clients

The Core frame mailbox, RFB frame handoff, GUI input delivery and incomplete-handshake lifetime are bounded by design. If memory/work still grows without bound, treat it as a product defect and record the exact client traffic/reproduction; do not raise queue capacities as a substitute for fixing ownership/backpressure.
