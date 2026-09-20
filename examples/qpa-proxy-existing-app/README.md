# E4 — Transparent QPA Proxy for an existing Qt application

This example is intentionally an **ordinary Qt Widgets application**. Its C++ source does not include, call or link any HyRemote application API.

The same application is used in two modes:

1. native Qt platform only — normal local application;
2. HyRemote Transparent QPA Proxy — native local platform remains the delegate while HyRemote adds remote view/control.

This is the V1 E4 example required by #41/#32. Packaging uses the same public `hyremote_deploy()` entry point as the other HyRemote modes.

## 1. Application boundary

The UI contains text input, a spin box, slider/progress state, checkbox, menu, operator notes and an independent diagnostics top-level window. These are ordinary Qt controls chosen so local and remote input plus multi-window behavior are observable.

There is deliberately no HyRemote status/control widget or backend selector inside the application source. Transparent QPA policy is process-launch configuration.

The executable target always links **only `Qt6::Widgets`**. The optional HyRemote CMake package integration below is packaging metadata only; it never adds `HyRemote::RemoteAccess`, Core, transport or QPA-private types to application code or link libraries.

## 2. Reference compatibility boundary

The V1 Transparent QPA reference package is version-coupled to **Qt 6.8.3 exactly**:

- Windows x86_64 -> native `qwindows` delegate;
- Linux x86_64 -> native `qxcb` delegate.

Current production-path capture classification is documented in `docs/internal/qpa-capture-classification-qt-6.8.3.md`.

Do not generalize this example to arbitrary Qt versions, Wayland, EGLFS, foreign/native windows or unsupported custom surfaces without separate compatibility evidence.

## 3. Build and install HyRemote with the QPA payload

Use the same Qt 6.8.3 SDK for HyRemote and the application. The only QPA-specific product option is:

```sh
cmake -S . -B build-qpa \
  -DCMAKE_PREFIX_PATH=<Qt-6.8.3-prefix> \
  -DCMAKE_INSTALL_PREFIX=<hyremote-prefix> \
  -DHYREMOTE_WITH_QPA_PROXY=ON
cmake --build build-qpa --config Release
cmake --install build-qpa --config Release
```

The normal V1 build supplies the shared `HyRemoteRemoteAccess` runtime and its internal adapters/transport. Users do not select Core, capture, input or RFB implementation targets to enable QPA.

The SDK keeps its qualified proxy module under its own prefix. Normal application deployment does **not** require copying that file by hand or modifying the Qt SDK.

## 4. Prove the application is ordinary Qt first

The default example configuration requires only Qt:

```sh
cmake -S examples/qpa-proxy-existing-app -B build-qpa-example \
  -DCMAKE_PREFIX_PATH=<Qt-6.8.3-prefix>
cmake --build build-qpa-example --config Release
```

`HYREMOTE_EXAMPLE_DEPLOY_QPA` defaults to `OFF`. In this mode the example CMake project does not find HyRemote at all, and the target links only `Qt6::Widgets`.

Run it natively first.

Windows:

```powershell
.\build-qpa-example\Release\hyremote-qpa-proxy-existing-app.exe -platform windows
```

Linux/X11:

```sh
./build-qpa-example/hyremote-qpa-proxy-existing-app -platform xcb
```

Verify local display, mouse, keyboard, text input, menus and the diagnostics window before introducing HyRemote.

## 5. Produce a normal deployed QPA application

Configure the same source with the installed HyRemote prefix and the explicit packaging option:

```sh
cmake -S examples/qpa-proxy-existing-app -B build-qpa-deployed \
  -DCMAKE_PREFIX_PATH="<Qt-6.8.3-prefix>;<hyremote-prefix>" \
  -DCMAKE_INSTALL_PREFIX=<application-prefix> \
  -DHYREMOTE_EXAMPLE_DEPLOY_QPA=ON
cmake --build build-qpa-deployed --config Release
cmake --install build-qpa-deployed --config Release
```

The entire HyRemote packaging integration is:

```cmake
find_package(HyRemote CONFIG REQUIRED)
install(TARGETS hyremote-qpa-proxy-existing-app ...)
hyremote_deploy(TARGET hyremote-qpa-proxy-existing-app QPA)
```

The installed package does **not** expose a `HyRemote::QpaPlatform` application target. `hyremote_deploy(... QPA)` resolves the package-owned `qhyremote` payload internally, preserves Qt's normal native application deployment and carries the same shared `HyRemoteRemoteAccess` runtime.

The deployed application must therefore **not require `QT_PLUGIN_PATH` or `QT_QPA_PLATFORM_PLUGIN_PATH`** for normal use.

## 6. Launch deployed view-only mode

The shortest normal launch is:

```text
hyremote-qpa-proxy-existing-app -platform hyremote
```

Safe defaults are:

- listener address: loopback only;
- port: 5921;
- remote input: **disabled**;
- native local display/input remains the authoritative local path.

The application source is unchanged. The `hyremote` platform plugin delegates normal platform behavior to `qwindows` or `qxcb` and composes the shared HyRemote runtime alongside it.

## 7. Connect and reconnect a viewer

Connect a standard RFB/VNC viewer to **127.0.0.1 TCP port 5921**. Some viewer UIs express port 5921 as display `:21`; use explicit host/port syntax when available.

Expected behavior in view-only mode:

- the native application remains visible and locally interactive;
- supported application content is visible remotely;
- opening/closing supported secondary windows does not restart the listener;
- remote input is rejected by policy;
- disconnect/reconnect does not require application restart.

Hosted/headless tests can validate deployment and session continuity, but they are not proof that a physical monitor remained visible. Physical local-display/local-input coexistence is tracked by #109 and remains a final #32 acceptance item.

## 8. Explicitly enable remote control

For the zero-code V1 QPA contract, remote-input policy is an explicit **startup policy**:

```text
hyremote-qpa-proxy-existing-app -platform "hyremote:hyremote-input=true"
```

Reconnect the viewer and verify pointer, keyboard and text input. Native/local input must continue through the native delegate.

To return to view-only, relaunch without `hyremote-input=true`. E4 deliberately does not invent a hidden runtime control channel. Applications that require application-owned live policy controls should use the C++ or QML integration mode.

## 9. Optional policy parameters and security boundary

The defaults require no parameters. Optional QPA policy parameters are:

```text
hyremote-address=<numeric-ip-address>
hyremote-port=<1..65535>
hyremote-input=<0|1|false|true|off|on|no|yes>
```

The current RFB correctness baseline uses **SecurityType None**. It provides no transport authentication or encryption. Do not expose the listener directly to the Internet or treat view-only policy as authentication.

## 10. Build-tree debugging only

Manual plugin-path overrides are **not** the installed product workflow. A maintainer may use `QT_PLUGIN_PATH` while debugging the plugin directly from a build tree, but a deployed application that needs that override is considered broken deployment.

## 11. Common failures

### Deployed application cannot find `hyremote`

Confirm the application was installed using `hyremote_deploy(TARGET ... QPA)` and that its deployed Qt plugin tree contains the platform module (`qhyremote.dll` on Windows or `libqhyremote.so` on Linux). Do not fix normal deployment by permanently adding the HyRemote SDK to `QT_PLUGIN_PATH`.

### `hyremote_deploy(... QPA)` rejects the SDK or Qt version

The V1 proxy is exact-version coupled. The installed SDK must contain the QPA payload and the consumer must resolve **Qt 6.8.3 exactly**. Unsupported combinations fail explicitly instead of silently loading a mismatched private ABI.

### Linux launch cannot initialize the native delegate

The V1 Linux reference delegate is `qxcb`. A usable X11/XCB environment and Qt's normal xcb dependencies are required.

### Viewer connects but input does nothing

That is the safe default. Relaunch with `hyremote-input=true` only when remote control is explicitly desired.

### A custom/native surface is missing

V1 QPA support is adapter-scoped, not arbitrary desktop/window-server capture. Check `docs/internal/qpa-capture-classification-qt-6.8.3.md` and `docs/compatibility.md` before treating it as a regression.

## 12. Evidence status

The current #106 candidate contains deterministic deployment-helper tests, source-target and installed-payload resolution tests, and a clean installed-SDK Qt-only QPA consumer. The clean consumer must deploy the application, remove plugin/runtime path overrides, launch `-platform hyremote`, and establish/re-establish an RFB connection on both exact reference operating systems.

#74 currently blocks GitHub-hosted jobs before runner assignment. Until those Windows/Linux jobs actually execute and pass, this is implemented/candidate evidence rather than a `Supported` release claim.

Physical local-visible + remote coexistence remains #109/#32 and is not replaced by hosted execution.
