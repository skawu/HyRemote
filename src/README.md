# `src/` - the shipping tree

`src/` contains one internal base and the three application integration technologies exposed by HyRemote.
The directory names intentionally use the technical terms developers already know from C++ and Qt:

| Directory | Role | Installed? | What it provides | Compatibility boundary |
| --- | --- | --- | --- | --- |
| `core/` | internal base | no | session, frame, storage, normalized input, capabilities | Qt-free/platform-free |
| `cpp/` | **C++ API integration** | yes | the one shared runtime and public `HyRemote::RemoteAccess` facade | Qt public API |
| `qml/` | **QML API integration** | yes | `import HyRemote`, a thin declarative layer over the same runtime | Qt/QML public API |
| `qpa/` | **QPA integration** | yes | `qhyremote`, a Qt Platform Abstraction platform plugin for low-intrusion application integration | Qt private QPA ABI; exact-patch qualification |

## Why these names

The previous source names `embedded`, `declarative` and `transparent` described qualities of the integration modes but hid the actual technologies. They also made `embedded` collide conceptually with future Embedded Linux/RK3588/platform work.

The frozen V1 terminology is therefore:

```text
C++ API integration
QML API integration
QPA integration
```

`C++`, `QML` and `QPA` are the terms used in code, documentation, examples and acceptance records. `QPA` means Qt Platform Abstraction. `QML` is both the declarative language used for Qt Quick UI and the surface of HyRemote's QML API; it is not a third UI rendering family beside Qt Widgets and Qt Quick.

## Dependency rules

- **One runtime, three ways in.** `src/cpp/` owns the single `HyRemoteRemoteAccess` shared runtime. The QML and QPA payloads reuse it; they never build a second runtime.
- `src/core/` is internal-only. Applications, QML and QPA never consume `HyRemote::Core` as a public/installed target.
- Dependency direction is:

```text
core -> cpp -> { qml, qpa }
```

The reverse direction is forbidden.
- `src/qml/` is a thin wrapper over the C++ runtime, not an independent product core.
- `src/qpa/` owns all QPA/private-ABI adaptation. Qt-family differences stay private here (for example bounded Qt 5.15 vs Qt 6.8 adapter code); they do not create separate public APIs or top-level product personalities.
- Every built `qhyremote` payload is qualified against the exact Qt patch/private ABI it runs with.

## Stable binary paths

Source naming is independent from artifact/binary-directory naming. The root build may map source directories to stable binary directories, for example:

```cmake
add_subdirectory(src/cpp remoteaccess)
add_subdirectory(src/qml qml/HyRemote)
add_subdirectory(src/qpa qpa)
```

This lets the repository use accurate source ownership names without unnecessarily moving installed artifacts or CI output paths.

## Repository ownership

The final repository layout is owned by `docs/internal/repository-layout.md` and #209. After the V1 foundation migration completes, adding a new Qt LTS, platform backend or product feature must not trigger another basic `src/` reorganization.
