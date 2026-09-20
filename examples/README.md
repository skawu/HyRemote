# HyRemote V1 examples

This directory is the canonical example matrix for the V1.0.0.0 product surface. Examples are release evidence, not alternate product architectures.

| ID | Example / fixture | Integration mode | Product surface | Release role |
| --- | --- | --- | --- | --- |
| E1 | `widgets-basic` | Embedded C++ / Qt Widgets | `HyRemote::RemoteAccess` | Basic Widgets attach/view/input/reconnect evidence for V0.0.1.0 |
| E2 | `quick-basic` | Embedded C++ / Qt Quick | `HyRemote::RemoteAccess` | Basic Quick attach/view/input/reconnect evidence for V0.0.1.0 |
| E3 | `qml-basic` | Declarative QML | `import HyRemote` over the shared runtime | Declarative product E2E for V0.0.2.0 |
| E4 | `qpa-proxy-existing-app` | Transparent QPA Proxy | Ordinary Qt app; HyRemote supplied by deployed QPA package | Existing-app / zero application-code integration evidence for V0.0.3.0 |
| E5 | `remote-support-showcase` | C++ API / Qt Widgets | `HyRemote::RemoteAccess` | Production-like operator workflow and explicit policy controls for V1.0.0.0 |
| E6 | `../tests/consumer-installed-sdk` | Installed SDK consumer | `find_package(HyRemote)` + exported targets / `hyremote_deploy()` | External clean-consumer SDK contract; intentionally not duplicated under `examples/` |

## Frozen rules

- E1, E2 and E5 use the public `HyRemote::RemoteAccess` facade only. They must not assemble Core, Session, CaptureSource, InputSink, Transport, RFB or platform-backend objects.
- E3 uses the declarative `HyRemote` module over the same runtime; it must not create a second QML-specific transport/session stack.
- E4 application source remains ordinary Qt-only code. QPA is a deployed platform-plugin/runtime concern, not an application link dependency.
- E6 validates the installed SDK from a clean consumer and remains the canonical external-consumer fixture from #43 / PR #46.
- Construction remains inert for C++ API/QML, listener defaults remain loopback-safe, and remote input remains disabled until explicitly enabled.
- `SecurityType None` is a correctness baseline only; examples must not present it as authenticated, encrypted or Internet-safe.
- Hosted/headless viewer tests do not prove physical local-display/local-input coexistence. Where a milestone requires that evidence, the milestone stays open until the real acceptance run exists.

## Build graph

With a suitable Qt SDK, configure HyRemote with `HYREMOTE_BUILD_EXAMPLES=ON`. Optional product modes remain explicit:

- `HYREMOTE_BUILD_QML_API=ON` enables E3;
- `HYREMOTE_WITH_QPA_PROXY=ON` builds the QPA runtime, while E4 itself remains an ordinary Qt application;
- E4's installed-package deployment path is enabled in that example with `HYREMOTE_EXAMPLE_DEPLOY_QPA=ON`.

A missing optional mode causes only its dependent example to be skipped. It does not silently substitute another integration path.

## Acceptance and tags

These examples participate in milestone acceptance but do not authorize a release by themselves. Git Flow release order is:

1. accepted feature work converges into `develop`;
2. after every gate for the milestone is satisfied, cut `release/vMajor.Minor.Feature.Maintenance` from the accepted `develop` integration point;
3. run release-candidate build/test/acceptance without adding unrelated features;
4. merge the accepted release branch into `main`;
5. create the matching `vMajor.Minor.Feature.Maintenance` tag on that accepted `main` commit;
6. merge the release result back into `develop` before later feature work diverges.

Do not create a milestone tag while executable evidence is blocked or while required physical acceptance is missing.
