# HyRemote Examples

## What HyRemote is

HyRemote adds remote access to an **existing Qt application** with as little intrusion as possible. You keep your
application; remote access is delivered as an SDK and a deployment payload rather than as a rewrite, and your
application does not have to learn about sessions, transports, capture or protocol internals.

The four frontends are **peers** and share one Runtime/Core: they are different ways for an application to reach the
same remote-access runtime, not different products. Every route follows the same
[acquisition -> integrate -> deploy -> run -> connect](../docs/guide/integrate-your-project.md) flow.

| Frontend | What it means |
| --- | --- |
| Embedded C++ API - `HyRemote::RemoteAccess` | link one shared library and drive the runtime from your own code |
| Generic Plugin - `-plugin hyremote` | keep the application source untouched; remote access arrives at run time |
| Declarative QML API - `import HyRemote` | a thin declarative wrapper over the same runtime |
| Transparent QPA Proxy - `-platform hyremote` | a platform integration for applications that cannot be edited; exact Qt/private ABI |

## Which path is yours?

| Your application | Route | Runnable example |
| --- | --- | --- |
| An editable Qt Widgets application | Embedded C++ API | [01 - Widgets + C++](learning/01-widgets-cpp) |
| An editable Qt Quick / QML UI, keeping C++ control | Embedded C++ API | [02 - Quick + C++](learning/02-quick-cpp) |
| You specifically want a declarative HyRemote API | Declarative QML API | [04 - QML API](learning/04-qml) |
| An existing application you would rather not modify | Generic Plugin | [03 - Generic zero-code](learning/03-zero-code-generic) |
| You need platform-entry or exact Qt private-ABI behaviour | Transparent QPA | [05 - QPA proxy](learning/05-qpa) |

The five entries above are **runnable peers**. The QPA route requires the exact qualified Qt build; the QML route
requires the QML frontend to be built and deployed. Neither is a replacement for the C++ route.

**A Qt Quick UI is not the HyRemote QML frontend.** Qt Quick is a UI family; the `HyRemote` QML module is one of the
four integration frontends. A Qt Quick application normally takes the **C++ API** - see
[02 - Quick + C++](learning/02-quick-cpp), which uses a Qt Quick window and no `import HyRemote` at all.

## The 5-minute path

1. **Build and install the examples** with the command below, then run **01 - Widgets + C++** from
   `build/install/bin/`.
2. **Connect a viewer** to `<host-lan-ip>:<port>` and see that window.
3. **Pick your route** from the table above and open its `README.md`.
4. **Then read the task guides:**
   - [Install and find the SDK](../docs/guide/install.md);
   - [Embedded C++ guide](../docs/getting-started/cpp.md);
   - [Generic Plugin guide](../docs/getting-started/generic.md);
   - [Integrate your project](../docs/guide/integrate-your-project.md);
   - [Deploy and run](../docs/guide/deployment.md);
   - [Connect a viewer](../docs/guide/viewer-connection.md);
   - [Troubleshoot](../docs/guide/troubleshooting.md).

Nothing in that path requires reading Runtime, Core, Session, transport or protocol internals: the public facade is
one type, and the deployment helper is one call.

## The five runnable route examples

| Example | Route | What it shows |
| --- | --- | --- |
| [01 - Widgets + C++](learning/01-widgets-cpp) | C++ API / Widgets | The shortest real C++ path: a `QMainWindow`, `HyRemote::RemoteAccess`, an explicit `start()`, listener `0.0.0.0:5921`, view-only by default, explicit `stop()`. |
| [02 - Quick + C++](learning/02-quick-cpp) | C++ API / Qt Quick | The same C++ path on a Qt Quick window. It never imports `HyRemote`: **Quick UI does not require the QML frontend.** |
| [04 - QML API](learning/04-qml) | QML API | `import HyRemote` and a declarative `RemoteAccess` object over the same runtime. |
| [03 - Generic zero-code](learning/03-zero-code-generic) | Generic plugin | Two ordinary Qt applications - Widgets and Quick - with zero HyRemote headers, API calls or link dependencies; run normally, or add `-plugin hyremote`. |
| [05 - QPA proxy](learning/05-qpa) | QPA proxy | An ordinary Qt Widgets application using `-platform hyremote`; exact Qt 6.8.3/private-ABI boundary, no HyRemote application API in its source. |

Each example directory has its own `README.md` with build, run and viewer instructions.

**HyRemoteTool** ([source directory](../tools/hyremote-tool)) is the companion access-plane client, not a tutorial
example. It connects to a remote HyRemote-enabled application by explicit IP/port and provides viewing, input and
telemetry; see [`docs/internal/hyremote-tool-plan.md`](../docs/internal/hyremote-tool-plan.md).

## Reference and compatibility

- **Reference matrix:** Windows x86_64 and Linux x86_64, with **Qt 6.8.3**.
- These are the V0.3.0.0 reference cells. A different Qt version is not implied to be supported unless it is recorded in
  [`docs/compatibility.md`](../docs/compatibility.md).
- **Qt 5.15 is not yet qualified** for V0.3.0.0; it remains a later preflight decision.

## Security boundary

The released product is a **LAN-capable** framework:

- the listener is on `0.0.0.0:5921` by default, so it is reachable on this host's IPv4 interfaces;
- remote input is **off** until you explicitly enable it;
- it is for a **trusted LAN only** and is **not** Internet-safe, and there is no encrypted-transport promise yet;
- secure remote access over untrusted networks belongs to a later release.

The deployment and platform-integrity details are in [`docs/known-limitations.md`](../docs/known-limitations.md) and
[`docs/security-model.md`](../docs/security-model.md); the examples do not restate the security architecture.

## Building the examples

Inside the repository build, configure with examples enabled:

```text
PowerShell:  .\build.cmd install --integrations=cpp,qml,generic,qpa --examples --qt-prefix=<qt-prefix>
POSIX:       sh ./build.cmd install --integrations=cpp,qml,generic,qpa --examples --qt-prefix=<qt-prefix>
```

The runnable examples are installed into `build/install/bin/` and run from there with no Qt SDK on `PATH` and no plugin-path variables set. `build/` on its own is developer intermediate output; `build/install/` is the tree to run and to consume as an SDK.

```
```

`HYREMOTE_BUILD_EXAMPLES=ON` is the CMake spelling, and the optional frontends stay explicit
(`HYREMOTE_BUILD_QML_API`, `HYREMOTE_WITH_QPA_PROXY`). A missing optional frontend skips only its own example - it is
never silently substituted by another integration path. The self-authored examples also build standalone against an
installed SDK, which is how an application would consume them.

## Real-world integration studies

The HyRemote-owned examples above are deliberately small. [`real-world/README.md`](real-world/README.md) records the
frozen representative set - KeePassXC, QGroundControl and Shotcut - as **third-party verification, not a HyRemote
compatibility or support claim**. Each study uses the Generic route on pristine upstream sources and records the exact
revision, build route, deployment step and known limits. Upstream sources are never vendored into this repository.

## Further material

- Product overview and architecture: [`../README.md`](../README.md)
- Repository layout authority: [`../docs/internal/repository-layout.md`](../docs/internal/repository-layout.md)
- Release discipline: [`../CONTRIBUTING.md`](../CONTRIBUTING.md)
