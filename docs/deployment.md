# Deployment

HyRemote owns deployment of its product runtime and integration payloads instead of requiring application developers to discover internal backend files manually.

## One installed-package entry point

The installed SDK exposes one application-facing helper with orthogonal mode options:

```cmake
hyremote_deploy(TARGET MyWidgetsOrCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

These forms extend one deployment contract; they do not create separate runtime architectures.

- ordinary C++/Widgets/Quick applications use Qt's supported normal application deployment path;
- `QML` selects Qt's QML-aware deployment path and bridges the installed HyRemote QML import root into Qt's scanner/deployment machinery;
- `QPA` adds the exact-version HyRemote platform proxy to the application's Qt platform-plugin deployment directory while preserving Qt ownership of the native `qwindows` / `qxcb` delegate deployment;
- `QML QPA` deploys both package payloads when an application deliberately needs both, without changing the semantics of either integration mode.

Applications must not copy or select RFB implementation files, QML plugin files, `qmldir`, or QPA private runtime files by name.

## Transparent QPA deployment

The current QPA package is version-coupled to exact Qt 6.8.3. The installed package records QPA availability and the qualified Qt version. `hyremote_deploy(... QPA)` fails clearly when:

- the installed HyRemote SDK was built without the QPA package; or
- the consumer Qt version does not match the qualified private-ABI line.

The helper uses Qt's deploy-time plugin/runtime locations rather than a hard-coded Qt SDK path. A normal deployed application therefore should not require `QT_PLUGIN_PATH` merely to find HyRemote's platform plugin. Manual plugin-path overrides remain a source-tree/debugging technique, not the normal installed-product workflow.

See `docs/getting-started/qpa-proxy.md` for the zero/minimal-source-change user path.

## QML deployment

For an installed declarative application, install the executable first and then call:

```cmake
hyremote_deploy(TARGET MyQmlApp QML)
```

The helper preserves application-owned QML import paths and appends the installed HyRemote import root before Qt prepares its deployment metadata. It never calls both the QML and non-QML high-level Qt deployment generators for the same application.

See `docs/qml-consumption.md` and `docs/getting-started/qml.md`.

## Runtime dependencies

Deploy the Qt modules required by the application and by the selected HyRemote package. The installed HyRemote CMake configuration records its public Qt dependencies.

For QPA shared-library builds, the helper also classifies the HyRemote runtime libraries required by `qhyremote` as additional deployment libraries. The target application still does not need to link `HyRemote::RemoteAccess` merely to use Transparent QPA.

The bounded RFB correctness transport remains an internal implementation detail. Changing the transport backend must not change the application-facing QPA launch/deployment contract.

## Security and network configuration

Deployment does not weaken the product defaults:

- Embedded C++/QML construction is inert; explicit start/enable is required;
- QPA listener creation follows the documented platform-plugin lifecycle rather than plugin discovery alone;
- loopback is the safe/default bind;
- remote input is disabled by default;
- the current RFB SecurityType None baseline provides neither viewer authentication nor transport encryption.

If an application intentionally changes the bind address, its operator/deployment documentation must describe the resulting trust boundary. See `docs/security.md`.

## Acceptance boundary

The repository contains installed-prefix/source-consumer fixtures for the C++ SDK, QML deployment and QPA deployment. Release support still requires those exact workflows to execute successfully on the claimed Windows x86_64 and Linux x86_64 reference environments.

Current hosted jobs are blocked before runner assignment by #74. That infrastructure failure is not release evidence, so no milestone release branch/tag may be created from documentation or unexecuted deployment code alone.
