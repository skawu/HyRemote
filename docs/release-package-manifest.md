# HyRemote V1 release package manifest

Status: **release-candidate contract; final payload acceptance pending**

This document defines the intended V1 installed SDK/product payload by category. It describes product/package boundaries, not private implementation filenames that applications should depend on.

## Always-present package metadata

An installed HyRemote SDK contains the applicable subset of:

- `HyRemoteConfig.cmake` and version/config export metadata;
- `HyRemoteTargets.cmake` and exported CMake target metadata;
- `HyRemoteDeploy.cmake` / the `hyremote_deploy()` entry point;
- Apache-2.0 `LICENSE`;
- `NOTICE.md`;
- release/package documentation shipped by the release process.

## Fixed V1 runtime shape

V1 deliberately avoids a user-visible static/shared matrix:

- `HyRemote::RemoteAccess` is the normal **shared** product library;
- `hyremote-core` is a **static** internal composition library behind `RemoteAccess`;
- `qhyremote` is a Qt platform **MODULE** when Transparent QPA is enabled;
- the `HyRemote` QML module is a thin declarative layer over the same shared `RemoteAccess` runtime.

`BUILD_SHARED_LIBS` does not change the normal V1 application artifact contract.

## Core SDK target

When Core is built, the installed SDK may export `HyRemote::Core` and its public low-level headers for framework/package composition and advanced users. The library itself is static in V1.

`HyRemote::Core` is **not** a second normal application integration mode and is not a separately deployed runtime dependency for ordinary C++/QML/QPA applications.

## Embedded C++ product facade

When RemoteAccess is built, the installed SDK exports:

- `HyRemote::RemoteAccess`;
- `HyRemote/RemoteAccess.h` and its supported public application types;
- the shared `HyRemoteRemoteAccess` runtime payload.

Normal applications link only `HyRemote::RemoteAccess` and must not need to know Session, CaptureSource, InputSink, transport, RFB, Core, or target-adapter implementation filenames.

Normal deployment is:

```cmake
hyremote_deploy(TARGET MyApp)
```

The helper owns placement of the shared facade and its Qt dependencies.

## Declarative QML payload

When the QML API is enabled, the package additionally contains the normal Qt QML module payload for URI `HyRemote`. That module wraps the same shared C++ runtime; it is not a separate backend stack.

Application packaging uses:

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

The consumer should not copy `qmldir`, QML plugin files or `HyRemoteRemoteAccess` manually.

## Transparent QPA payload

When the exact-version QPA package is enabled, the SDK contains the HyRemote platform-proxy payload and metadata required by:

```cmake
hyremote_deploy(TARGET ExistingQtApp QPA)
```

or:

```cmake
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

The deployed HyRemote payload is intentionally bounded to the QPA module plus the same shared `RemoteAccess` runtime. The target application itself stays Qt-only at link level.

QPA is version-coupled to the qualified Qt private ABI and must not imply generic Qt-private compatibility. Native `qwindows` / `qxcb` deployment remains Qt-owned. Normal deployed applications must not require manually configured `QT_PLUGIN_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH` or SDK-specific runtime search paths.

## Source-tree evidence that is not an installed runtime API

The following remain repository development or acceptance assets unless a release artifact explicitly includes them as documentation/source material:

- `examples/`;
- `tests/` and product-fit harnesses;
- `.github/workflows/`;
- aqtinstall, vncdotool, Pillow, Ninja and GitHub Actions tooling;
- architecture spikes under `spikes/`;
- QPA test clients and test-only RFB decoders;
- private Widgets/Quick adapters and QPA interception/composite/controller/provider classes.

Their presence in the source repository does not make them stable application-facing 1.x APIs.

## License and notices

The installed SDK must preserve the project Apache-2.0 `LICENSE` and `NOTICE.md`. Qt and other separately obtained third-party dependencies remain under their own terms; HyRemote's Apache-2.0 license does not replace them.

Any future bundled/copied third-party payload must update notice/license material deliberately before release acceptance.

## Release verification

A release candidate must verify at minimum:

1. package config/export files install correctly;
2. project license and notice install correctly;
3. public headers/targets match the frozen V1 API surface;
4. clean installed C++ consumers build and deploy with one `HyRemote::RemoteAccess` target plus one `hyremote_deploy()` call;
5. deployed C++ applications run without the original SDK runtime path;
6. QML/QPA payloads appear only when their build options are enabled;
7. QPA deployment contains the exact module + shared facade, preserves the exact Qt ABI boundary and requires no manual plugin-path override;
8. no Core/runtime/backend filename knowledge leaks into the normal application contract;
9. no examples/tests/CI-only dependencies are accidentally classified as mandatory runtime payload.

The exact accepted payload is frozen only on the accepted release branch/main commit that receives the milestone tag.
