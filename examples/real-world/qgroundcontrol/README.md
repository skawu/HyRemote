# QGroundControl - Qt Quick / QML study

A large, actively maintained Qt Quick/QML ground-control application. It is the case for the **QML route**: the UI is
declarative, and the application already links its own QML modules, so `import HyRemote` plus the deployment helper is
the least intrusive honest integration.

```text
UPSTREAM_REPOSITORY=github.com/mavlink/qgroundcontrol
UPSTREAM_REVISION=2b7e55dea40ed88d879029dc6871674d78e666f3   (master, 2026-09-08, "test(Camera): ignore connect-time StandardModes warning in lost camera test")
UPSTREAM_LICENSE=Apache-2.0
STAR_COUNT_AT_SELECTION=4974   (forks 5067, read from the GitHub API 2026-09-23)
MAINTAINED=yes (pushed 2026-09-23, not archived)
UI_FAMILY=Qt Quick / QML
INTEGRATION_ROUTE=QML API (+ C++ link of HyRemote::RemoteAccess; the QML module is a wrapper over the same runtime)
UPSTREAM_BUILD_REQUIREMENTS=CMake 3.25+, Qt 6 (minimum enforced by upstream's own config), submodules, and an upstream-fetched Vulkan SDK (GIT_TAG vulkan-sdk-1.4.341.0)
```

## Why this route

QGC is QML-first: its application shell is declarative. The QML route lets the integration live where the UI lives,
without wrapping the whole application in an imperative lifecycle the project does not otherwise use. The four HyRemote
routes are peers, so this is a fit choice, not a capability difference - both reach the same shared runtime.

The main application target is `${CMAKE_PROJECT_NAME}` (`project(...)` at the root, with its sources and links in
`src/CMakeLists.txt`), and the C++ entry point is `QGCApplication app(argc, argv, args);` in `src/main.cc`.

## What the patch changes

One file, `src/CMakeLists.txt`, immediately after an existing library link into the main application target:

```text
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE HyRemote::RemoteAccess)
hyremote_deploy(TARGET ${CMAKE_PROJECT_NAME} QML)
```

- **Which `CMakeLists.txt` was changed:** `src/CMakeLists.txt` (inserted immediately after
  `target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE AutoPilotPluginsAPMModule)`).
- **Which `find_package(HyRemote ...)` was added:** `find_package(HyRemote CONFIG REQUIRED)`.
- **Which target is linked:** `HyRemote::RemoteAccess` (the QML module is backed by the same shared runtime; there is no
  second runtime to link).
- **Why linking is still needed on the QML route:** the declarative wrapper lives in the HyRemote QML plugin, which is
  backed by the shared runtime library; linking the public target is what makes the deployment helper and the runtime
  closure coherent.
- **How `hyremote_deploy()` is executed:** `hyremote_deploy(TARGET ${CMAKE_PROJECT_NAME} QML)` at install time - the
  `QML` keyword is what deploys `qml/HyRemote/` next to the executable.
- **Window object the remote access is created on:** the QGC root application window. In QML this is an object
  declaration targeting that window:

```qml
import HyRemote

HyRemote.RemoteAccess {
    target: rootWindow        // the QGC application window in the checked-out revision
    remoteInputEnabled: true
}
```

  The **exact QML file that declares the root window is upstream-version dependent** and is deliberately not asserted
  here: confirm it in the revision you checked out before applying the snippet. This study records the integration
  mechanics, not a claim about a specific upstream file name.
- **Application code change:** one import plus a four-line object declaration at the root window, plus the three CMake
  lines. No upstream module is restructured.
- **Runnable tree / how to launch:** the install prefix; launch the deployed `QGroundControl` executable, not the build
  tree one.

## Verification actually performed, and its limit

```text
PATCH_APPLIES=PASS      git apply --check on the pinned revision 2b7e55d -> exit 0
CMAKE_INTEGRATION=REAL  the targets and the helper referenced by the patch were verified against the
                        installed SDK: HyRemote::RemoteAccess is a real imported target, and
                        hyremote_deploy(TARGET ... QML) is a real function provided by the SDK's
                        HyRemoteDeploy.cmake
CONFIGURE=NOT COMPLETED  an upstream configure was started on this host and did not complete within this
                        slice; no result is claimed. Upstream requires CMake 3.25+, submodules and an
                        upstream-fetched Vulkan SDK in addition to Qt, so a full build is a
                        substantially larger operation than the consumer walkthrough in this slice
BUILD=NOT REACHED
LISTENER=NOT REACHED
VIEWER_CONNECT=NOT REACHED
```

**This study was not taken to a remote-viewable QGroundControl window.** It is deliberately reported as such: the patch
is verified to apply to the pinned revision and to reference real HyRemote targets and helper functions, and the
blocking dependency set is recorded rather than glossed over. No statement here should be read as "it should work".

## How a viewer would connect (once built)

Same three facts as every other route, which is the point of the peer design:

1. launch the deployed `QGroundControl` executable;
2. confirm the listener: `Get-NetTCPConnection -LocalPort 5921 -State Listen` (Windows) or `ss -ltnp | grep 5921`
   (Linux);
3. connect any standard VNC/RFB viewer to `<HOST_LAN_IP>:5921`.

Default facts are unchanged: listener `0.0.0.0:5921`, authentication off, transport encryption off, remote input off
until enabled, trusted LAN only, not Internet-safe.

## Known limitations

- Apache-2.0 upstream: a distributed patched build stays under Apache-2.0 with its attribution requirements.
- No upstream branding is changed, and this is not an endorsement by the QGC project.
- The root-window QML file name is upstream-version dependent and must be confirmed per revision.
