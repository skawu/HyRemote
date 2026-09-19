# Getting started — Transparent QPA Proxy

This guide is for an existing Qt application that should gain HyRemote remote access with **zero or minimal application-source changes**.

Transparent QPA is the third mandatory HyRemote V1 integration mode. It is not a replacement-only/headless VNC platform: HyRemote delegates normal platform behavior to the native Qt platform integration and adds the same shared remote-access runtime alongside it.

For a complete ordinary application, see `examples/qpa-proxy-existing-app`.

## V1 qualified reference line

Transparent QPA is deliberately version-coupled to Qt private APIs:

- Qt: **6.8.3 exactly**;
- Windows x86_64 delegate: `qwindows`;
- Linux x86_64 delegate: `qxcb`.

The capture-family classification is maintained in `../qpa-capture-classification-qt-6.8.3.md`; the general evidence matrix is `../compatibility.md`.

Do not infer support for another Qt patch/minor, Wayland, EGLFS, OpenHarmony or arbitrary foreign/native windows.

## Safe defaults

- listener address: loopback;
- port: 5921;
- remote input: disabled;
- local display/input: native platform delegate remains authoritative;
- current bounded RFB correctness baseline: `SecurityType None`, therefore unauthenticated and unencrypted.

## 1. Build and install HyRemote with QPA

Use one coherent Qt 6.8.3 SDK containing the private Gui development target. The normal HyRemote product options already build the shared C++ runtime and its Widgets/Quick adapters; enabling QPA requires one additional product option:

```sh
cmake -S . -B build-qpa \
  -DCMAKE_PREFIX_PATH=<qt-6.8.3-prefix> \
  -DCMAKE_INSTALL_PREFIX=<hyremote-prefix> \
  -DHYREMOTE_WITH_QPA_PROXY=ON
cmake --build build-qpa --config Release
cmake --install build-qpa --config Release
```

The resulting V1 package has one fixed HyRemote runtime shape for this mode:

- `qhyremote` — Qt platform MODULE;
- `HyRemoteRemoteAccess` — shared HyRemote runtime used internally by the module.

Core, Session, capture, input and transport objects are not separate QPA deployment choices.

## 2. Keep the application ordinary Qt

The application does not include or link HyRemote. For example:

```cmake
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)

add_executable(MyExistingApp main.cpp)
target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)
```

Before enabling the proxy, verify the application's native behavior with `-platform windows` or `-platform xcb` as appropriate.

## 3. Deploy through the one HyRemote helper

```cmake
find_package(HyRemote CONFIG REQUIRED)

install(TARGETS MyExistingApp
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

hyremote_deploy(TARGET MyExistingApp QPA)
```

`find_package(HyRemote)` here supplies packaging metadata; it does **not** add a HyRemote link dependency to `MyExistingApp`.

Deployment responsibilities remain simple:

1. Qt deploys the application runtime and native `qwindows` / `qxcb` delegate;
2. HyRemote adds `qhyremote` and the shared `HyRemoteRemoteAccess` runtime required by it.

The helper uses Qt's deployment support to resolve runtime dependencies. It does not modify the user's Qt SDK, hard-code its installation path, or require the application to know HyRemote runtime filenames.

A QML application can use the same helper:

```cmake
hyremote_deploy(TARGET MyQmlApp QML QPA)
```

## 4. Launch with the safe view-only default

Windows:

```powershell
.\bin\MyExistingApp.exe -platform hyremote
```

Linux:

```sh
./bin/MyExistingApp -platform hyremote
```

No HyRemote-specific environment variable is required for a correctly deployed application. In particular, the normal deployed path must not require `QT_PLUGIN_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH`, or an SDK-specific runtime library path.

Expected behavior:

- the native delegate still owns local display/input;
- supported application surfaces form one remote session;
- listener remains loopback:5921 by default;
- remote input remains disabled;
- supported secondary windows/dialogs may enter and leave the remote canvas without restarting the listener.

## 5. Viewer connect/reconnect

Connect an RFB/VNC viewer to `127.0.0.1:5921`. Close it and connect again. Normal viewer disconnect/reconnect must not require an application restart.

The shared transport correctness gate also requires held remote keys/buttons to be balanced after abrupt disconnect.

## 6. Explicitly enable remote control

Remote input is startup policy in the zero-code QPA mode:

```text
-platform "hyremote:hyremote-input=true"
```

Local input remains on the native platform path. Omit the parameter to return to view-only behavior.

There is intentionally no private QPA control object for application code. Applications needing runtime policy controls should use the public C++ or QML product API rather than private platform-plugin internals.

## 7. Optional address and port parameters

```text
hyremote-address=<numeric-ip-address>
hyremote-port=<1..65535>
hyremote-input=<0|1|false|true|off|on|no|yes>
```

Example:

```text
-platform "hyremote:hyremote-address=127.0.0.1:hyremote-port=5921:hyremote-input=false"
```

Invalid HyRemote values fail closed.

## 8. Security boundary

The current RFB baseline advertises `SecurityType None`. Do not bind the current V1 correctness baseline directly to an untrusted network or the public Internet and do not describe it as authenticated/encrypted remote support.

## 9. Multi-window behavior

Transparent QPA represents one Qt application as one logical remote session, not one listener per window. Supported application-owned top-level surfaces are composed into that session; opening, closing or moving a supported dialog, tool window, QWidget popup/menu or second QQuickWindow must not itself restart the listener.

Qt Quick content inside one QQuickWindow remains part of that window's scene rather than becoming a duplicate remote surface. Arbitrary foreign/native OS windows are outside V1 scope.

## 10. Capture-family limits

Do not generalize evidence between QWidget, QOpenGLWidget, QQuickWindow, Quick3D, custom FBO and arbitrary native windows. The authoritative production classification is `../qpa-capture-classification-qt-6.8.3.md`.

Current boundaries include:

- QWidget correctness capture uses `QWidget::render()`;
- QQuickWindow uses public `contentItem()->grabToImage()`;
- Quick3D/custom Quick FBO evidence is backend-specific;
- mixed QQuickWidget composition remains configuration-specific;
- generic QWindow/QOpenGLWindow/foreign-native surfaces without a qualified adapter are not silently claimed.

## 11. Build-tree diagnostics only

Manual `QT_PLUGIN_PATH=<hyremote-build-plugin-root>` may be useful when running directly from a development build tree. It is **not** part of the installed product contract. If a deployed application requires it to locate `qhyremote`, treat that as a deployment defect.

## 12. Troubleshooting

### Platform plugin missing

Confirm deployment used `hyremote_deploy(TARGET ... QPA)` and the installed application contains:

```text
plugins/platforms/qhyremote.dll       # Windows
plugins/platforms/libqhyremote.so     # Linux
```

Do not permanently point `QT_PLUGIN_PATH` at the SDK to hide a packaging defect.

### Deployment helper rejects Qt

QPA requires the exact qualified Qt 6.8.3 line. A missing QPA package or different consumer Qt version fails explicitly.

### Plugin present but cannot load

Verify the deployed tree contains `HyRemoteRemoteAccess` and that the application/proxy/native Qt runtime come from the same qualified Qt line. The user should not need to copy Core or backend libraries.

### Viewer connects but cannot control

That is the default. Add `hyremote-input=true` only when remote control is deliberately required.

### Supported window changes disconnect the viewer

That violates the one-session surface-continuity requirement and should be treated as a QPA regression.

## 13. Acceptance boundary

The clean installed-SDK QPA consumer must prove that an ordinary Qt-only executable can be deployed with the helper, find `qhyremote` from its own application tree, load the shared HyRemote runtime, connect/reconnect an RFB viewer, and do so without SDK/plugin-path overrides.

Exact Windows x86_64 and Linux x86_64 / Qt 6.8.3 jobs must actually execute before QPA can be called accepted. Current hosted jobs are blocked by #74 before runner assignment; an unexecuted job is not passing evidence.

Separately, hosted/headless CI cannot prove physical native local-display/local-input + remote coexistence. That remains a mandatory #32/#109 acceptance item and is the point at which a genuine local test environment may be required.
