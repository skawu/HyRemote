# QGroundControl v5.0.6 - Qt Quick/QML study (Qt 6 lane)

```text
UPSTREAM_REPOSITORY=github.com/mavlink/qgroundcontrol
UPSTREAM_REVISION=v5.0.6   (pinned release tag; upstream's latest release at selection is v5.1.4 - the set pins v5.0.6 deliberately)
UPSTREAM_LICENSE=Apache-2.0 (LICENSE-APACHE; the tree also carries LICENSE-GPL)
STAR_COUNT_AT_SELECTION=4974   (read from the GitHub API 2026-09-23)
MAINTAINED=yes (pushed 2026-09-23, not archived)
UI_FAMILY=Qt Quick / QML (C++)
INTEGRATION_ROUTE=GENERIC
UPSTREAM_QT_LANE=Qt 6 - find_package(Qt6 ...) in the pinned CMakeLists.txt; cmake_minimum_required(VERSION 3.25)
UPSTREAM_SOURCE_PATCHES_FOR_HYREMOTE=0
```

## Why GENERIC here

QGC is a large Qt Quick application. It is in the frozen set as the **QML-heavy** representative, and the honest way to
reach it is the zero-code route: the application keeps its own QML, its own build and its own branding, and remote access
arrives at run time as a Qt generic plugin.

The frozen set's `PRIMARY_ROUTE=GENERIC` applies here exactly as it does to the Widgets representative. **Nothing is
added to QGC's source**: no `find_package(HyRemote)`, no `HyRemote::RemoteAccess` link, no `hyremote_deploy()`, no patch.

## How the integration works (no upstream change)

```text
original QGC application (obtained the way upstream says to: their installer or their own build)
  -> deploy the Generic payload from the installed HyRemote SDK
  -> QGroundControl -plugin hyremote
  -> listener 0.0.0.0:5921
  -> connect a viewer
```

```powershell
# Windows PowerShell
$env:QT_PLUGIN_PATH = "$env:HYREMOTE_SDK_ROOT/plugins"       # generic/libqhyremote.dll
$env:PATH           = "$env:HYREMOTE_SDK_ROOT/bin;$env:PATH"  # libHyRemoteRemoteAccess.dll
QGroundControl.exe -plugin hyremote
```

```sh
# POSIX
export QT_PLUGIN_PATH="$HYREMOTE_SDK_ROOT/plugins"
export LD_LIBRARY_PATH="$HYREMOTE_SDK_ROOT/bin:$LD_LIBRARY_PATH"
./QGroundControl -plugin hyremote
```

Then confirm the listener (`Get-NetTCPConnection -LocalPort 5921 -State Listen`, or `ss -ltnp | grep 5921`) and connect a
standard VNC/RFB viewer to `<HOST_LAN_IP>:5921`.

Defaults are unchanged by the integration and are the product's own: `0.0.0.0:5921`, authentication off, transport
encryption off, remote input **off** until enabled, trusted LAN only, not Internet-safe.

## Verification actually performed, and what is missing

```text
UPSTREAM_PINNED_TAG_EXISTS=PASS      tag v5.0.6 exists upstream
QT_LANE_READ_FROM_UPSTREAM=PASS      find_package(Qt6 ...) and cmake_minimum_required(VERSION 3.25) in the pinned tree
QT_LANE_ON_REFERENCE=PASS            Qt 6 sits on the current V0.3.0 reference lane (Qt 6.8.3)
LICENCE_FILES_READ=PASS              LICENSE-APACHE (plus LICENSE-GPL for parts)
UPSTREAM_PATCH=0
GENERIC_MECHANISM_ON_CURRENT_LANE=PASS   verified with a pristine Qt 6.8.3 application, zero HyRemote code:
                                         process alive, listening 0.0.0.0:5921, application log
                                         "HyRemote automatic application access active on \"0.0.0.0\" 5921
                                          remote input: false security profile: insecure"
GENERIC_RUN_ON_QGC_ITSELF=NOT PERFORMED  obtaining or building QGC is a large third-party operation
                                         (upstream needs CMake 3.25+, submodules and an upstream-fetched
                                         Vulkan SDK); it was not carried out in this slice, so no
                                         listener/viewer result is claimed for QGC itself
VIEWER_CONNECT=NOT REACHED ON QGC ITSELF
```

This is stated plainly rather than softened: the *mechanism* is proven on the current lane with a pristine Qt
application, and the *application-specific* run is not done here. No statement in this file means "it should work".

## Known limitations

- Apache-2.0 upstream (with GPL-licensed parts): redistribution obligations stay with upstream's licensing.
- No upstream branding or identity is changed; this is not an endorsement by the QGC project.
- Remote input is opt-in and application-scoped in focus
  ([`docs/known-limitations.md`](../../../docs/known-limitations.md)).
