# HyRemote Compatibility Matrix

HyRemote compatibility is stated only for environments that have an explicit product status. Similar Qt APIs, a nearby patch version, another operating system, or a comparable graphics backend do not automatically create a support claim.

## Status definitions

- **Primary** — current product path for the active product line.
- **Preview** — implemented and usable, but not yet promoted to the same product-support level as the primary path.
- **TODO** — planned product coverage that is not yet available/qualified.
- **Unsupported** — outside the stated product contract or known not to work for the stated combination.

## Current V0.1 reference matrix

The current Developer Preview reference environment is **Qt 6.8.3 on Windows x86_64 and Linux x86_64**.

| Qt | Platform | Integration | UI target | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| 6.8.3 | Windows x86_64 | C++ API | Widgets | **Primary** | Public Qt APIs, explicit Runtime lifecycle |
| 6.8.3 | Windows x86_64 | C++ API | Qt Quick | **Primary** | Same `HyRemote::RemoteAccess` facade |
| 6.8.3 | Linux x86_64 | C++ API | Widgets | **Primary** | Public Qt APIs, explicit Runtime lifecycle |
| 6.8.3 | Linux x86_64 | C++ API | Qt Quick | **Primary** | Same `HyRemote::RemoteAccess` facade |
| 6.8.3 | Windows x86_64 | Generic Plugin | Widgets / Quick | **Primary** | Zero-code path, preserves native Qt platform |
| 6.8.3 | Linux x86_64 | Generic Plugin | Widgets / Quick | **Primary** | Zero-code path, preserves native Qt platform |
| 6.8.3 | Windows x86_64 | QML API | Qt Quick | **Preview** | Thin declarative frontend over Shared Runtime |
| 6.8.3 | Linux x86_64 | QML API | Qt Quick | **Preview** | Thin declarative frontend over Shared Runtime |
| 6.8.3 exact | Windows x86_64 | QPA | Widgets / Quick | **Preview** | Private ABI; native delegate `qwindows` |
| 6.8.3 exact | Linux x86_64 | QPA | Widgets / Quick | **Preview** | Private ABI; native delegate `qxcb` |

The matrix above describes the current product line. It does not imply that every graphics configuration inside a Widgets/Quick application is already qualified.

## Qt LTS policy

HyRemote is intended to support selected Qt LTS families rather than promise every Qt release automatically.

Current state:

- **Qt 6.8 LTS** — current reference family; Qt **6.8.3** is the exact reference SDK used by V0.1.
- **Qt 5.15 LTS** — **TODO V0.4 qualification**. Do not describe it as currently supported until the product can actually build, deploy, and run through the applicable matrix.
- other Qt LTS/non-LTS families — no support claim unless they receive an explicit compatibility row.

Public-Qt frontends and QPA have different compatibility rules:

- **C++ / QML / Generic** primarily depend on public Qt APIs and can be qualified by Qt family/version range;
- **QPA** depends on Qt private ABI and must be qualified per exact Qt patch/platform combination.

A public-API-compatible Qt patch does not automatically make a QPA binary compatible.

## Operating systems

Current desktop reference platforms:

- Windows x86_64;
- Linux x86_64.

Passing on one operating system does not imply the same result on the other.

> **TODO V0.4:** expand formal qualification coverage across the selected Qt LTS matrix while keeping Windows/Linux results explicit.

## Product payload compatibility

| Product payload | Compatibility expectation |
| --- | --- |
| `HyRemote::RemoteAccess` | Shared Runtime used by the C++ API and by other frontends internally |
| QML `HyRemote` module | Declarative payload over the same Shared Runtime |
| Generic Plugin | Public Qt generic-plugin payload; must preserve the application's native Qt platform identity |
| `qhyremote` QPA plugin | Exact-Qt private-ABI payload; delegates to the qualified native platform integration |
| Core | Internal implementation component, not an independent SDK/runtime compatibility surface |

Applications should not mix payloads from unrelated HyRemote SDK builds or rely on a build-tree copy to repair an incomplete deployed package.

## Widgets scope

The portable correctness baseline supports qualified QWidget top-level targets through the Widgets adapter path.

Basic Widgets support does not automatically qualify every configuration involving:

- `QOpenGLWidget`;
- embedded/native child windows;
- unusual platform-native ownership;
- third-party rendering engines that bypass normal QWidget rendering assumptions.

These cases receive explicit qualification when they become part of the product matrix.

## Qt Quick scope

The portable correctness baseline supports qualified `QQuickWindow` targets through the public asynchronous Quick capture path.

Basic Qt Quick support does not automatically qualify every configuration involving:

- Quick3D;
- custom FBO/render-node pipelines;
- unusual graphics backends;
- `QQuickWidget` mixed Widgets/Quick composition;
- foreign/native windows outside the application's normal Qt surface model.

> **TODO V0.4:** qualify representative real-world rendering/application combinations instead of broadening claims by inference.

## Generic Plugin compatibility

Generic Plugin is the preferred zero-code path when the application can keep its normal Qt platform.

A compatible Generic deployment should satisfy all of the following:

- the application source remains Qt-only;
- the application does not link `HyRemote::RemoteAccess`;
- HyRemote is activated through Qt's generic-plugin mechanism;
- the native Qt platform identity remains `windows`, `xcb`, or the platform the application would normally use;
- the deployed tree contains both the Generic Plugin and the normal native Qt platform plugin;
- the application does not depend on the original HyRemote/Qt build tree at runtime.

Generic compatibility is a public-Qt claim and is not tied to QPA private ABI.

## QPA compatibility

QPA is a specialized zero-code route and has a narrower compatibility boundary.

Current reference pairs:

| Qt | OS | Native delegate | Status |
| --- | --- | --- | --- |
| 6.8.3 exact | Windows x86_64 | `qwindows` | **Preview** |
| 6.8.3 exact | Linux x86_64 | `qxcb` | **Preview** |

Do not infer support for another Qt patch, Wayland, EGLFS, macOS, or another platform plugin from these rows.

> **TODO:** add new QPA rows only after the exact Qt/private-ABI/platform combination is qualified.

## Transport and viewer boundary

The current transport baseline is bounded RFB 3.8.

Current product behavior includes:

- loopback-first listener behavior;
- standard RFB remote viewing;
- optional remote input;
- reconnect without rebuilding the application Runtime;
- `Insecure` loopback-only behavior;
- conditional RFB VNC authentication only when HyRemote was built with the transport-security capability and a valid security descriptor is configured;
- stream encryption is available for the `AuthenticatedEncrypted` profile and requires a transport-security-enabled build, an available OpenSSL 3.x TLS runtime for Qt's OpenSSL backend, and a matching certificate/private-key pair; `Insecure` and `Authenticated` stay unencrypted;
- `AuthenticatedEncrypted` fails closed before listener creation when that backend or that material is unusable, and never falls back to a weaker profile.

The default V0.1 build/profile must not be interpreted as providing authenticated transport merely because the public API exposes the `Authenticated` profile.

Viewer-specific interoperability claims should be added only for viewers/versions that have been explicitly exercised. A viewer's ability to connect once is not enough to broaden the product compatibility matrix for every viewer implementation.

## Input compatibility

Remote input compatibility includes lifecycle behavior, not only ordinary pointer/key delivery.

The product must keep supported held key/button state balanced across disconnect and Runtime stop. Unsupported IME/composition or key cases are documented rather than guessed.

## Deployment compatibility

A supported deployment path should run from the application's deployed tree without depending on:

- the original HyRemote SDK path;
- the HyRemote build tree;
- a Qt SDK plugin path used as a runtime crutch;
- manually copied internal HyRemote implementation files.

The product deployment entry point is `hyremote_deploy()`; see [`guide/deployment.md`](guide/deployment.md).

## Embedded/platform expansion

Embedded deployment is a later product line, not part of the current V0.1 desktop claim.

Planned directions include:

| Direction | Current status |
| --- | --- |
| ARM64 Embedded Linux | **TODO V1.1** |
| RK3588 / EGLFS or Wayland | **TODO V1.1** |
| NXP i.MX class | **TODO V1.1** |
| DMA-BUF / GBM / external-buffer paths | **TODO later acceleration line** |
| RKMPP / VAAPI / platform hardware encoding | **TODO later acceleration line** |
| OpenHarmony | Long-term direction; no current support claim |

Desktop evidence does not imply embedded support, and one BSP does not imply an entire SoC/platform family.

## Compatibility rules

1. Primary/Preview/TODO statuses are not interchangeable.
2. Windows results do not substitute for Linux results, or vice versa.
3. Public Qt API compatibility does not imply QPA private-ABI compatibility.
4. Basic Widgets/Quick support does not automatically qualify every graphics/rendering configuration.
5. Desktop x86 results do not imply Embedded Linux support.
6. Hardware acceleration support is independent of the stable application-facing integration contract.
7. New support rows are added only when the product can state the exact Qt, OS, integration path, application scope, and known limitations.
