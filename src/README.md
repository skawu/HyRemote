# `src/` - the shipping tree

`src/` contains HyRemote's shared implementation layers and the four application integration technologies exposed by the product.

The directory names intentionally describe architectural ownership rather than historical implementation order.

| Directory | Role | Installed? | What it provides | Compatibility boundary |
| --- | --- | --- | --- | --- |
| `core/` | shared core implementation | internal | session, frame/storage lifetime, normalized input, bounded scheduling/backpressure, capabilities | ordinary C++ / UI-neutral / protocol-neutral |
| `runtime/` | shared product runtime | internal runtime payload | Qt target adapters, concrete transport/security, factories, automatic application composition | Qt public API + product implementation |
| `cpp/` | **Embedded C++ integration** | yes | public `HyRemote::RemoteAccess` C++ facade | public C++ / Qt public API |
| `qml/` | **QML integration** | yes | `import HyRemote` frontend | Qt/QML public API |
| `generic/` | **Generic Plugin integration** | yes | `QGenericPlugin` zero-code payload (`-plugin hyremote`) | Qt public Generic Plugin API |
| `qpa/` | **QPA integration** | yes | `qhyremote` zero-code platform payload (`-platform hyremote`) | Qt private QPA ABI; qualified per supported Qt/QPA unit |

## Architectural meaning

`core` and `runtime` are shared implementation layers. `cpp`, `qml`, `generic` and `qpa` are **four peer integration frontends**.

`src/cpp` is not the common implementation parent of the other modes. It is specifically the Embedded C++ integration surface. Likewise, application-level automatic access logic is not inherently QPA logic and must not remain owned by `src/qpa` once it is shared by Generic Plugin and QPA modes.

The target dependency direction is:

```text
cpp --------\
qml ---------> runtime -> core
generic ----/
qpa --------/
```

No frontend may depend on another frontend as an architectural requirement.

## Core boundary

`src/core/` owns the product semantics that must remain independent of UI technology, concrete network transport and platform implementation:

- Session lifecycle/state/error semantics;
- `RemoteFrame` and storage lifetime;
- bounded mailbox/scheduling/backpressure;
- normalized remote input and dispatch boundary;
- target/transport-neutral capability and interface contracts.

Core must not require QWidget, Qt Quick/QML, Qt private/QPA types, concrete RFB implementation types, graphics-platform APIs or SoC/vendor APIs.

## Runtime boundary

`src/runtime/` owns implementation shared by multiple integration frontends that cannot live in the UI-neutral Core, including:

- Widgets and Quick target adapters;
- concrete RFB transport and transport-security implementation;
- component factories that assemble Core interfaces with product implementations;
- Qt application surface discovery;
- application-surface model and composite capture/input routing;
- automatic-access controller shared by Generic Plugin and QPA zero-code modes.

Runtime may depend on Qt public APIs and Core. It must not depend on any integration frontend.

## Four integration frontends

### `cpp/` — Embedded C++

Owns the public `HyRemote::RemoteAccess` C++ facade and Embedded C++ lifecycle/configuration surface. Product work is delegated to Runtime/Core.

### `qml/` — QML

Owns `import HyRemote` and the QML property/lifecycle facade. It does not own a separate Session, capture, input or transport architecture.

### `generic/` — Generic Plugin

Owns the `QGenericPlugin` payload for zero-code integration while keeping the application's native Qt platform integration unchanged. Generic Plugin uses Qt public APIs and bootstraps the shared automatic runtime.

### `qpa/` — QPA

Owns only the Qt private/QPA compatibility boundary and QPA-specific process bootstrap/configuration. The long-term design is a Factory Trampoline that creates and returns the qualified native Qt platform integration while using the same automatic runtime as Generic Plugin.

Only `src/qpa/` may include/link Qt private QPA interfaces for these four integration modes.

## Platform rule

HyRemote does not create Rockchip/NXP/TI-specific QPA product personalities. QPA delegates to the native Qt backend (`windows`, `xcb`, `wayland`, `eglfs`, etc.); vendor Qt/BSP owns native display/input/GPU adaptation. Platform-specific product code is added only for a proven compatibility shim or a separately justified acceleration backend.

## Stable binary/build paths

Source ownership and build artifact paths are separate concerns. The root build continues to map each source directory to an explicit binary directory so an ownership migration does not accidentally relocate installed/deployment artifacts.

The architecture migration for #219 may preserve existing artifact names while source and target ownership are corrected. Artifact/API changes require their own explicit compatibility decision; they are not implied by moving implementation code.

## Repository ownership

The final repository layout is owned by `docs/internal/repository-layout.md` and ADR-0007. `src/` remains the shipping tree; `tests/`, `examples/`, `docs/`, `cmake/` and `.github/` keep their existing evidence/user/process responsibilities.

The V1 release candidate remains governed by its frozen evidence chain. The #219 source/runtime migration is developed on a dedicated post-V1 architecture branch and must not be back-ported into the frozen V1 candidate merely to make its layout match this target architecture.
