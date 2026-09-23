# KeePassXC 2.7.12 - Widgets/C++ study (Qt 5.15 lane)

```text
UPSTREAM_REPOSITORY=github.com/keepassxreboot/keepassxc
UPSTREAM_REVISION=2.7.12   (annotated release tag; latest release at selection, published 2026-03-10)
UPSTREAM_LICENSE=GPL-2.0-or-later / GPL-3.0-or-later (COPYING, LICENSE.GPL-2, LICENSE.GPL-3, plus component licences; GitHub reports NOASSERTION)
STAR_COUNT_AT_SELECTION=28909   (read from the GitHub API 2026-09-23)
MAINTAINED=yes (pushed 2026-09-23, not archived)
UI_FAMILY=Qt Widgets / C++
INTEGRATION_ROUTE=GENERIC (primary route for the frozen set)
UPSTREAM_QT_LANE=Qt 5.15 - find_package(Qt5 ...) in the pinned CMakeLists.txt
UPSTREAM_BUILD_SYSTEM=CMake (cmake_minimum_required(VERSION 3.10.0))
UPSTREAM_SOURCE_PATCHES_FOR_HYREMOTE=0
```

## Status: selected representative, not currently qualified

```text
KEEEPASSXC_STATUS=SELECTED REPRESENTATIVE, BUT NOT CURRENTLY QUALIFIED ON V0.3.0
```

KeePassXC 2.7.12 is a **Qt 5.15** application. HyRemote V0.3.0's reference lane is **Qt 6.8.3**, and
[`docs/compatibility.md`](../../../docs/compatibility.md) records Qt 5.15 LTS as a **V0.4 qualification** item with an
explicit instruction not to describe it as supported until the product can actually build and deliver on it.

So this study **does not** claim a working Generic integration today. There is no Qt 5.15 HyRemote build to deploy a
Generic payload from, and the chain stops at the platform boundary rather than at HyRemote's mechanics. It waits on the
Qt 5.15 product lane (`docs/versioning.md`, V0.4 "Qualify It").

This is exactly the fact a user needs: choosing KeePassXC today means waiting for the Qt 5.15 lane, not filing a bug
about the Generic route.

## Why this project is in the frozen set

It is a large, actively maintained, real Qt **Widgets** desktop application with very high adoption, and it exercises the
case the Generic route exists for: a security-sensitive application whose maintainers would never accept an out-of-tree
patch just to make it remotely viewable. A runtime-only route is the only honest one, which is why the frozen set's
primary route is GENERIC.

The set is frozen. This entry is **not** replaced by an easier project because its Qt lane is not qualified yet.

## What the integration will be, once the Qt 5.15 lane exists

No upstream change at any point:

1. obtain the application the way upstream says to (their released installer or their own build);
2. deploy the Generic payload from the Qt 5.15 HyRemote SDK:

   ```powershell
   $env:QT_PLUGIN_PATH = "$env:HYREMOTE_SDK_ROOT/plugins"      # generic/libqhyremote.dll
   $env:PATH           = "$env:HYREMOTE_SDK_ROOT/bin;$env:PATH" # libHyRemoteRemoteAccess.dll
   ```

3. run `KeePassXC.exe -plugin hyremote`;
4. confirm the listener: `Get-NetTCPConnection -LocalPort 5921 -State Listen` (Windows) or `ss -ltnp | grep 5921`;
5. connect a standard VNC/RFB viewer to `<HOST_LAN_IP>:5921`.

Default facts, unchanged by the integration: `0.0.0.0:5921`, authentication off, transport encryption off, remote input
**off** (enabling it is the interesting case for a password-manager UI, and it is the application owner's decision),
trusted LAN only, not Internet-safe.

## Verification actually performed

```text
UPSTREAM_PINNED_TAG_EXISTS=PASS     tag 2.7.12 exists; it is also upstream's latest release
QT_LANE_READ_FROM_UPSTREAM=PASS     find_package(Qt5 COMPONENTS ${QT_COMPONENTS} REQUIRED) in the pinned CMakeLists.txt
LICENCE_FILES_READ=PASS             COPYING, LICENSE.GPL-2, LICENSE.GPL-3 and component licences listed
UPSTREAM_PATCH=0
GENERIC_RUNTIME_EVIDENCE=BLOCKED    no Qt 5.15 HyRemote build exists in V0.3.0, so no Generic payload to deploy
VIEWER_CONNECT=NOT REACHED          blocked by the platform boundary above, not by the Generic route itself
```

The Generic mechanism itself is verified elsewhere in this repository on the current lane (see
[`../README.md`](../README.md)): a pristine Qt 6.8.3 application with zero HyRemote code, started with the SDK payload on
`QT_PLUGIN_PATH` and `-plugin hyremote`, listens on `0.0.0.0:5921`. What is missing for KeePassXC specifically is the Qt
5.15 lane, and this file says so instead of pretending otherwise.

## Known limitations

- Qt 5.15 is not qualified in V0.3.0; this study cannot produce current-lane evidence until V0.4.
- GPL licensing: deploying HyRemote next to a GPL KeePassXC binary does not change KeePassXC's licence; if you
  redistribute a combined product, that distribution's obligations are yours to work out.
- No upstream branding or identity is changed, and this is not an endorsement by the KeePassXC project.
- Remote input is opt-in and application-scoped in focus, as documented in
  [`docs/known-limitations.md`](../../../docs/known-limitations.md).
