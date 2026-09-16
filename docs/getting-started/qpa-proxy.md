# Getting started — Transparent QPA Proxy

This guide is for an existing Qt application that should gain HyRemote remote access with **zero or minimal application-source changes**.

Transparent QPA is the third mandatory HyRemote V1 integration mode. It is not a replacement-only/headless VNC platform: HyRemote delegates normal platform behavior to the native Qt platform integration and adds the shared HyRemote remote-access runtime alongside it.

For a complete ordinary application, see `examples/qpa-proxy-existing-app`.

## Supported reference line for V1 acceptance

The current QPA package is intentionally version-coupled to Qt private APIs:

- Qt: **6.8.3 exactly**;
- Windows x86_64 native delegate: `qwindows`;
- Linux x86_64 native delegate: `qxcb`.

The exact capture-family classification is maintained in `../qpa-capture-classification-qt-6.8.3.md`. The general evidence matrix is in `../compatibility.md`.

Do not infer support for another Qt patch/minor, Wayland, EGLFS, OpenHarmony or arbitrary native/foreign windows from this guide.

## Product defaults

Transparent QPA follows the same product policy as the other integration modes:

- the listener defaults to loopback;
- port defaults to 5900;
- remote input defaults to disabled;
- local display/input remains the native platform-delegate path;
- the current bounded RFB correctness baseline uses `SecurityType None` and therefore is not authenticated or encrypted.

Loopback and view-only defaults reduce accidental exposure; they are not substitutes for production authentication or encryption.

## Prerequisites

Use one coherent Qt 6.8.3 SDK for HyRemote and the target application. Building the proxy package requires Qt's private Gui development target (`Qt6::GuiPrivate`).

Linux additionally requires a working X11/XCB environment and the normal Qt xcb runtime dependencies because `qxcb` is the V1 reference delegate.

## 1. Build and install the HyRemote SDK with QPA

From a HyRemote source checkout:

```sh
cmake -S . -B build-qpa \
  -DCMAKE_INSTALL_PREFIX=<hyremote-prefix> \
  -DHYREMOTE_BUILD_CORE=ON \
  -DHYREMOTE_BUILD_REMOTE_ACCESS=ON \
  -DHYREMOTE_BUILD_WIDGETS_ADAPTER=ON \
  -DHYREMOTE_BUILD_QUICK_ADAPTER=ON \
  -DHYREMOTE_WITH_VNC=ON \
  -DHYREMOTE_WITH_QPA_PROXY=ON \
  -DHYREMOTE_BUILD_TESTS=OFF \
  -DHYREMOTE_BUILD_SPIKES=OFF
cmake --build build-qpa --config Release
cmake --install build-qpa --config Release
```

The installed SDK records that the QPA package is available and pins its exact Qt private-ABI version. Its qualified proxy module remains package-owned under the HyRemote prefix; application developers do not copy it into the Qt SDK.

## 2. Keep the application target ordinary Qt

Transparent QPA does not require application business/UI code to include or link HyRemote. A normal target remains ordinary Qt, for example:

```cmake
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)

add_executable(MyExistingApp
    main.cpp
)

target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)
```

Before adding the deployment step, run the application with the native platform and verify the local behavior you intend to preserve.

Windows:

```powershell
.\MyExistingApp.exe -platform windows
```

Linux/X11:

```sh
./MyExistingApp -platform xcb
```

Check local display, mouse, keyboard, text input, menus/dialogs and any application-specific rendering first.

## 3. Deploy through the single HyRemote helper

The normal product deployment path uses the same installed SDK entry point as other HyRemote modes:

```cmake
find_package(HyRemote CONFIG REQUIRED)

install(TARGETS MyExistingApp
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

hyremote_deploy(TARGET MyExistingApp QPA)
```

`find_package(HyRemote)` here is packaging/build integration. It does **not** add a HyRemote link dependency to the application target.

`hyremote_deploy(... QPA)` performs two bounded responsibilities:

1. Qt's normal application deployment remains responsible for the Qt runtime and native platform delegate (`qwindows` or `qxcb`);
2. HyRemote deploys the exact SDK-owned `qhyremote` module into the application's Qt `platforms` directory and asks Qt's deployment support to resolve dependencies of that additional module.

For an SDK built with shared HyRemote libraries, the helper also deploys the required HyRemote runtime libraries and submits them to Qt's dependency deployment path. It does not hard-code a Qt installation path, modify the user's Qt SDK, or require application code to know runtime filenames.

A QML application can compose both product concerns through the same helper:

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

This remains one deployment API. `QML` selects Qt's QML-aware high-level deployment path; `QPA` adds the HyRemote proxy payload afterwards.

## 4. Normal deployed launch — view-only safe default

After installing the application, launch the deployed executable directly with the HyRemote platform selection.

Windows:

```powershell
.\bin\MyExistingApp.exe -platform "hyremote:hyremote-port=5900"
```

Linux:

```sh
./bin/MyExistingApp -platform 'hyremote:hyremote-port=5900'
```

A correctly deployed application does **not** need `QT_PLUGIN_PATH` or `QT_QPA_PLATFORM_PLUGIN_PATH` merely to locate `qhyremote`.

Expected behavior:

- the application still uses the qualified native delegate for local display/input;
- supported application-owned surfaces are composed into one HyRemote remote session;
- the listener is loopback by default;
- remote input is disabled;
- supported dialogs/secondary windows may enter or leave the remote canvas without restarting the listener.

## 5. Connect and reconnect a viewer

Connect an RFB/VNC viewer to `127.0.0.1` TCP port `5900`.

Some viewers display TCP 5900 as VNC display `:0`; prefer explicit host/port syntax when available.

Close the viewer and connect again. A normal viewer disconnect must not require restarting the application.

The current transport correctness gate also requires held remote keys/buttons to be balanced on abrupt disconnect (#90); final QPA acceptance inherits that shared transport requirement rather than defining a separate QPA-specific rule.

## 6. Explicitly opt into remote control

For the current zero-code QPA contract, remote-input policy is explicit startup configuration:

```text
hyremote-input=true
```

Windows:

```powershell
.\bin\MyExistingApp.exe -platform "hyremote:hyremote-port=5900:hyremote-input=true"
```

Linux:

```sh
./bin/MyExistingApp -platform 'hyremote:hyremote-port=5900:hyremote-input=true'
```

Remote pointer/keyboard/text events then use HyRemote's normalized input path while native local input remains on the native QPA delegate.

To return to zero-code view-only mode, relaunch without `hyremote-input=true`.

There is intentionally no hidden example-only runtime toggle in Transparent QPA. An application that requires application-owned runtime policy controls should use a product API mode that exposes those controls rather than coupling application code to private QPA internals.

## 7. Optional address and port parameters

HyRemote-owned platform parameters are:

```text
hyremote-address=<numeric-ip-address>
hyremote-port=<1..65535>
hyremote-input=<0|1|false|true|off|on|no|yes>
```

Example:

```text
-platform "hyremote:hyremote-address=127.0.0.1:hyremote-port=5900:hyremote-input=false"
```

Invalid HyRemote values fail closed instead of silently changing product policy. Parameters not owned by HyRemote remain available for the qualified native delegate according to the implemented proxy contract.

## 8. Security warning

The current RFB correctness baseline advertises `SecurityType None`.

Therefore do **not**:

- bind directly to an Internet-facing interface;
- assume view-only authenticates a viewer;
- assume the VNC stream is encrypted;
- describe the current baseline as secure remote support.

Use loopback for local validation. If a non-loopback address is required for controlled testing, use a trusted isolated network and treat the traffic as unauthenticated/unencrypted.

## 9. Multi-window behavior

Transparent QPA represents one Qt application as one logical remote session, not one listener per window.

The production model tracks supported application-owned top-level surfaces and composes them into one logical canvas. Opening/closing/moving a supported dialog, tool window, QWidget popup/menu or second QQuickWindow must not by itself restart the listener.

Qt Quick content that remains inside one QQuickWindow scene remains in that window's normal scene capture and is not double-composed as a second surface.

Arbitrary foreign/native OS windows are outside the V1 contract.

## 10. Capture-family limits

Do not treat QWidget, QOpenGLWidget, QQuickWindow, Quick3D, custom FBO and arbitrary native windows as interchangeable.

The authoritative production-path matrix is `../qpa-capture-classification-qt-6.8.3.md`. Current boundaries include:

- QWidget uses production `QWidget::render()`;
- QQuickWindow uses public `contentItem()->grabToImage()`;
- Quick3D/custom Quick FBO evidence is backend-specific;
- mixed QQuickWidget whole-window composition remains unverified until the exact production parent `QWidget::render()` path is proven;
- generic `QWindow`, `QOpenGLWindow` and foreign native surfaces without a built-in adapter are unsupported rather than silently generalized.

## 11. Build-tree/debug plugin discovery

Manual plugin-path configuration is a development/debug tool, **not the normal installed deployment contract**.

For example, while running directly against a HyRemote build tree it can be useful to point Qt at the generated plugin root:

```text
QT_PLUGIN_PATH=<hyremote-build-plugin-root>
```

If a normal installed/deployed application requires that variable merely to locate `qhyremote`, treat it as a deployment defect under #94.

## 12. Troubleshooting

### Deployed application says the `hyremote` platform plugin is missing

Confirm the application was installed through `hyremote_deploy(TARGET ... QPA)` and its deployed Qt plugin tree contains:

```text
plugins/platforms/qhyremote.dll       # Windows
plugins/platforms/libqhyremote.so     # Linux
```

Do not mask a broken product deployment by permanently adding the SDK directory to `QT_PLUGIN_PATH`.

### `hyremote_deploy(... QPA)` rejects the configuration

The V1 QPA package is exact-version coupled. The SDK must contain Transparent QPA and the consumer must resolve **Qt 6.8.3 exactly**. Unsupported/missing combinations fail explicitly instead of silently loading a mismatched private ABI.

### Plugin is present but cannot load

Check that the application, proxy and Qt runtime all come from the same exact Qt 6.8.3 line. If HyRemote was built shared, verify the deployment tree contains the HyRemote runtime libraries installed by the helper.

### Windows native delegate fails

V1 expects Qt's normal `qwindows` platform plugin to remain available from Qt's application deployment.

### Linux native delegate fails

V1 expects `qxcb`. Confirm X11/XCB is available and Qt's xcb plugin/dependencies were deployed normally.

### Viewer connects but cannot control the application

That is the default view-only policy. Remote control requires an explicit `hyremote-input=true` launch.

### Viewer disconnects when a supported dialog opens/closes

That violates the one-session multi-surface continuity requirement. Reproduce on an exact supported environment and include the surface type and viewer behavior.

### A custom surface is blank or missing

Check the capture classification before treating it as a regression. V1 does not claim arbitrary native/custom-window capture.

## 13. Acceptance and evidence boundary

Repository implementation contains native-delegate preservation, one-session multi-surface composition, popup/Quick-window continuity, capture-family classification, deterministic deployment-helper tests and a clean installed-SDK QPA consumer.

The clean consumer is required to:

- link only its normal Qt application libraries;
- deploy through `hyremote_deploy(... QPA)`;
- contain `qhyremote` in the application `platforms` tree;
- clear plugin-path overrides;
- launch with `-platform hyremote`;
- establish and re-establish an RFB connection without application restart.

Exact Windows x86_64 and Linux x86_64 / Qt 6.8.3 jobs must actually execute before #94/#32 can claim the deployed path is supported. Current hosted jobs are affected by #74 before runner assignment; an unexecuted job is not passing evidence.

Separately, hosted/headless CI cannot prove that a physical local display and local input remain usable while a remote viewer is active. That final local-visible + remote coexistence evidence remains a mandatory #32 acceptance item and is the point at which a genuine local test environment may be required.
