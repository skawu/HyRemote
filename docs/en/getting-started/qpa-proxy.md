# QPA Getting Started (Preview)

> Language / 语言: **English** | [中文](../../getting-started/qpa-proxy.md)

QPA is HyRemote's specialized zero-code integration path. Use it when the application genuinely needs a Qt platform-entry route. If Generic Plugin is sufficient, prefer [`generic.md`](generic.md) because Generic uses public Qt plugin APIs and does not introduce QPA private-ABI coupling.

Current product status: **Preview**.

## Product model

QPA does not switch the application to a replacement-only offscreen/qvnc platform. `qhyremote` uses a Factory Trampoline to create and return the native Qt platform integration while attaching the HyRemote Shared Runtime alongside it:

```text
MyApp -platform hyremote
        |
        v
qhyremote Factory Trampoline
        |
        +--> native qwindows / qxcb platform integration
        |
        +--> HyRemote Shared Runtime
```

Local display and local input remain owned by the native Qt platform.

## Current reference environment

QPA depends on Qt private ABI, so compatibility is declared per **exact Qt patch**.

Current reference pairs:

- Windows x86_64 + Qt 6.8.3 exact + `qwindows`;
- Linux x86_64 + Qt 6.8.3 exact + `qxcb`.

Do not infer support for another Qt patch, Wayland, EGLFS, macOS, or another native platform from these pairs.

> **TODO V0.3/V0.4:** complete formal productization, additional exact Qt/OS combinations, and physical local+remote coexistence qualification before broadening the QPA support statement.

See [`../../compatibility.md`](../../compatibility.md).

## Keep the application Qt-only

The application itself does not include or link HyRemote:

```cmake
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets)

add_executable(MyExistingApp main.cpp)
target_link_libraries(MyExistingApp PRIVATE Qt6::Widgets)
```

HyRemote enters through deployment and process startup instead of application linkage.

## Deployment

```cmake
find_package(HyRemote CONFIG REQUIRED)

install(TARGETS MyExistingApp
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

hyremote_deploy(TARGET MyExistingApp QPA)
```

The deployment helper adds:

- the `qhyremote` platform plugin;
- the Shared Runtime;
- the matching native Qt platform plugin;
- required Qt runtime dependencies.

The application executable still does not link `HyRemote::RemoteAccess`.

A normal deployment should not depend on SDK-specific `QT_PLUGIN_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH`, or build-tree library paths.

See [`../guide/deployment.md`](../guide/deployment.md).

## Launch

Windows:

```powershell
.\bin\MyExistingApp.exe -platform hyremote
```

Linux:

```sh
./bin/MyExistingApp -platform hyremote
```

Default behavior:

- the native platform continues to own local display and input;
- listener defaults to `0.0.0.0:5921`;
- remote input is disabled by default;
- supported application top-level surfaces participate in one logical remote session;
- ordinary viewer disconnect/reconnect does not require restarting the Qt application.

## Enable remote control

Remote input is startup policy for the QPA route:

```text
-platform "hyremote:hyremote-input=true"
```

Omit the parameter to stay view-only.

Applications that need rich runtime policy changes should prefer the public C++ API or QML API instead of depending on private QPA control objects.

## Address and port options

QPA uses the same listener semantics as the Shared Runtime:

```text
hyremote-address=<numeric-ip-address>
hyremote-port=<1..65535>
hyremote-input=<0|1|false|true|off|on|no|yes>
```

Example:

```text
-platform "hyremote:hyremote-address=127.0.0.1:hyremote-port=5921:hyremote-input=false"
```

Invalid options fail closed.

See [`../../known-limitations.md`](../../known-limitations.md) for address-family and platform-specific limits.

## Multi-window behavior

QPA represents one Qt application as one logical remote session rather than one listener per window.

Supported application-owned top-level QWidget / QQuickWindow surfaces may enter and leave the remote canvas. Opening or closing a supported dialog, tool window, or second top-level window should not require restarting the listener.

Arbitrary foreign/native OS windows are not automatically part of the QPA support scope.

## Widgets / Qt Quick

QPA reuses the same Runtime target adapters as the other frontends:

- QWidget uses the Widgets adapter;
- QQuickWindow uses the Quick adapter;
- QOpenGLWidget, QQuickWidget, Quick3D, custom FBOs, and other complex combinations are supported only where explicitly stated by the compatibility matrix.

See [`../../compatibility.md`](../../compatibility.md).

## Security boundary

QPA reuses the Shared Runtime security policy:

- default bind is loopback;
- remote input is disabled by default;
- `Insecure` is unauthenticated and unencrypted: it is for a **trusted LAN only** and is not Internet-safe;
- `Authenticated` may use RFB VNC authentication, while the stream remains unencrypted;
- `AuthenticatedEncrypted` fails closed while the encrypted backend is unavailable.

Do not expose the current product directly to the public Internet. See [`../../security.md`](../../security.md).

## Troubleshooting

### `qhyremote` is missing

Confirm the application uses:

```cmake
hyremote_deploy(TARGET MyExistingApp QPA)
```

A normal deployed tree should contain `qhyremote` in the platform-plugin directory together with the matching native platform plugin.

### Qt version is rejected

QPA is an exact-private-ABI route. The current reference is Qt 6.8.3; a different patch must not be silently treated as compatible.

### Viewer can see but cannot control

That is the default. Enable `hyremote-input=true` only when remote control is deliberately required.

### Generic or QPA?

Prefer Generic when it satisfies the application. Use QPA when platform-entry behavior is actually required.

## Examples

The new Example curriculum teaches Generic and QPA separately so the two zero-code routes remain easy to compare.

> **TODO V0.3:** complete the bilingual, branded `examples/learning/07-zero-code-qpa/{widgets-app,quick-app}` product examples.