# E4 — Transparent QPA Proxy for an existing Qt application

This example is intentionally an **ordinary Qt Widgets application**. Its source and CMake target do not include, link, or call any HyRemote application API.

The same executable is used in two modes:

1. native Qt platform only — normal local application;
2. HyRemote Transparent QPA Proxy — native local platform remains the delegate while HyRemote adds remote view/control.

This is the V1 E4 example required by #41 / #88.

## 1. What the application contains

The UI includes text input, a spin box, slider/progress state, checkbox, menu, operator notes and an independent diagnostics top-level window. These are ordinary Qt controls chosen so local and remote pointer/keyboard/text behavior and multi-window composition are observable.

There is deliberately no HyRemote status/control widget inside the application. Zero-code Transparent QPA mode is configured at process launch.

## 2. Reference compatibility boundary

The V1 Transparent QPA reference package is version-coupled to **Qt 6.8.3 exactly**:

- Windows x86_64 -> native `qwindows` delegate;
- Linux x86_64 -> native `qxcb` delegate.

Current production-path capture classification is documented in `docs/qpa-capture-classification-qt-6.8.3.md`.

Do not generalize this example to arbitrary Qt versions, Wayland, EGLFS, foreign/native windows or unsupported custom surfaces without separate compatibility evidence.

## 3. Build and install HyRemote QPA

Use the same Qt 6.8.3 SDK for HyRemote and the application. The QPA package requires the Qt private Gui development target (`Qt6::GuiPrivate`) because QPA has no binary/source compatibility guarantee.

From the HyRemote repository root:

```sh
cmake -S . -B build-qpa \
  -DHYREMOTE_WITH_QPA_PROXY=ON \
  -DHYREMOTE_BUILD_TESTS=OFF \
  -DHYREMOTE_BUILD_SPIKES=OFF \
  -DCMAKE_INSTALL_PREFIX=<hyremote-prefix>
cmake --build build-qpa --config Release
cmake --install build-qpa --config Release
```

The installed platform plugin is under:

```text
<hyremote-prefix>/<libdir>/HyRemote/plugins/platforms/
```

Qt must receive the parent plugin root (`.../HyRemote/plugins`) in its plugin search path.

## 4. Build the ordinary application

No HyRemote package is required by this example's CMake project:

```sh
cmake -S examples/qpa-proxy-existing-app -B build-qpa-example
cmake --build build-qpa-example --config Release
```

Inspect `CMakeLists.txt`: it finds and links only `Qt6::Widgets`.

## 5. Run normally first

Verify the application behaves as a normal local Qt application before enabling HyRemote.

Windows:

```powershell
.\build-qpa-example\Release\hyremote-qpa-proxy-existing-app.exe -platform windows
```

Linux/X11:

```sh
./build-qpa-example/hyremote-qpa-proxy-existing-app -platform xcb
```

Open the diagnostics window, edit text and operate controls locally. HyRemote is not involved in this run.

## 6. Enable Transparent QPA — safe view-only default

Expose the installed HyRemote plugin root to Qt.

Windows PowerShell:

```powershell
$env:QT_PLUGIN_PATH="<hyremote-prefix>\lib\HyRemote\plugins"
.\build-qpa-example\Release\hyremote-qpa-proxy-existing-app.exe -platform "hyremote:hyremote-port=5900"
```

Linux:

```sh
export QT_PLUGIN_PATH="<hyremote-prefix>/lib/HyRemote/plugins"
./build-qpa-example/hyremote-qpa-proxy-existing-app -platform 'hyremote:hyremote-port=5900'
```

Important defaults:

- listener address: loopback only;
- port: 5900 unless changed explicitly;
- remote input: **disabled**;
- native local display/input remains the authoritative local path.

The application source is unchanged. The `hyremote` platform plugin delegates normal platform behavior to `qwindows` or `qxcb` and composes the shared HyRemote runtime alongside it.

## 7. Connect and reconnect a viewer

Use a standard RFB/VNC viewer and connect to **127.0.0.1 TCP port 5900**. Some viewer UIs express port 5900 as display `:0`; use the viewer's explicit host/port syntax when available.

Expected behavior in view-only mode:

- the native application remains visible and locally interactive;
- the same supported application content is visible remotely;
- opening/closing the diagnostics window does not require viewer reconnect;
- the remote viewer cannot inject input;
- closing the viewer and reconnecting later does not require restarting the Qt application.

Hosted/headless tests can validate session continuity, but they are not proof that a physical monitor remained visible. That final evidence belongs to #32.

## 8. Explicitly enable remote control

Remote input is deliberately opt-in. For the current zero-code QPA contract it is a **startup policy**, not a runtime application API.

Windows:

```powershell
.\build-qpa-example\Release\hyremote-qpa-proxy-existing-app.exe -platform "hyremote:hyremote-port=5900:hyremote-input=true"
```

Linux:

```sh
./build-qpa-example/hyremote-qpa-proxy-existing-app -platform 'hyremote:hyremote-port=5900:hyremote-input=true'
```

Reconnect the viewer and verify pointer, keyboard and text input. Native/local input must continue to work because Transparent QPA does not replace the native delegate path.

To return to view-only, restart without `hyremote-input=true`. This example does **not** invent a hidden runtime toggle to make zero-code mode look more capable than the implemented product contract.

Applications that need runtime policy controls should use a product API mode where such application-owned policy is explicitly exposed, rather than adding backend-specific control code to this example.

## 9. Listener address

The default loopback listener is intentional. An explicit address may be provided as a platform parameter:

```text
hyremote-address=<ip-address>
```

For example, the full parameter form is conceptually:

```text
-platform "hyremote:hyremote-address=127.0.0.1:hyremote-port=5900:hyremote-input=false"
```

Do not expose the current V1 correctness baseline to an untrusted network merely to make the demo reachable from another machine.

## 10. Security boundary

The current baseline RFB transport uses **SecurityType None**. It provides no production authentication or encryption.

Therefore:

- loopback is the example default;
- remote input is off by default;
- do not expose the listener directly to the Internet;
- do not treat `hyremote-input=false` as an authentication mechanism;
- use only a controlled/trusted test network if a non-loopback address is explicitly required.

See the repository security documentation before any deployment decision.

## 11. Common failures

### `Could not find the Qt platform plugin "hyremote"`

Verify `QT_PLUGIN_PATH` points to the directory that contains the `platforms` subdirectory, not to the plugin file itself.

Expected layout:

```text
<plugin-root>/platforms/qhyremote.dll       # Windows
<plugin-root>/platforms/libqhyremote.so     # Linux
```

### QPA package refuses to configure/build

Transparent QPA is intentionally exact-version coupled. Verify Qt **6.8.3** and the private Gui development package/target are available.

### Linux launch cannot initialize the native delegate

The V1 Linux reference delegate is `qxcb`. A usable X11/XCB environment and the normal Qt xcb platform dependencies are required. Do not substitute another delegate and call it supported without evidence.

### Viewer connects but input does nothing

That is the expected safe default. Relaunch with `hyremote-input=true` only when remote control is explicitly desired.

### A custom/native surface is missing

V1 QPA support is adapter-scoped, not an arbitrary desktop/window-server capture claim. Check `docs/qpa-capture-classification-qt-6.8.3.md` and `docs/compatibility.md` before filing a capture regression.

## 12. Evidence status

Repository implementation and E2E tests for QPA multi-surface continuity exist, but the exact Windows/Linux hosted matrix is currently blocked before runner assignment by #74. Until those jobs execute successfully, documentation must not convert candidate rows into `Supported` claims.

Physical local-visible + remote coexistence remains a final #32 acceptance item.
