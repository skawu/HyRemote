# Troubleshooting

> Language / 语言: **English** | [中文](../../guide/troubleshooting.md)

This guide covers the V1 Embedded C++, Declarative QML and Transparent QPA paths. Start with product-level errors, package metadata and documented policy before investigating internal implementation layers.

## `find_package(HyRemote)` cannot find the package

`find_package(HyRemote CONFIG REQUIRED)` is for an installed HyRemote prefix. Add that prefix to `CMAKE_PREFIX_PATH` or use the source-consumption path from `docs/guide/install.md`.

Do not combine `add_subdirectory(HyRemote)` with an assumption that an installed `HyRemoteConfig.cmake` has been generated.

The installed V1 package exposes one normal C++ target: `HyRemote::RemoteAccess`. Core, the QML backing library and `qhyremote` are not alternate application link targets.

## `RemoteAccess::start()` returns false

Check `lastError()` and verify:

- the target is a live supported `QWidget` or `QQuickWindow`;
- configuration was completed while stopped;
- the selected port is valid and not already occupied;
- the target adapter required by the built product is present;
- the internal VNC correctness transport was not explicitly disabled in a custom source build.

The public error surface intentionally does not expose RFB backend types.

## QML `enabled: true` returns to false

Declarative enable is transactional. During initial QML construction, `enabled: true` is a request; the actual shared-runtime start is deferred until component completion so target/policy bindings can settle.

If startup then fails, `enabled` returns to false. Inspect `errorCode` / `errorString` and verify the bound target is a supported live Quick target and the configured endpoint is available.

Do not add `Component.onCompleted: remote.enabled = true` merely to work around property ordering; normal declarative use should not require that lifecycle glue.

## Viewer cannot connect

Verify the integration mode actually started the service:

- C++: `start()` succeeded;
- QML: `enabled` remained true and state reached Running;
- QPA: the deployed application was launched with `-platform hyremote` rather than the native platform directly.

Also verify the viewer uses the configured loopback address/port, another process is not occupying the port, the process is still running, and a firewall/security product is not interfering with the intended local/trusted connection.

Construction alone never opens the Embedded C++ listener; merely importing the QML module also does not start it.

## Viewer sees the application but input does nothing

Remote input is disabled by default.

- C++: enable `setRemoteInputEnabled(true)` while stopped, then start;
- QML: set `remoteInputEnabled: true` before enabling;
- QPA: relaunch with `-platform hyremote:hyremote-input=true`.

If only specific keys/compositions fail, consult `docs/input-model.md` and `docs/known-limitations.md`; V1 does not claim full IME/dead-key/international-layout parity.

## Pointer coordinates are wrong

Record target logical size, captured framebuffer size, device-pixel ratio and resize state. Coordinate mapping is based on the remote frame/target geometry; stale or unsupported geometry changes must not be papered over as viewer behavior.

For QPA, also record which top-level surface was active and the composite canvas geometry.

## Quick capture is blank or stops while hidden

The public async Quick correctness path depends on a capturable Qt Quick scene/window. Hidden/minimized behavior has explicit handling/limits and is not equivalent to a compositor-level desktop capture service.

Check `docs/compatibility.md`, `docs/known-limitations.md` and the QPA capture classification before broadening the issue into a generic graphics-support claim.

## QPA deploy says the package is unavailable

The installed SDK must have been built with:

```text
-DHYREMOTE_WITH_QPA_PROXY=ON
```

`hyremote_deploy(TARGET ... QPA)` consumes package-owned availability/version/plugin metadata. The installed SDK intentionally does **not** export `HyRemote::QpaPlatform` for applications to link.

## QPA deploy rejects the Qt version

Transparent QPA is private-ABI coupled to **exact Qt 6.8.3** in V1. The application/deployment configure must resolve the same exact Qt version as the qualified QPA payload.

Do not bypass this guard by editing package files or manually copying `qhyremote`; qualify another Qt line explicitly instead.

## Deployed QPA application cannot find `hyremote`

A normal deployed application should contain `qhyremote` under its Qt `plugins/platforms` tree and should not require the original SDK path on `QT_PLUGIN_PATH` or `QT_QPA_PLATFORM_PLUGIN_PATH`.

If the plugin is present only in the SDK staging prefix, treat that as a deployment defect rather than permanently adding the SDK directory to the environment.

## Qt-linked tests fail to launch on Windows

When running **build-tree** tests, ensure the matching Qt `bin` and build-tree `remoteaccess` directory are discoverable on `PATH`.

For an installed/deployed acceptance fixture, the opposite rule applies: remove the original SDK/build path and verify the application runs from its deployment tree.

## Linux build-tree test works only with `LD_LIBRARY_PATH`

Using Qt/build output directories in `LD_LIBRARY_PATH` can be appropriate for direct build-tree developer tests. It is not acceptable evidence for installed deployment.

`hyremote_deploy()` must produce a deployment that resolves the shared facade and plugin/QML payloads without relying on the original HyRemote SDK/build directory.

## Linux test works under offscreen/Xvfb but local desktop behavior is unknown

Headless CI proves only the behavior it actually executes. It does not upgrade local-visible coexistence or a desktop QPA/graphics combination to Supported.

## Viewer disconnect leaves input apparently held

Recognized pressed key/button state is balanced when a viewer disappears abruptly. If a build still reproduces held input after an abrupt viewer loss, capture the exact event sequence and treat it as a regression.

## Slow or malicious clients

The Core frame mailbox, RFB frame handoff, GUI input delivery and incomplete-handshake lifetime are bounded by design. If memory/work still grows without bound, treat it as a product defect and record the exact client traffic/reproduction; do not raise queue capacities as a substitute for fixing ownership/backpressure.

## Security warning

The current RFB SecurityType None baseline is unauthenticated and unencrypted. Do not expose a listener directly to an untrusted/public network merely to test connectivity. See `docs/security.md`.
