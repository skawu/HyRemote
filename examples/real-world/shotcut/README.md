# Shotcut v26.8.1 - Qt Quick/QML + Widgets study (Qt 6 lane)

```text
UPSTREAM_REPOSITORY=github.com/mltframework/shotcut
UPSTREAM_REVISION=v26.8.1   (pinned release tag; upstream's latest release at selection, published 2026-08-01)
UPSTREAM_LICENSE=GPL-3.0 (COPYING)
STAR_COUNT_AT_SELECTION=15261   (read from the GitHub API 2026-09-23)
MAINTAINED=yes (pushed 2026-09-22, not archived)
UI_FAMILY=Qt Quick / QML with Qt Widgets (C++)
INTEGRATION_ROUTE=GENERIC
UPSTREAM_QT_LANE=Qt 6 - find_package(Qt6 6.4 REQUIRED ...) in the pinned CMakeLists.txt; cmake_minimum_required(VERSION 3.12...3.31)
UPSTREAM_SOURCE_PATCHES_FOR_HYREMOTE=0
```

## Why GENERIC here

Shotcut is a large, heavily used Qt desktop application whose release engineering is its maintainers' own concern. It is
in the frozen set as a second Qt 6 representative with a different UI composition (QML plus Widgets), and it is reached
the way any unmodified application is reached: at run time, through the Qt generic plugin, with no source change at all.

`PRIMARY_ROUTE=GENERIC`. **No upstream change**: no `find_package(HyRemote)`, no `HyRemote::RemoteAccess` link, no
`hyremote_deploy()`, no patch. `UPSTREAM_SOURCE_PATCHES_FOR_HYREMOTE=0`.

## How the integration works (no upstream change)

```text
original Shotcut application (obtained the way upstream says to: their installer or their own build)
  -> deploy the Generic payload from the installed HyRemote SDK
  -> shotcut -plugin hyremote
  -> listener 0.0.0.0:5921
  -> connect a viewer
```

```powershell
# Windows PowerShell
$env:QT_PLUGIN_PATH = "$env:HYREMOTE_SDK_ROOT/plugins"       # generic/libqhyremote.dll
$env:PATH           = "$env:HYREMOTE_SDK_ROOT/bin;$env:PATH"  # libHyRemoteRemoteAccess.dll
shotcut.exe -plugin hyremote
```

```sh
# POSIX
export QT_PLUGIN_PATH="$HYREMOTE_SDK_ROOT/plugins"
export LD_LIBRARY_PATH="$HYREMOTE_SDK_ROOT/bin:$LD_LIBRARY_PATH"
./shotcut -plugin hyremote
```

Confirm the listener (`Get-NetTCPConnection -LocalPort 5921 -State Listen`, or `ss -ltnp | grep 5921`) and connect a
standard VNC/RFB viewer to `<HOST_LAN_IP>:5921`.

Defaults are the product's own and are unchanged by the integration: `0.0.0.0:5921`, authentication off, transport
encryption off, remote input **off** until enabled, trusted LAN only, not Internet-safe.

## Verification actually performed, and what is missing

```text
UPSTREAM_PINNED_TAG_EXISTS=PASS      tag v26.8.1 exists upstream (and is its latest release)
QT_LANE_READ_FROM_UPSTREAM=PASS      find_package(Qt6 6.4 REQUIRED ...) in the pinned tree
QT_LANE_ON_REFERENCE=PASS            Qt 6 sits on the current V0.3.0 reference lane (Qt 6.8.3)
LICENCE_FILES_READ=PASS              COPYING (GPL-3.0)
UPSTREAM_PATCH=0
GENERIC_MECHANISM_ON_CURRENT_LANE=PASS   verified with a pristine Qt 6.8.3 application, zero HyRemote code:
                                         process alive, listening 0.0.0.0:5921, application log
                                         "HyRemote automatic application access active on \"0.0.0.0\" 5921
                                          remote input: false security profile: insecure"
GENERIC_RUN_ON_SHOTCUT_ITSELF=NOT PERFORMED  obtaining or building Shotcut (MLT/FFmpeg toolchain) was not
                                             carried out in this slice, so no listener/viewer result is
                                             claimed for Shotcut itself
VIEWER_CONNECT=NOT REACHED ON SHOTCUT ITSELF
```

Stated plainly: the mechanism is proven on the current lane with a pristine Qt application; the application-specific run
is not done here. Nothing here means "it should work".

## Known limitations

- GPL-3.0 upstream: redistribution obligations stay with upstream's licensing.
- No upstream branding or identity is changed; this is not an endorsement by the Shotcut/MLT project.
- Remote input is opt-in and application-scoped in focus
  ([`docs/known-limitations.md`](../../../docs/known-limitations.md)).
