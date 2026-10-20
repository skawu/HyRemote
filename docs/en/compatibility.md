# HyRemote Compatibility Matrix

> Language / 语言: **English** | [中文](../compatibility.md)

HyRemote states compatibility only for environments with an explicit product status. Similar Qt APIs, a nearby patch version, another operating system, or a comparable graphics backend do not create a support claim.

## Status definitions

- **Supported:** qualified for the current product line. The C++ API, QML API, Generic Plugin, and QPA are peers. None is a primary or preview tier over another.
- **Limited:** qualified only under the stated narrower condition, such as QPA's exact Qt private ABI.
- **TODO:** planned product coverage that is not yet available or qualified.
- **Unsupported:** outside the stated product contract or known not to work for the stated combination.

## Current released reference matrix (V0.3.0.0)

The current released reference environment is **Qt 6.8.3 on Windows x86_64 and Linux x86_64**.

| Qt | Platform | Integration | UI target | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| 6.8.3 | Windows x86_64 | C++ API | Widgets | **Supported** | Public Qt APIs, explicit Runtime lifecycle |
| 6.8.3 | Windows x86_64 | C++ API | Qt Quick | **Supported** | Same `HyRemote::RemoteAccess` entry |
| 6.8.3 | Linux x86_64 | C++ API | Widgets | **Supported** | Public Qt APIs, explicit Runtime lifecycle |
| 6.8.3 | Linux x86_64 | C++ API | Qt Quick | **Supported** | Same `HyRemote::RemoteAccess` entry |
| 6.8.3 | Windows x86_64 | Generic Plugin | Widgets / Quick | **Supported** | Zero-code path, preserves native Qt platform |
| 6.8.3 | Linux x86_64 | Generic Plugin | Widgets / Quick | **Supported** | Zero-code path, preserves native Qt platform |
| 6.8.3 | Windows x86_64 | QML API | Qt Quick | **Supported** | Thin declarative frontend over Shared Runtime |
| 6.8.3 | Linux x86_64 | QML API | Qt Quick | **Supported** | Thin declarative frontend over Shared Runtime |
| 6.8.3 exact | Windows x86_64 | QPA | Widgets / Quick | **Limited** | Private ABI; native delegate `qwindows` |
| 6.8.3 exact | Linux x86_64 | QPA | Widgets / Quick | **Limited** | Private ABI; native delegate `qxcb` |

The table describes the current product line only. It does not imply that every graphics configuration inside a Widgets or Quick application is qualified.

## Qt LTS policy

HyRemote supports selected Qt LTS families and does not automatically promise every Qt release.

Current state:

- **Qt 6.8 LTS:** current reference family; Qt 6.8.3 is the exact reference SDK for V0.3.0.0.
- **Qt 5.15 LTS:** TODO. It is mandatory pre-GA V0.3-family adaptation and qualification work. Do not describe it as supported before the product can build, deploy, and qualify it.
- other Qt LTS or non-LTS families: no support claim without an explicit compatibility row.

Public Qt frontends and QPA follow different rules:

- C++, QML, and Generic primarily depend on public Qt APIs and can be qualified by Qt family or version range;
- QPA depends on the Qt private ABI and must be qualified per exact Qt patch and platform combination.

A Qt patch that is compatible through public APIs does not automatically make a QPA binary compatible.

## Operating systems

Current desktop reference platforms:

- Windows x86_64;
- Linux x86_64.

Passing on one operating system does not imply the same result on the other.

> **TODO V0.4:** expand formal qualification across the selected Qt LTS matrix while keeping Windows and Linux results separate.

## Product payload compatibility

| Product payload | Compatibility expectation |
| --- | --- |
| `HyRemote::RemoteAccess` | Shared Runtime used by the C++ API and internally by the other integration technologies |
| QML `HyRemote` module | Declarative payload over the same Shared Runtime |
| Generic Plugin | Public Qt generic-plugin payload; must preserve the application's native Qt platform identity |
| `qhyremote` QPA plugin | Exact-Qt private-ABI payload; delegates to the qualified native platform integration |
| Core | Internal implementation component, not an independent SDK or Runtime compatibility surface |

Applications must not mix payloads from unrelated HyRemote SDK builds or use a build-tree copy to repair an incomplete deployed package.

## Widgets scope

The portable correctness baseline supports qualified QWidget top-level targets through the Widgets adapter path.

Basic Widgets support does not automatically qualify configurations involving:

- `QOpenGLWidget`;
- embedded or native child windows;
- unusual platform-native ownership;
- third-party rendering engines that bypass normal QWidget rendering assumptions.

These cases receive explicit qualification when they enter the product matrix.

## Qt Quick scope

The portable correctness baseline supports qualified `QQuickWindow` targets through the public asynchronous Quick capture path.

Basic Qt Quick support does not automatically qualify configurations involving:

- Quick3D;
- custom FBO or render-node pipelines;
- unusual graphics backends;
- `QQuickWidget` mixed Widgets and Quick composition;
- foreign or native windows outside the application's normal Qt surface model.

> **TODO V0.4:** qualify representative real-world rendering and application combinations instead of broadening claims by inference.

## Generic Plugin compatibility

Generic Plugin is the preferred zero-code path when the application can keep its normal Qt platform.

A compatible Generic deployment satisfies all of the following:

- the application source remains Qt-only;
- the application does not link `HyRemote::RemoteAccess`;
- HyRemote is activated through Qt's generic-plugin mechanism;
- the native Qt platform identity remains `windows`, `xcb`, or the platform the application normally uses;
- the deployed tree contains both the Generic Plugin and the normal native Qt platform plugin;
- runtime does not depend on the original HyRemote or Qt build tree.

Generic compatibility is a public-Qt claim and is not tied to the QPA private ABI.

## QPA compatibility

QPA is a specialized zero-code route with a narrower compatibility boundary.

Current reference pairs:

| Qt | OS | Native delegate | Status |
| --- | --- | --- | --- |
| 6.8.3 exact | Windows x86_64 | `qwindows` | peer route |
| 6.8.3 exact | Linux x86_64 | `qxcb` | peer route |

Do not infer support for another Qt patch, Wayland, EGLFS, macOS, or another platform plugin from these rows.

> **TODO:** add a new QPA row only after the exact Qt, private-ABI, and platform combination is qualified.

## Transport and viewer boundary

The current transport baseline is bounded RFB 3.8.

Current product behavior:

- LAN-capable listener behavior by default;
- standard RFB remote viewing;
- optional remote input;
- reconnect without rebuilding the application Runtime;
- `Insecure` is unauthenticated and unencrypted, for a trusted LAN only;
- conditional RFB VNC authentication only when HyRemote was built with the transport-security capability and a valid security descriptor is configured;
- no stream encryption in the current product line;
- `AuthenticatedEncrypted` is unavailable and fails closed before listener creation.

The default build must not be interpreted as providing authenticated transport merely because the public API exposes the `Authenticated` profile.

Viewer interoperability claims are added only for viewers and versions that have been exercised explicitly. One successful connection does not broaden the compatibility matrix for every viewer implementation.

## Input compatibility

Remote input compatibility includes lifecycle behavior, not only ordinary pointer and key delivery.

The product must keep supported held key and button state balanced across disconnect and Runtime stop. Unsupported input-method, composition, or key cases are documented, not guessed.

## Deployment compatibility

A supported deployment path runs from the application's deployed tree without depending on:

- the original HyRemote SDK path;
- the HyRemote build tree;
- a Qt SDK plugin path used as a runtime crutch;
- manually copied internal HyRemote implementation files.

The product deployment entry point is `hyremote_deploy()`. See [`../guide/deployment.md`](../guide/deployment.md).

## Embedded and platform expansion

The current V0.3.0.0 matrix is desktop x86_64. ARM64, embedded Linux, and EGLFS are not supported by this release, but they are mandatory pre-GA V0.3-family compatibility baseline work and must converge before V0.4 RC entry. Additional BSP, SoC, or platform expansion beyond the declared GA reference cells is admitted separately as later coherent outcomes. Hardware or vendor acceleration has no preassigned version or GA placement; it is admitted through its recorded activation authority only when measured performance evidence proves a portable-baseline SLO or resource blocker.

Embedded deployment is a later product line, not part of the current desktop claim.

Planned directions:

| Direction | Current state |
| --- | --- |
| ARM64 embedded Linux | TODO, mandatory pre-GA V0.3-family baseline; no exact Feature version reserved |
| RK3588 / EGLFS or Wayland | TODO, mandatory pre-GA V0.3-family reference cells; no exact Feature version reserved |
| NXP i.MX class | TODO, separately admitted platform expansion beyond the declared GA cells |
| DMA-BUF / GBM / external-buffer paths | TODO, conditional implementation through recorded authority (#123) after measured performance evidence proves a blocker; no version placement |
| RKMPP / VAAPI / platform hardware encoding | TODO, conditional implementation through recorded authority (#123) after measured performance evidence proves a blocker; no version placement |
| OpenHarmony | Long-term direction; no current support claim |

Desktop evidence does not imply embedded support, and one BSP does not imply an entire SoC or platform family.

## Compatibility rules

1. Supported, Limited, TODO, and Unsupported are not interchangeable.
2. Windows results do not substitute for Linux results, or the reverse.
3. Public Qt API compatibility does not imply QPA private-ABI compatibility.
4. Basic Widgets or Quick support does not automatically qualify every graphics or rendering configuration.
5. Desktop x86 results do not imply embedded Linux support.
6. Hardware acceleration support is independent of the stable application integration contract.
7. New support rows are added only when the product can state the exact Qt, OS, integration path, application scope, and known limitations.
