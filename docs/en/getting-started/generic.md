# Generic Plugin integration (zero business-code changes)

> Language / 语言: **English** | [中文](../../getting-started/generic.md)

The Generic Plugin is intended for existing Qt Widgets and Qt Quick applications. The application remains an ordinary Qt program and does not link the HyRemote API; remote access is enabled at runtime through Qt's **generic plugin** mechanism.

Its defining property is that it **does not replace the application's Qt platform integration**. Windows keeps `qwindows`, Linux/X11 keeps `qxcb`, and HyRemote attaches the same Shared Runtime alongside the native platform path.

## When to use it

Prefer the Generic Plugin when you want to:

- avoid business-code changes;
- avoid linking `HyRemote::RemoteAccess` into the application;
- keep the native Qt platform, local window/input, and GPU path;
- package everything through the same `hyremote_deploy()` entry point.

If the application needs explicit runtime start/stop, target switching, or policy control, use the [C++ API](cpp.md) instead.

## Build the application

The application still links only Qt:

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)

target_link_libraries(MyApp PRIVATE Qt6::Widgets)
```

A Qt Quick application links `Qt6::Quick` instead.

## Deploy

Bring HyRemote into the packaging stage:

```cmake
find_package(HyRemote CONFIG REQUIRED)

install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp GENERIC)
```

The deployed tree contains:

- the HyRemote Generic Plugin;
- the Shared Runtime;
- the native Qt platform plugin needed by the application;
- the required Qt runtime dependencies.

The application source still links no HyRemote target.

## Start the application

Use Qt's generic-plugin command-line option:

```text
MyApp -plugin hyremote
```

Or use the environment variable:

```text
QT_QPA_GENERIC_PLUGINS=hyremote
```

The defaults match the C++ API:

- address: `127.0.0.1`;
- port: `5921`;
- remote input: disabled;
- unauthenticated non-loopback listening: rejected.

## Configuration format

The Generic Plugin uses `key=value` fields separated by semicolons:

```text
address=127.0.0.1;port=5901;input=true;security=insecure
```

Recognized fields:

| Field | Meaning |
| --- | --- |
| `address` | Numeric IP address |
| `port` | `1..65535` |
| `input` | `true/false`, `on/off`, `yes/no`, `1/0` |
| `security` | `insecure`, `authenticated`, `authenticated-encrypted` |
| `security-config` | Security configuration file path |

`authenticated-encrypted` **fails closed** in V0.1. It is never silently downgraded to an unencrypted connection.

> **TODO:** V0.2 will provide the complete VeNCrypt/TLS, certificate-policy, and session-security implementation before `authenticated-encrypted` is presented as a production security profile.

## Native platform identity is preserved

Generic Plugin and QPA are different product entry points:

```text
Generic Plugin:
Qt application -> native qwindows/qxcb/... + HyRemote generic plugin

QPA:
Qt application -> qhyremote Factory Trampoline -> native qwindows/qxcb
```

The Generic Plugin should not change `QGuiApplication::platformName()` to `hyremote`.

## Widgets and Qt Quick

The Generic Plugin's automatic-access Runtime discovers and composes supported application top-level windows. Widgets and Qt Quick share the same Runtime; the application does not need to change UI technology.

Current reference environments:

- Windows x86_64 + Qt 6.8.3;
- Linux x86_64 + Qt 6.8.3.

See [`../../compatibility.md`](../../compatibility.md) for other environments.

## Security boundary

V0.1 is a Developer Preview:

- loopback is the default bind;
- remote control is disabled by default;
- unauthenticated non-loopback listening is rejected;
- `AuthenticatedEncrypted` fails closed while the TLS backend is unavailable;
- do not expose the current product directly to the public Internet.

See [`../../security.md`](../../security.md).

## Deployment troubleshooting

If the application cannot start on a machine without the Qt SDK, first confirm that the deployed tree contains:

- the HyRemote Generic Plugin under `plugins/generic/`;
- the application's normal Qt platform plugin under `plugins/platforms/`;
- the Shared Runtime and Qt runtime dependencies.

Do not hide an incomplete deployment by pointing `QT_PLUGIN_PATH`, `LD_LIBRARY_PATH`, or other variables at the SDK/build tree. A product deployment should be self-contained through `hyremote_deploy(TARGET ... GENERIC)`.

## Next

- Deployment: [`../../guide/deployment.md`](../../guide/deployment.md)
- Viewer: [`../../guide/viewer-connection.md`](../../guide/viewer-connection.md)
- Security: [`../../security.md`](../../security.md)
- Compatibility: [`../../compatibility.md`](../../compatibility.md)
- Known limitations: [`../../known-limitations.md`](../../known-limitations.md)
