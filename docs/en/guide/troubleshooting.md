# Troubleshooting

> Language / 语言: **English** | [中文](../../guide/troubleshooting.md)

This guide covers all four product paths: C++ API, Generic Plugin, QML API, and QPA. Start with product-level errors, deployed payloads, and the compatibility matrix instead of internal implementation details.

## `find_package(HyRemote)` cannot find the package

`find_package(HyRemote CONFIG REQUIRED)` is for an installed SDK.

Check that:

- the HyRemote install prefix is on `CMAKE_PREFIX_PATH`;
- the application is not mixing an installed SDK with `add_subdirectory(HyRemote)` in the same configure;
- the HyRemote SDK matches the intended Qt/toolchain environment.

See [`install.md`](install.md) for source consumption.

## `RemoteAccess::start()` returns false

Inspect `lastError()` and verify:

- the target is a live supported `QWidget` or `QQuickWindow`;
- configuration was completed while the Runtime was stopped;
- the selected port is valid and not occupied;
- the listener address is a valid numeric IP address;
- the selected security profile is configured correctly;
- `AuthenticatedEncrypted` is not being treated as an implemented V0.1 capability.

Selecting `AuthenticatedEncrypted` in V0.1 intentionally fails with security unavailable and opens no listener.

## QML `enabled: true` returns to false

QML enablement is an explicit request. If Runtime startup fails, `enabled` returns to false.

Check:

- the `target` is a valid Quick top-level window;
- address/port are available;
- security policy allows startup;
- product-level `errorCode` / `errorString`.

Normal usage should not require `Component.onCompleted` startup glue.

## Generic Plugin does not activate

First confirm deployment used:

```cmake
hyremote_deploy(TARGET MyApp GENERIC)
```

Then activate HyRemote through Qt's generic-plugin mechanism, for example:

```text
MyApp -plugin hyremote
```

If the plugin is not loaded, inspect the deployed Generic Plugin directory and Qt plugin search path.

A defining Generic property is that the application's **native Qt platform identity is preserved**. If the application unexpectedly runs on a platform named `hyremote`, that is not the Generic route.

## Viewer cannot connect

Verify the selected frontend actually started HyRemote:

- C++: `start()` succeeded;
- Generic: the Generic Plugin loaded successfully;
- QML: `enabled` remains true and state reaches `Running`;
- QPA: the application was launched with `-platform hyremote`.

Also check:

- viewer address/port;
- default address is `0.0.0.0:5921`;
- no other process owns the port;
- the application is still running;
- firewall/security software is not blocking the intended connection.

## Viewer can see the application but input does nothing

Remote input is disabled by default.

- C++: `setRemoteInputEnabled(true)`;
- QML: `remoteInputEnabled: true`;
- Generic: set `input=true` in the Generic specification;
- QPA: `-platform "hyremote:hyremote-input=true"`.

If only specific keys, IME behavior, or shortcuts fail, see [`../../input-model.md`](../../input-model.md) and [`../../known-limitations.md`](../../known-limitations.md).

## Viewer connects but the image is blank or incomplete

Confirm the application target belongs to the current compatibility scope:

- QWidget top-level;
- QQuickWindow;
- or a complex combination explicitly listed as compatible.

Do not infer support for QOpenGLWidget, QQuickWidget, Quick3D, custom FBOs, or foreign/native windows from basic Widgets/Quick support.

See [`../../compatibility.md`](../../compatibility.md).

## Pointer coordinates are wrong

Record:

- target logical size;
- frame pixel size;
- device-pixel ratio;
- scaling state;
- window geometry before/after resize.

For QPA/automatic-surface composition, also inspect the remote canvas and top-level surface geometry.

## Input looks stuck after viewer disconnect

HyRemote cleans up recognized held key/button state when a remote peer disappears or when the Runtime stops.

If the issue is reproducible, record:

- viewer and version;
- key/button sequence;
- whether disconnect was abrupt;
- whether a second viewer was connected;
- application target type.

Treat persistent stuck input as a product defect rather than compensating in application code.

## QPA deployment says the payload is unavailable

Confirm the HyRemote SDK was built with:

```text
-DHYREMOTE_WITH_QPA_PROXY=ON
```

Also confirm the consumer uses **exact Qt 6.8.3** and the matching private Gui development components.

QPA does not export an application link target such as `HyRemote::QpaPlatform`; the application remains Qt-only.

## QPA rejects the Qt version

This is intentional fail-closed behavior.

QPA uses Qt private ABI. The current reference is **Qt 6.8.3 exact**. Do not bypass the version boundary by manually copying `qhyremote` or editing package metadata.

Qualify another Qt private-ABI line before using it as a product combination.

## Deployed application cannot find a HyRemote plugin/runtime

A normal deployment should run from the application's own tree rather than the original SDK.

Do not permanently “fix” deployment by:

- pointing `QT_PLUGIN_PATH` back to the Qt/HyRemote SDK;
- pointing `QT_QPA_PLATFORM_PLUGIN_PATH` back to a build directory;
- pointing `LD_LIBRARY_PATH` back to the HyRemote build tree;
- manually copying internal Core/transport/capture files.

Recheck the applicable `hyremote_deploy()` call in [`deployment.md`](deployment.md).

## Windows cannot find Qt/HyRemote DLLs

Confirm the application was deployed, not merely compiled.

An installed/deployed application should resolve from its own tree:

- Qt runtime;
- `HyRemoteRemoteAccess`;
- the relevant Generic/QPA/QML payload.

Do not permanently add the developer Qt `bin` directory to the product environment to hide missing deployment files.

## Linux only runs with `LD_LIBRARY_PATH`

Temporarily using `LD_LIBRARY_PATH` can be useful while debugging a build tree, but a product deployment should not rely on the original HyRemote/Qt SDK path.

If the deployed application fails once the build tree is removed, treat it as a deployment problem.

## Headless/offscreen works but native desktop behavior is unknown

Headless/offscreen execution proves only the path it actually runs.

It does not automatically prove:

- physical display behavior;
- local keyboard/mouse behavior;
- all native QPA delegate behavior;
- special GPU/rendering paths.

> **TODO V0.4:** complete final physical Windows/Linux local + remote coexistence qualification.

## Slow clients cause resource growth

HyRemote frame handoff, transport, and input paths are designed to be bounded.

If a slow client can make memory, queue depth, or work grow without bound, treat that as a product defect. Do not merely increase queue sizes to hide backpressure/ownership problems.

## Security-related startup problems

V0.1 does not provide encrypted RFB traffic.

If you see:

- non-loopback startup being refused;
- `AuthenticatedEncrypted` failing to start;
- viewer password behavior not matching expectations;

read [`../../security.md`](../../security.md) first.

Do not expose the current product directly to the public Internet just to test connectivity.

## Still cannot isolate the problem

Record at least:

- HyRemote version/commit;
- exact Qt version;
- OS/architecture;
- integration frontend;
- Widgets / Quick target type;
- viewer and version;
- startup arguments;
- `lastError()` / logs;
- source build or installed SDK;
- whether `hyremote_deploy()` was used.

This keeps diagnosis at the product boundary before investigating internal modules.