# E4 — Transparent QPA Proxy for an existing Qt application

This example is intentionally an **ordinary Qt Widgets application**. Its C++ source does not include, call or link any HyRemote application API.

The same application is used in two modes:

1. native Qt platform only — normal local application;
2. HyRemote Transparent QPA Proxy — native local platform remains the delegate while HyRemote adds remote view/control.

This is the V1 E4 example required by #41 / #88. Installed deployment is owned by #94 through the same public `hyremote_deploy()` entry point used by the other HyRemote product modes.

## 1. Application boundary

The UI contains text input, a spin box, slider/progress state, checkbox, menu, operator notes and an independent diagnostics top-level window. These are ordinary Qt controls chosen so local and remote input plus multi-window behavior are observable.

There is deliberately no HyRemote status/control widget or backend selector inside the application source. Transparent QPA policy is process-launch configuration.

The executable target always links **only `Qt6::Widgets`**. The optional HyRemote CMake package integration described below is packaging/deployment metadata only; it never adds `HyRemote::RemoteAccess`, Core, transport or QPA-private types to application code or link libraries.

## 2. Reference compatibility boundary

The V1 Transparent QPA reference package is version-coupled to **Qt 6.8.3 exactly**:

- Windows x86_64 -> native `qwindows` delegate;
- Linux x86_64 -> native `qxcb` delegate.

Current production-path capture classification is documented in `docs/qpa-capture-classification-qt-6.8.3.md`.

Do not generalize this example to arbitrary Qt versions, Wayland, EGLFS, foreign/native windows or unsupported custom surfaces without separate compatibility evidence.

## 3. Build and install the HyRemote SDK with QPA

Use the same Qt 6.8.3 SDK for HyRemote and the application. Building the QPA package requires `Qt6::GuiPrivate` because Qt does not provide QPA source/binary compatibility.

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

The SDK keeps its qualified proxy module under its own prefix. Normal application deployment does **not** require copying that file by hand or modifying the Qt SDK.

## 4. Prove the application is ordinary Qt first

The default example configuration requires only Qt:

```sh
cmake -S examples/qpa-proxy-existing-app -B build-qpa-example
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

For the product deployment path, configure the same source with the installed HyRemote prefix and the explicit packaging option:

```sh
cmake -S examples/qpa-proxy-existing-app -B build-qpa-deployed \
  -DCMAKE_PREFIX_PATH="<Qt-6.8.3-prefix>;<hyremote-prefix>" \
  -DCMAKE_INSTALL_PREFIX=<application-prefix> \
  -DHYREMOTE_EXAMPLE_DEPLOY_QPA=ON
cmake --build build-qpa-deployed --config Release
cmake --install build-qpa-deployed --config Release
```

The optional block in the example CMake file is intentionally small:

```cmake
find_package(HyRemote CONFIG REQUIRED)
install(TARGETS hyremote-qpa-proxy-existing-app ...)
hyremote_deploy(TARGET hyremote-qpa-proxy-existing-app QPA)
```

`hyremote_deploy(... QPA)` keeps Qt's normal application deployment path for the native runtime/delegate, then deploys the exact SDK-owned `qhyremote` module into the application's Qt `platforms` tree and resolves dependencies of that additional module through Qt's deployment support.

The deployed application must therefore **not require `QT_PLUGIN_PATH` or `QT_QPA_PLATFORM_PLUGIN_PATH`** for normal use.

## 6. Launch deployed view-only mode

From the deployed application tree, launch with the HyRemote platform selection only.

Windows:

```powershell
.\bin\hyremote-qpa-proxy-existing-app.exe -platform "hyremote:hyremote-port=5900"
```

Linux:

```sh
./bin/hyremote-qpa-proxy-existing-app -platform 'hyremote:hyremote-port=5900'
```

Safe defaults remain:

- listener address: loopback only;
- port: 5900 unless changed explicitly;
- remote input: **disabled**;
- native local display/input remains the authoritative local path.

The application source is unchanged. The `hyremote` platform plugin delegates normal platform behavior to `qwindows` or `qxcb` and composes the shared HyRemote runtime alongside it.

## 7. Connect and reconnect a viewer

Connect a standard RFB/VNC viewer to **127.0.0.1 TCP port 5900**. Some viewer UIs express port 5900 as display `:0`; use explicit host/port syntax when available.

Expected behavior in view-only mode:

- the native application remains visible and locally interactive;
- supported application content is visible remotely;
- opening/closing supported secondary windows does not restart the listener;
- remote input is rejected by policy;
- disconnect/reconnect does not require application restart.

Hosted/headless tests can validate deployment and session continuity, but they are not proof that a physical monitor remained visible. Physical local-display/local-input coexistence is still a final #32 acceptance item.

## 8. Explicitly enable remote control

For the current zero-code QPA contract, remote-input policy is an explicit **startup policy**:

Windows:

```powershell
.\bin\hyremote-qpa-proxy-existing-app.exe -platform "hyremote:hyremote-port=5900:hyremote-input=true"
```

Linux:

```sh
./bin/hyremote-qpa-proxy-existing-app -platform 'hyremote:hyremote-port=5900:hyremote-input=true'
```

Reconnect the viewer and verify pointer, keyboard and text input. Native/local input must continue through the native delegate.

To return to view-only, relaunch without `hyremote-input=true`. E4 deliberately does not invent a hidden runtime control channel. Applications that require application-owned runtime policy controls should use a product API mode that exposes those controls.

## 9. Address and security boundary

The default loopback listener is intentional. Explicit QPA parameters are:

```text
hyremote-address=<numeric-ip-address>
hyremote-port=<1..65535>
hyremote-input=<0|1|false|true|off|on|no|yes>
```

The current RFB correctness baseline uses **SecurityType None**. It provides no production authentication or encryption. Do not expose the listener directly to the Internet or treat view-only policy as authentication.

## 10. Build-tree/debug plugin discovery

Manual plugin-path overrides are **not the normal installed deployment workflow**. They may still be useful while developing the QPA plugin directly from a build tree:

```text
QT_PLUGIN_PATH=<hyremote-build-or-sdk-plugin-root>
```

If a released/deployed application requires that environment variable merely to locate `qhyremote`, treat that as a deployment defect under #94 rather than documenting it as the product workflow.

## 11. Common failures

### Deployed application cannot find `hyremote`

Confirm the application was installed using `hyremote_deploy(TARGET ... QPA)` and that its deployed Qt plugin tree contains:

```text
plugins/platforms/qhyremote.dll       # Windows
plugins/platforms/libqhyremote.so     # Linux
```

Do not fix a broken normal deployment by permanently adding the SDK directory to `QT_PLUGIN_PATH`.

### `hyremote_deploy(... QPA)` rejects the SDK or Qt version

The V1 proxy is exact-version coupled. The installed SDK must contain QPA and the consumer must resolve **Qt 6.8.3 exactly**. Unsupported combinations fail explicitly instead of silently loading a mismatched private ABI.

### Linux launch cannot initialize the native delegate

The V1 Linux reference delegate is `qxcb`. A usable X11/XCB environment and Qt's normal xcb dependencies are required.

### Viewer connects but input does nothing

That is the safe default. Relaunch with `hyremote-input=true` only when remote control is explicitly desired.

### A custom/native surface is missing

V1 QPA support is adapter-scoped, not arbitrary desktop/window-server capture. Check `docs/qpa-capture-classification-qt-6.8.3.md` and `docs/compatibility.md` before treating it as a regression.

## 12. Evidence status

#94 adds deterministic deployment-helper tests plus a clean installed-SDK Qt-only consumer. That consumer must install the application, remove `QT_PLUGIN_PATH` overrides, launch `-platform hyremote` and establish/re-establish an RFB connection on both exact reference operating systems.

The repository currently has #74 blocking GitHub-hosted jobs before runner assignment. Until the Windows/Linux jobs actually execute and pass, the deployment path remains implemented/candidate evidence rather than a `Supported` release claim.

Physical local-visible + remote coexistence remains a separate final #32 acceptance requirement.
