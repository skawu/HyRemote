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
- repository-owned release/package documentation installed or shipped by the release packaging process.

## Core SDK target

When Core is built, the installed SDK exports:

- `HyRemote::Core`;
- public Core headers and library payload required by that low-level SDK target.

`HyRemote::Core` is available for framework/package composition and advanced consumers. It is **not** a second normal application integration mode and does not replace `HyRemote::RemoteAccess` for ordinary product integration.

## Embedded C++ product facade

When RemoteAccess is built, the installed SDK exports:

- `HyRemote::RemoteAccess`;
- `HyRemote/RemoteAccess.h` and its supported public application types;
- the runtime/library payload required by the configured build.

Normal applications must not need to know Session, CaptureSource, InputSink, transport, RFB, or target-adapter implementation filenames.

## Declarative QML payload

When the QML API is enabled, the package additionally contains the normal Qt QML module payload for URI `HyRemote`, including the package metadata/plugin/runtime files produced by the supported Qt QML packaging path.

Application packaging uses:

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

The consumer should not copy `qmldir` or plugin files manually as part of the normal installed-SDK workflow.

## Transparent QPA payload

When the exact-version QPA package is enabled, the SDK contains the HyRemote platform-proxy payload and package metadata required by:

```cmake
hyremote_deploy(TARGET ExistingQtApp QPA)
```

or, for a declarative application that also uses Transparent QPA:

```cmake
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

The QPA payload is separately version-coupled to the qualified Qt private ABI. It must not make a generic Qt-private compatibility promise. Native `qwindows` / `qxcb` deployment remains Qt-owned; HyRemote owns only its proxy/runtime payload.

Normal deployed applications must not require a manually configured `QT_PLUGIN_PATH` to discover the HyRemote proxy.

## Conditional shared-library runtime payload

Shared builds may require HyRemote runtime libraries in the application deployment. The deployment helper owns that package/runtime integration. Applications must not encode repository build-tree paths or manually infer backend library names.

Static builds naturally have a different runtime-file shape; the public CMake/application contract remains the same.

## Source-tree evidence that is not an installed runtime API

The following are repository development or acceptance assets unless a release artifact explicitly includes them as documentation/source material:

- `examples/`;
- `tests/` and product-fit harnesses;
- `.github/workflows/`;
- aqtinstall, vncdotool, Pillow, Ninja, GitHub Actions tooling;
- architecture spikes under `spikes/`;
- QPA test clients and test-only RFB decoders;
- private Widgets/Quick adapters and QPA interception/composite/controller/provider classes.

Their presence in the source repository does not make them stable application-facing 1.x APIs.

## License and notices

The installed SDK must preserve the project Apache-2.0 `LICENSE` and `NOTICE.md`. Qt and any other separately obtained third-party dependencies remain under their own terms; HyRemote's Apache-2.0 license does not replace them.

Any future bundled/copied third-party payload must update the notice/license material deliberately before a release is accepted.

## Release verification

A release candidate must verify at minimum:

1. package config/export files install correctly;
2. the project license and notice install correctly;
3. public headers/targets match the frozen V1 API surface;
4. clean installed-SDK consumers build without source-tree knowledge;
5. QML/QPA payloads appear only when their build options are enabled;
6. QPA deployment keeps the exact Qt ABI boundary and requires no manual plugin-path override in the normal deployed path;
7. no examples/tests/CI-only dependencies are accidentally classified as mandatory runtime payload.

The exact accepted payload is frozen only on the accepted release branch/main commit that receives the milestone tag.
