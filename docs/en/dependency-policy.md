# Third-Party Dependency Policy
> **Current authority (2026-10-23):** V0.3.0.0 Product Preview has shipped; first GA support covers only actually qualified Windows Qt5.15/Qt6.8 cells. Encrypted transport, gateway, relay, cloud services and additional platforms require separate admission; historical TODOs do not create GA requirements. Long-lived dependency and vulnerability response: [#611](https://github.com/skawu/HyRemote/issues/611).


> Language / 语言: **English** | [中文](../dependency-policy.md)

HyRemote is licensed under the Apache License 2.0 and is intended for reuse in commercial and open-source Qt products, including future embedded deployments.

Dependencies must be technically justified, maintainable, legally compatible, and invisible to normal application integration unless a product capability genuinely requires them.

## Product toolchain rule

HyRemote has one Shared Runtime and four peer integration frontends:

```text
C++ API ---------\
QML API ----------\
Generic Plugin ----> Shared Runtime -> Core
QPA --------------/
```

The normal product implementation uses C++17, CMake, and Qt.

C++/QML/Generic/QPA are integration frontends, not separate backend toolchains. A normal Windows or Linux SDK consumer does not install Rust, Cargo, Go, Node, Python, or another backend-specific toolchain merely to use HyRemote.

A future implementation may use an additional language or toolchain internally only when the resulting SDK preserves the documented application API, package, deployment, and platform contracts.

## Dependency classes

HyRemote distinguishes three classes of dependency:

| Class | Meaning |
| --- | --- |
| **Application/runtime dependency** | Needed by a shipped HyRemote capability at runtime |
| **Build dependency** | Needed to build an enabled capability but not exposed as an application API |
| **Development/verification tool** | Used to build, test, benchmark, package, or qualify HyRemote; not part of the installed product contract |

Development tooling does not become an SDK/runtime prerequisite merely because the repository uses it.

## No hidden dependency downloads

The HyRemote product build does not silently download or patch major product dependencies.

In particular:

- Qt is supplied by the selected developer, CI, or toolchain environment;
- OpenSSL, when a secure transport build requires it, is supplied by the environment or an explicitly selected prefix;
- product dependencies are not silently added through hidden `FetchContent` or `ExternalProject` download flows;
- third-party application sources used for qualification are not installed as part of the HyRemote SDK.

This keeps offline, enterprise, and embedded build environments predictable.

## Qt

Core remains independent of Qt UI technology.

The Shared Runtime and frontends use Qt under these rules:

- public Qt APIs are preferred for normal product integration;
- Generic Plugin stays on public Qt plugin APIs and preserves the application's native platform integration;
- QPA is the only frontend allowed to depend on the Qt private/QPA ABI as part of its product role;
- QPA compatibility is qualified per exact Qt patch and platform combination;
- public-API compatibility never implies QPA private-ABI compatibility.

Current desktop reference: Qt 6.8.3.

Qt remains under its own licensing terms. A distributor is responsible for the licence, notice, relinking or source-availability, and other obligations associated with the exact Qt binaries it redistributes.

HyRemote does not change those obligations.

## Transport security and OpenSSL

Transport security is a Runtime capability, not a fifth integration mode.

The build capability is represented by:

```text
HYREMOTE_WITH_TRANSPORT_SECURITY
```

When secure transport is requested:

- the required cryptographic or TLS dependency must be explicitly available;
- configuration fails closed when the dependency is unavailable;
- HyRemote does not silently downgrade a requested secure build to an insecure one;
- cryptographic implementation types remain private Runtime details, not downstream SDK targets.

In the released V0.3.0.0 Product Preview, `AuthenticatedEncrypted` remains unavailable and fails closed before creating a listener.

> **Future candidate, not an automatic first-GA requirement:** encrypted transport, certificate policy and exact third-party runtime dependencies require their own user-value, security and #24/#95 release admission. A TODO does not imply shipped encryption or Internet-safe exposure (#143/#190/#611).

## RFB/VNC transport baseline

The current Windows and Linux correctness baseline is HyRemote's bounded C++ RFB 3.8 transport behind the private Runtime transport boundary.

RFB is an implementation and interoperability baseline, not the permanent public identity of HyRemote.

All four frontends share the same transport-neutral Runtime and Core model, so a future accepted transport may replace or supplement RFB without creating frontend-specific Runtime variants.

Current security semantics:

- `Insecure` is appropriate only within the documented trusted or loopback boundary;
- `Authenticated` may use RFB VNC authentication but remains unencrypted;
- `AuthenticatedEncrypted` fails closed while encrypted transport is unavailable.

## Adding a new product dependency

A new dependency must have a documented product justification covering:

1. upstream project and canonical source;
2. selected release, tag, or commit policy;
3. licence and attribution obligations;
4. compatibility with HyRemote's distribution model;
5. static and dynamic linking implications;
6. activity, maintenance, and security-update posture;
7. required or optional status;
8. supported platforms and toolchains;
9. effect on C++/QML/Generic/QPA packaging and deployment;
10. fallback and removal strategy;
11. offline and cross-build impact;
12. whether upstream contribution is preferable to maintaining a fork.

A dependency must not silently force downstream applications into an incompatible licence or a new mandatory toolchain merely by using HyRemote.

## Fork policy

Prefer upstream collaboration over a permanent HyRemote-specific fork when a change is generally useful.

A temporary pinned patch or fork must have a clear product reason and a deletion or upstreaming plan. It must not become invisible long-term infrastructure that users are expected to understand or maintain.

## Product-distribution rules

Two product rules are especially important:

### Qt remains external to the HyRemote source tree

HyRemote consumes a selected Qt SDK. It does not fetch, rebuild, or patch Qt as part of the normal product build.

QPA may compile against qualified Qt private interfaces, but that does not turn Qt into vendored HyRemote source or broaden the QPA compatibility promise.

### Third-party qualification applications are not HyRemote runtime payloads

Real-world applications used to qualify HyRemote remain separate upstream projects. HyRemote records the exact upstream revision and test method but does not rebrand or redistribute those applications as part of its own SDK unless a future distribution decision explicitly addresses the associated licensing obligations.

## Platform and hardware libraries

GBM, DRM, DMA-BUF-related libraries, RKMPP, V4L2, VA-API, D3D-class APIs, and similar platform or hardware dependencies remain optional implementation dependencies unless a product line explicitly requires them.

They must not:

- enter the generic Core;
- change the four application integration contracts;
- force Generic or QPA applications to link SoC-specific libraries directly;
- become prerequisites for ordinary desktop users merely to optimize one target platform.

> **Current Owner #563/#605 boundary:** first GA qualification is limited to actually tested Windows x86_64 Qt5.15/Qt6.8 cells. Linux/ARM64/EGLFS/Wayland are community/user-validation pending, not RC/GA gates; publish exact BSP/package manifests only when those environments have real deploy evidence. Qt6.5/6.12 belong to independent post-GA qualification, with no version number preallocated.

## Historical and future candidates

Past experiments with other VNC or transport implementations remain engineering evidence, not current application dependencies.

A future transport or low-copy dependency is accepted only when it provides measurable product value while preserving the normal C++/Qt SDK and deployment experience.

No dependency is promoted merely because it is technically interesting.

## Current limits

The current product line adds no new transport-security runtime dependency. Encrypted transport remains unimplemented.

Any new dependency must update `LICENSE`, `NOTICE.md`, and the deployed payload list in the same change.
