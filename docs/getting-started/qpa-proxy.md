# Getting started — Transparent QPA Proxy

This guide is for an existing Qt application that should gain HyRemote remote access with **zero or minimal application-source changes**.

Transparent QPA is the third mandatory HyRemote V1 integration mode. It is different from replacing the application's platform with a headless-only VNC backend: HyRemote delegates normal platform behavior to the native Qt platform integration and adds the shared HyRemote remote-access runtime alongside it.

For a complete ordinary application to try, see `examples/qpa-proxy-existing-app`.

## Supported reference line for V1 acceptance

The current package is intentionally version-coupled to Qt private QPA APIs:

- Qt: **6.8.3 exactly**;
- Windows x86_64 native delegate: `qwindows`;
- Linux x86_64 native delegate: `qxcb`.

The exact capture-family classification is maintained in `../qpa-capture-classification-qt-6.8.3.md`. The general evidence matrix is in `../compatibility.md`.

Do not infer support for another Qt patch/minor, Wayland, EGLFS, OpenHarmony or arbitrary native windows from this guide.

## Product defaults

Transparent QPA follows the same safe product policy as the other integration modes:

- constructing/loading the package is not meant to expose a public network listener by application code;
- when QPA remote access starts for a supported application surface, the listener defaults to loopback;
- port defaults to 5900;
- remote input defaults to disabled;
- local/native display and input remain the platform-delegate path;
- the current baseline RFB security type is `None`, so there is no production authentication or encryption.

The last point is important: loopback and view-only defaults reduce accidental exposure, but they are not substitutes for authentication or encryption.

## Prerequisites

Use one coherent Qt 6.8.3 SDK for HyRemote and the target application. Building the QPA plugin requires Qt's private Gui development target (`Qt6::GuiPrivate`).

Linux additionally requires a working X11/XCB environment and the usual Qt xcb runtime dependencies because `qxcb` is the V1 reference delegate.

## Build and install the QPA package

From a HyRemote source checkout:

```sh
cmake -S . -B build-qpa \
  -DHYREMOTE_WITH_QPA_PROXY=ON \
  -DHYREMOTE_BUILD_TESTS=OFF \
  -DHYREMOTE_BUILD_SPIKES=OFF \
  -DCMAKE_INSTALL_PREFIX=<hyremote-prefix>
cmake --build build-qpa --config Release
cmake --install build-qpa --config Release
```

The plugin installs under:

```text
<hyremote-prefix>/<libdir>/HyRemote/plugins/platforms/
```

Qt plugin discovery must receive the parent `.../HyRemote/plugins` directory.

If HyRemote was built with shared libraries instead of the default build's static library behavior, also make the installed HyRemote runtime libraries discoverable through the normal operating-system loader path. Do not copy arbitrary DLL/SO files beside the application without recording the deployed package layout.

## Step 1 — prove the application still works natively

Run the existing application with the normal native platform before introducing HyRemote.

Windows:

```powershell
.\myapp.exe -platform windows
```

Linux/X11:

```sh
./myapp -platform xcb
```

Verify local display, mouse, keyboard, text input, dialogs and any application-specific rendering you intend to share.

## Step 2 — make the HyRemote plugin discoverable

Windows PowerShell:

```powershell
$env:QT_PLUGIN_PATH="<hyremote-prefix>\lib\HyRemote\plugins"
```

Linux:

```sh
export QT_PLUGIN_PATH="<hyremote-prefix>/lib/HyRemote/plugins"
```

`QT_PLUGIN_PATH` points to the directory **containing** the `platforms` subdirectory.

Expected default install shape:

```text
<plugin-root>/platforms/qhyremote.dll       # Windows
<plugin-root>/platforms/libqhyremote.so     # Linux
```

## Step 3 — launch in view-only mode

The minimum launch is:

```text
-platform hyremote
```

An explicit port is often easier while testing:

Windows:

```powershell
.\myapp.exe -platform "hyremote:hyremote-port=5900"
```

Linux:

```sh
./myapp -platform 'hyremote:hyremote-port=5900'
```

Equivalent environment selection is possible with `QT_QPA_PLATFORM`, for example:

```text
QT_QPA_PLATFORM=hyremote:hyremote-port=5900
```

Do not set both command-line and environment platform selection to contradictory values.

Expected behavior:

- the native application window still exists through `qwindows` or `qxcb`;
- supported application-owned surfaces are composed into one HyRemote remote session;
- the remote listener is loopback by default;
- remote input is disabled;
- dialogs/secondary supported windows may enter or leave the remote application canvas without restarting the listener.

## Step 4 — connect a viewer

Connect an RFB/VNC viewer to `127.0.0.1` TCP port `5900`.

Some viewers display TCP 5900 as VNC display `:0`; prefer an explicit host/port field or viewer-specific explicit-port syntax so there is no ambiguity.

Close the viewer and connect again. A normal viewer disconnect must not require restarting the application.

## Step 5 — explicitly opt into remote control

For the current zero-code QPA contract, remote-input policy is a startup configuration parameter:

```text
hyremote-input=true
```

Windows:

```powershell
.\myapp.exe -platform "hyremote:hyremote-port=5900:hyremote-input=true"
```

Linux:

```sh
./myapp -platform 'hyremote:hyremote-port=5900:hyremote-input=true'
```

Remote pointer/keyboard/text events are then routed through HyRemote's normalized input path to the selected application surface. Native local input still follows the native QPA delegate.

To return to zero-code view-only mode, relaunch without `hyremote-input=true`.

There is intentionally no hidden example-only runtime toggle in Transparent QPA. If an application needs application-owned runtime policy controls, use an integration mode/API that exposes that policy explicitly, or track a dedicated future QPA control mechanism rather than coupling application code to private platform internals.

## Optional address and port parameters

Current HyRemote-owned platform parameters are:

```text
hyremote-address=<numeric-ip-address>
hyremote-port=<1..65535>
hyremote-input=<0|1|false|true|off|on|no|yes>
```

Example with all values explicit:

```text
-platform "hyremote:hyremote-address=127.0.0.1:hyremote-port=5900:hyremote-input=false"
```

Invalid HyRemote parameters fail closed with a diagnostic instead of being silently forwarded to the native delegate.

Parameters not owned by HyRemote remain available for native delegate handling according to the qualified QPA implementation.

## Security warning

The current correctness baseline advertises RFB `SecurityType None`.

Therefore do **not**:

- bind directly to an Internet-facing interface;
- assume view-only mode authenticates a viewer;
- assume the VNC stream is encrypted;
- document the current baseline as a secure remote-support deployment.

Use loopback for local validation. If a non-loopback address is required for controlled testing, use a trusted isolated network and treat the traffic as unauthenticated/unencrypted.

## Multi-window behavior

Transparent QPA represents the Qt application as one logical remote session, not one listener per window.

The current model tracks supported application-owned top-level surfaces and composes them into one logical application canvas. Opening/closing/moving a supported dialog, tool window, QWidget popup/menu or second QQuickWindow must not by itself restart the RFB listener.

Qt Quick content that remains inside one QQuickWindow scene stays in that window's normal scene capture and is not double-composed as a second surface.

Arbitrary foreign/native OS windows are not part of the V1 contract.

## Capture-family limits

Do not treat `QWidget`, `QOpenGLWidget`, `QQuickWindow`, Quick3D, custom FBO and arbitrary native windows as interchangeable.

The authoritative production-path matrix is `../qpa-capture-classification-qt-6.8.3.md`. Current key boundaries include:

- QWidget uses production `QWidget::render()`;
- QQuickWindow uses public `contentItem()->grabToImage()`;
- Quick3D/custom Quick FBO evidence is backend-specific;
- mixed QQuickWidget whole-window composition remains unverified until the exact production parent-`QWidget::render()` path is proven;
- generic `QWindow`/`QOpenGLWindow`/foreign native surfaces without a built-in adapter are unsupported rather than silently captured.

## Troubleshooting

### Qt says the `hyremote` platform plugin is missing

Check the plugin root and install tree. `QT_PLUGIN_PATH` must point to a directory containing `platforms/qhyremote.dll` or `platforms/libqhyremote.so`.

### Plugin is found but cannot load

Check that the application, plugin and Qt private ABI all come from the exact Qt 6.8.3 package/build. QPA private ABI mismatch is not a supported configuration.

If HyRemote was built shared, also check operating-system runtime-library search paths.

### Windows native delegate fails

V1 expects the normal Qt `qwindows` platform plugin to remain available from the Qt installation/deployment.

### Linux native delegate fails

V1 expects `qxcb`. Confirm X11/XCB is actually available and Qt's xcb platform plugin/dependencies are deployed.

### Viewer connects but cannot control the application

That is the default view-only policy. Remote control requires an explicit `hyremote-input=true` launch.

### Viewer disconnects when a supported dialog opens/closes

That violates the QPA multi-surface continuity goal tracked by #76. Reproduce on an exact supported environment and include the surface type and viewer behavior.

### A custom surface is blank or missing

Check the capture classification before treating it as a regression. V1 does not claim arbitrary native/custom-window capture.

## Acceptance/evidence boundary

Repository implementation and dedicated E2E tests exist for native delegate preservation, one-session multi-surface composition, popup/Quick window churn and production capture classification.

At the time of this guide, exact Windows/Linux hosted jobs are blocked before runner assignment by #74. Those unexecuted jobs are not counted as passing evidence.

Separately, hosted/headless CI is not proof that a physical local display and local input remained usable while a remote viewer was active. Final physical local-visible + remote coexistence evidence remains part of #32 acceptance.
