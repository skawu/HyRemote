# HyRemote V1 release package manifest

Status: **release-candidate contract; final payload acceptance pending**

This document defines the intended V1 installed SDK/product payload. The package is deliberately small: ordinary users get one C++ product library, an optional QML module, and an optional QPA platform plugin.

## Always-present package metadata

An installed HyRemote SDK contains:

- `HyRemoteConfig.cmake` and version/config export metadata;
- `HyRemoteTargets.cmake` with the application-facing product target metadata;
- `HyRemoteDeploy.cmake` / the `hyremote_deploy()` entry point;
- Apache-2.0 `LICENSE`;
- `NOTICE.md`;
- release/package documentation shipped by the release process.

## Fixed V1 runtime shape

V1 deliberately avoids a user-visible static/shared or backend-target matrix:

- `HyRemote::RemoteAccess` is the normal **shared** C++ product library;
- `hyremote-core` is a **static source/internal composition library** behind `RemoteAccess` and is **not installed/exported** in the normal V1 SDK;
- `qhyremote` is a Qt platform **MODULE payload** when Transparent QPA is enabled; the installed SDK **does not export `HyRemote::QpaPlatform`** as a consumer CMake target;
- the `HyRemote` QML module is a thin declarative layer over the same shared `RemoteAccess` runtime and its backing library is not a second consumer C++ target.

`BUILD_SHARED_LIBS` does not change the normal V1 application artifact contract.

## Embedded C++ product facade

The installed SDK exports:

- `HyRemote::RemoteAccess`;
- `HyRemote/RemoteAccess.h` and its supported public application types;
- the shared `HyRemoteRemoteAccess` runtime payload.

Normal applications link only `HyRemote::RemoteAccess`. They must not need to know `Session`, `CaptureSource`, `InputSink`, transport, RFB, Core, QPA plugin, or target-adapter implementation filenames or targets.

Normal deployment is one call:

```cmake
hyremote_deploy(TARGET MyApp)
```

The helper owns placement of the shared facade and its Qt dependencies.

## Declarative QML payload

When the QML API is enabled, the package additionally contains the normal Qt QML module payload for URI `HyRemote`. That module wraps the same shared C++ runtime; it is not a separate backend stack or a separately selected C++ library.

Application packaging uses:

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

The consumer should not copy `qmldir`, QML plugin files or `HyRemoteRemoteAccess` manually.

## Transparent QPA payload

When the exact-version QPA package is enabled, the SDK contains the qualified `qhyremote` platform plugin and package metadata required by:

```cmake
hyremote_deploy(TARGET ExistingQtApp QPA)
```

or:

```cmake
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

The application does not link a HyRemote QPA target. `find_package(HyRemote)` publishes QPA availability/version/payload metadata internally to the deployment helper, while the target application remains Qt-only at link level.

The deployed HyRemote payload is intentionally bounded to the QPA module plus the same shared `RemoteAccess` runtime. QPA is version-coupled to the qualified Qt private ABI and must not imply generic Qt-private compatibility. Native `qwindows` / `qxcb` deployment remains Qt-owned. Normal deployed applications must not require manually configured `QT_PLUGIN_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH` or SDK-specific runtime search paths.

The combined `QML QPA` form is a first-class deployment composition, not a fourth runtime architecture and not something release verification may infer from two independent successful consumers. The clean combined fixture must load the deployed `HyRemote` QML module while the transparent QPA plugin owns the one active remote listener/runtime, and must do so without SDK/plugin/QML path overrides.

## Source-tree internal architecture is not an installed product API

The repository still contains the Qt-free Core architecture, QPA build target and their tests/headers. Those source-level contracts support HyRemote implementation and future backend work, but their presence in the repository does not create extra V1 application integration paths or installed library choices.

The following remain repository development or acceptance assets unless a release artifact explicitly includes them as documentation/source material:

- `src/core/` low-level implementation headers and tests;
- the source-tree/internal `HyRemote::Core` and `HyRemote::QpaPlatform` aliases;
- `examples/`;
- `verification/` and product-fit harnesses;
- `.github/workflows/`;
- aqtinstall, vncdotool, Pillow, Ninja and GitHub Actions tooling;
- maintainer documentation under `docs/internal/` (evaluation records, runbooks);
- QPA test clients and test-only RFB decoders;
- private Widgets/Quick adapters and QPA interception/composite/controller/provider classes.

Their presence in the source repository does not make them stable application-facing 1.x APIs.

Source/add_subdirectory consumption nevertheless uses the same shared product artifacts and the same `hyremote_deploy()` entry point. On Linux, build-tree shared artifacts that the helper may copy directly must contain bounded origin-relative deployment lookup entries; a source deployment is not considered clean merely because the original build tree or Qt SDK happens to remain reachable through an embedded absolute RUNPATH.

## License and notices

The installed SDK must preserve the project Apache-2.0 `LICENSE` and `NOTICE.md`. Qt and other separately obtained third-party dependencies remain under their own terms; HyRemote's Apache-2.0 license does not replace them.

Any future bundled/copied third-party payload must update notice/license material deliberately before release acceptance.

## Release verification

A release candidate must verify at minimum:

1. package config/export files install correctly;
2. project license and notice install correctly;
3. the installed product export contains `HyRemote::RemoteAccess` and exposes neither `HyRemote::Core` nor `HyRemote::QpaPlatform` as alternate application targets;
4. clean installed C++ consumers build and deploy with one `HyRemote::RemoteAccess` target plus one `hyremote_deploy()` call;
5. deployed C++ applications run without the original SDK runtime path;
6. source/add_subdirectory consumers deploy the same shared product runtime without requiring original build-tree runtime lookup;
7. QML/QPA payloads appear only when their build options are enabled;
8. QPA deployment resolves its package-owned plugin payload without a consumer link target, contains the exact module + shared facade, preserves the exact Qt ABI boundary and requires no manual plugin-path override;
9. `hyremote_deploy(... QML QPA)` is configured, installed and executed as its own clean-consumer case on both reference operating systems, proving the deployed QML module + qhyremote + one shared runtime coexist in one application tree;
10. no Core/runtime/backend filename knowledge leaks into the normal application contract;
11. no examples/tests/CI-only dependencies are accidentally classified as mandatory runtime payload.

The exact accepted payload is frozen only on the accepted release branch/main commit that receives the milestone tag.
