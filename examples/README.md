# HyRemote Examples

## What HyRemote is

HyRemote adds remote access to an **existing Qt application** with as little intrusion as possible. You keep your
application; remote access is delivered as an SDK and a deployment payload rather than as a rewrite, and your
application does not have to learn about sessions, transports, capture or protocol internals.

V0.1 is a **Developer Preview** with two primary adoption paths and two preview frontends. All four frontends share
one Runtime/Core: they are different ways for an application to reach the same remote-access runtime, not different
products.

| V0.1 status | Frontend | What it means |
| --- | --- | --- |
| **Primary** | Embedded C++ API - `HyRemote::RemoteAccess` | link one shared library and drive the runtime from your own code |
| **Primary** | Generic Plugin - `-plugin hyremote` | keep the application source untouched; remote access arrives at run time |
| **Preview** | Declarative QML API - `import HyRemote` | a thin declarative wrapper over the same runtime |
| **Preview** | Transparent QPA Proxy - `-platform hyremote` | a platform integration for applications that cannot be edited |

## Which path is yours?

| Your application | Recommended V0.1 path |
| --- | --- |
| An editable Qt Widgets application | C++ API - [example 01](learning/01-widgets-cpp) |
| An editable Qt Quick / QML UI | C++ API - [example 02](learning/02-quick-cpp) |
| An existing application you would rather not modify | Generic Plugin - [example 03](learning/03-zero-code-generic) |
| You need platform-entry or Qt private-ABI behaviour | QPA Proxy - **Preview** ([guide](../docs/getting-started/qpa-proxy.md)) |
| You specifically want a declarative HyRemote API | QML API - **Preview** ([guide](../docs/getting-started/qml.md)) |

**A Qt Quick UI is not the HyRemote QML frontend.** Qt Quick is a UI family; the `HyRemote` QML module is one of the
four integration frontends. A Qt Quick application normally takes the **C++ API** in V0.1, exactly like a Widgets
application - see [example 02](learning/02-quick-cpp), which uses a Qt Quick window and no `import HyRemote` at all.

## The 5-minute path

1. **Build and run [example 01](learning/01-widgets-cpp)** - a Qt Widgets window that becomes remotely viewable.
2. **Connect a viewer** to `<host-lan-ip>:<port>` and see that window.
3. **Then read the guide for your route:**
   - [Embedded C++](../docs/getting-started/cpp.md) - the C++ API, for Widgets and for Quick;
   - [Generic Plugin](../docs/getting-started/generic.md) - for an application you do not want to edit;
   - [Deployment](../docs/guide/deployment.md) - how the SDK and the deployment payload fit together.

Nothing in that path requires reading Runtime, Core, Session, transport or protocol internals: the public facade is
one type, and the deployment helper is one call.

## The three V0.1 examples

| Example | What it shows |
| --- | --- |
| [01 - Widgets + C++](learning/01-widgets-cpp) | The shortest real C++ path: a `QMainWindow`, `HyRemote::RemoteAccess`, an explicit `start()`, a listener on `0.0.0.0:5921`, view-only by default, an explicit `stop()`. |
| [02 - Quick + C++](learning/02-quick-cpp) | The same C++ path on a Qt Quick window. It links Qt Quick/Qml plus `HyRemote::RemoteAccess` and never imports `HyRemote`: **Quick UI does not require the HyRemote QML frontend.** |
| [03 - Generic zero-code](learning/03-zero-code-generic) | Two ordinary Qt applications - Widgets and Quick - with zero HyRemote headers, zero HyRemote API calls and zero HyRemote link dependencies. They run normally, or become remotely viewable with `-plugin hyremote`. |

Each example directory has its own `README.md` with build, run and viewer instructions.

## Reference and compatibility

- **Reference matrix:** Windows x86_64 and Linux x86_64, with **Qt 6.8.3**.
- Those two platforms are what V0.1 is built and exercised on. A different Qt version is not implied to be supported
  unless it is recorded in [`docs/compatibility.md`](../docs/compatibility.md).
- **Qt 5.15 is not yet qualified** for V0.1: it is not supported, and it is not ruled out for ever - it is a later
  preflight decision, not a V0.1 statement.

## Security boundary

The shipped candidate is a **LAN-capable Developer Preview**:

- the listener is on `0.0.0.0:5921` by default, so it is reachable on this host's IPv4 interfaces;
- remote input is **off** until you explicitly enable it;
- it is for a **trusted LAN only** and is **not** Internet-safe, and this candidate makes **no encrypted-transport promise**;
- secure remote access over untrusted networks belongs to a later release.

The deployment and platform-integrity details are in [`docs/known-limitations.md`](../docs/known-limitations.md) and
[`docs/security-model.md`](../docs/security-model.md); the examples do not restate the security architecture.

## Preview surfaces

The QML API and the Transparent QPA Proxy are **Preview** in V0.1: they are real, they share the same runtime, and
their support contract is deliberately narrower than the two primary paths. Use them when your situation actually
calls for them:

- [`examples/qml-basic`](qml-basic) and [`docs/getting-started/qml.md`](../docs/getting-started/qml.md) - the
  declarative API;
- [`examples/qpa-proxy-existing-app`](qpa-proxy-existing-app) and
  [`docs/getting-started/qpa-proxy.md`](../docs/getting-started/qpa-proxy.md) - the platform-level route;
- [`examples/remote-support-showcase`](remote-support-showcase) - a wider operator-workflow sample over the C++ API.

They are not additional V0.1 primary adoption routes.

## Building the examples

Inside the repository build, configure with examples enabled:

```text
build.cmd install --integrations=cpp,qml,generic,qpa --examples --qt-prefix=<qt-prefix>
```

The runnable examples are installed into `build/install/bin/` and run from there with no Qt SDK on `PATH` and no plugin-path variables set. `build/` on its own is developer intermediate output; `build/install/` is the tree to run and to consume as an SDK.

```
```

`HYREMOTE_BUILD_EXAMPLES=ON` is the CMake spelling, and the optional frontends stay explicit
(`HYREMOTE_BUILD_QML_API`, `HYREMOTE_WITH_QPA_PROXY`). A missing optional frontend skips only its own example - it is
never silently substituted by another integration path. The three V0.1 examples also build standalone against an
installed SDK, which is how an application would consume them.

## Not here yet

Complete bilingual example documentation, shared UI/branding infrastructure and a full 01-09 learning curriculum are
**V0.3 productization**, not V0.1. V0.1 ships the three examples above as the adoption paths, in English, with the
SDK and deployment guides above as the reference material.

## Further material

- Product overview and architecture: [`../README.md`](../README.md)
- Repository layout authority: [`../docs/internal/repository-layout.md`](../docs/internal/repository-layout.md)
- Release discipline: [`../CONTRIBUTING.md`](../CONTRIBUTING.md)
