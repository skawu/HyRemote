# HyRemote Widgets Basic

`widgets-basic` is the minimum **Embedded C++ API + Qt Widgets** product example for HyRemote V1.

The application uses only the public product facade:

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&window);
remote.start();
```

It does not construct or reference Core `Session`, capture, input, transport, RFB, or backend objects.

## Safety defaults

Normal launch keeps the product defaults:

- listener address: loopback (`127.0.0.1`);
- port: `5900` unless changed with `--port`;
- remote viewing: available after the explicit `remote.start()` in the example;
- remote input: **disabled by default**;
- construction of `RemoteAccess` alone never opens a listener.

The current baseline RFB transport uses **SecurityType None**. Treat it as a correctness/trusted-loopback baseline, not Internet-safe authentication or encryption. Do not expose the listener to an untrusted network.

## Build

From a HyRemote source tree with the matching Qt 6.8.3 Widgets kit available:

```sh
cmake -S . -B build \
  -DHYREMOTE_BUILD_EXAMPLES=ON
cmake --build build --parallel
```

The V1 reference/acceptance line is exact Qt **6.8.3** on Windows x86_64 and Linux x86_64. Do not infer support for another Qt/OS combination from source compatibility alone.

## Run view-only

Run the built `hyremote-widgets-basic` executable normally. With no extra option the listener is loopback-only and remote input stays disabled.

Optional port override:

```sh
hyremote-widgets-basic --port 5901
```

Connect a standard RFB/VNC viewer to:

```text
127.0.0.1:5901
```

You should be able to view the application. Pointer, keyboard, wheel, and text input received from the remote viewer must not be injected while the example is in its default view-only policy.

## Run with explicit remote control

For a trusted local test, relaunch with:

```sh
hyremote-widgets-basic --port 5901 --remote-input
```

The example contains a button and a text field so pointer, keyboard, and text delivery can be checked against normal Qt Widgets behavior.

`--remote-input` is deliberately explicit; it is not the product default.

## Stopped-runtime policy transition

The V1 application contract does not require an application restart to change C++ view/control policy. The application keeps running while the HyRemote runtime follows:

```cpp
remote.stop();
remote.setRemoteInputEnabled(true);
remote.start();
```

Repository product-fit exercises that exact public-API sequence in the same `widgets-basic` process. The acceptance-only helper below starts from the normal view-only default; the first viewer disconnect triggers the transition, while the timer is only a watchdog fallback:

```sh
hyremote-widgets-basic --port 5901 --policy-transition-ms 15000 --test-seconds 24
```

The helper adds no alternate runtime or private control path. Normal users do not need it; it exists so automated and later physical acceptance can prove the frozen stopped-runtime policy contract using the real E1 application.

## Reconnect

Disconnect the viewer while the application remains running, then connect again to the same address/port. A normal disconnect must not require restarting the target application or recreating `RemoteAccess`.

The V1 product-fit additionally proves a viewer can reconnect after the same-process view-only -> control `stop -> configure -> start` transition. Final `RemoteAccess::stop()` must release the listener.

## Input and geometry evidence

The current V1 candidate implements and tests:

- left/middle/right pointer buttons;
- pointer coordinates through the remote framebuffer viewport;
- vertical wheel delivery;
- key press/release;
- Shift/Ctrl/Alt modifier propagation;
- text commit as a separate semantic path from key delivery;
- view-only input rejection;
- same-process stopped-runtime transition from view-only to explicit control;
- disconnect/reconnect before/after that transition;
- stop/listener release;
- balancing releases when a viewer disappears with recognized keys/buttons still held.

Precise DPR/edge-coordinate normalization is additionally covered by deterministic Core tests. Dual-OS acceptance remains pending until the reference jobs actually execute; #74 no-runner failures are not pass evidence.

## Local + remote coexistence boundary

The application is a normal visible Qt Widgets window and HyRemote's Embedded C++ design is additive to that local UI. However, hosted offscreen CI is **not** evidence that a physical monitor and local keyboard/mouse remained usable while a real remote viewer was attached.

That physical local-visible/local-input coexistence check is tracked by #109 and remains a final #30/#33 acceptance item. The physical run reuses this same application/public facade lifecycle; it must not introduce a test-only product API.

## Connected-viewer status

Lifecycle `Running` means the remote runtime/listener is running; it does **not** mean that a viewer is connected. The current V1 candidate exposes backend-neutral `RemoteAccess::connectedClientCount()` and this example displays the real client count rather than deriving a fake connected state from lifecycle status.

The connect/disconnect/reconnect count transitions are part of the product-fit gate, which now executes in the reference jobs on both Windows and Linux. Executing a gate is not a support claim: the compatibility rows stay `Candidate` until the physical local-display/local-input evidence required by the V1 acceptance gate exists.

## Deployment

This is a normal Qt Widgets application that links the installed SDK, so it deploys like any other application:

```text
cmake --install build/ga --prefix <install-prefix>
```

A consumer that links `HyRemote::RemoteAccess` deploys through the SDK's own helper, which owns the shared runtime
and its private Qt runtime closure; do not assemble Qt libraries or plug-ins by hand:

```cmake
find_package(HyRemote REQUIRED)
hyremote_deploy(TARGETS hyremote-widgets-basic ...)
```

What a deployment must contain, and how to run the deployed application from a clean directory with the path
overrides removed, is in `docs/deployment.md` and `docs/sdk-installation.md`.

## Troubleshooting

| Symptom | Cause / action |
| --- | --- |
| The window opens but a viewer cannot type or click | Expected by default: the example starts **view-only**. Pass `--remote-input`. |
| `RemoteAccessState::Running` but no viewer appears connected | Lifecycle `Running` means the listener runs. Read `connectedClientCount()`, which is what this example displays, instead of deriving a connected state from lifecycle status. |
| The application exits immediately on Windows with no output | The shared runtime DLLs are not on `PATH`: add Qt's `bin`, the build's `remoteaccess` directory (and `qml/HyRemote` for the QML modes). |
| The listener never starts | The port is already in use; pass another `--port`. The default bind is loopback-only. |
| A viewer connects but no picture appears | The viewer must speak the RFB 3.8 baseline without authentication, which is the current V1 transport; see `docs/viewer-connection.md`. |
| `libpng warning: iCCP: known incorrect sRGB profile` on startup | Benign: it comes from the example's bundled PNG asset. |

More: `docs/troubleshooting.md`, `docs/known-limitations.md`.

## Related documentation

Use the V1 user guides under `docs/getting-started/`, `docs/viewer-connection.md`, `docs/security.md`, `docs/compatibility.md`, and `docs/known-limitations.md`. Support claims remain evidence-driven.
