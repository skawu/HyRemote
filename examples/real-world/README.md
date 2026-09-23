# Real-world integration studies

These studies show how HyRemote reaches **real, maintained, high-adoption open-source Qt applications** - without
touching their source. The teaching point is the one a user actually faces: you already have an upstream application
built the way its own project builds it, and you want it remotely viewable.

The frozen representative set is:

| # | Upstream | Pinned revision | UI family | Qt lane | Primary route |
| --- | --- | --- | --- | --- | --- |
| W | [keepassxc/](keepassxc) | KeePassXC `2.7.12` | Qt Widgets / C++ | **Qt 5.15** - not currently qualified on V0.3.0, see below | GENERIC |
| Q | [qgroundcontrol/](qgroundcontrol) | QGroundControl `v5.0.6` | Qt Quick / QML | Qt 6 | GENERIC |
| X | [shotcut/](shotcut) | Shotcut `v26.8.1` | Qt Quick / QML + Qt Widgets | Qt 6 | GENERIC |

**FROZEN_REAL_WORLD_SET=KeePassXC/QGroundControl/Shotcut.** The set is fixed: this is a representative set, not a score
table, and projects are not swapped for easier ones when third-party dependency work turns out to be heavy.

## `PRIMARY_ROUTE=GENERIC`

Every frozen study uses the **Generic Plugin** route, because it is the only route that reaches an unmodified
application:

```text
original upstream app
  -> obtain it the way the upstream project says to (their installer, their build)
  -> deploy the Generic payload from the installed HyRemote SDK
  -> run:  <UpstreamApp> -plugin hyremote
  -> listener 0.0.0.0:5921
  -> connect a viewer
  -> use the application
```

**Upstream sources stay pristine.** These studies add **no** `find_package(HyRemote)`, **no** `HyRemote::RemoteAccess`
link, **no** `hyremote_deploy()` and **no source patch** to any third-party project. `UPSTREAM_SOURCE_PATCHES_FOR_HYREMOTE=0`.

QPA (`-platform hyremote`) is documented as an **additional** route only where Qt is exactly qualified; it is not part
of the primary story, and it is never the way a pristine third-party application is reached by default.

## The deployment step, for an application you cannot modify

Two supported ways to make the Generic payload visible, neither of which edits upstream:

```powershell
# Windows PowerShell - point the process at the SDK payloads
$env:QT_PLUGIN_PATH = "$env:HYREMOTE_SDK_ROOT/plugins"
$env:PATH           = "$env:HYREMOTE_SDK_ROOT/bin;$env:PATH"
<UpstreamApp>.exe -plugin hyremote
```

```sh
# POSIX
export QT_PLUGIN_PATH="$HYREMOTE_SDK_ROOT/plugins"
export LD_LIBRARY_PATH="$HYREMOTE_SDK_ROOT/bin:$LD_LIBRARY_PATH"
<UpstreamApp> -plugin hyremote
```

Or copy the payload next to the application (still no upstream change):

```text
<UpstreamAppDir>/
  <UpstreamApp executable>
  plugins/generic/libqhyremote.dll      <- from <HYREMOTE_SDK_ROOT>/plugins/generic/
  libHyRemoteRemoteAccess.dll           <- from <HYREMOTE_SDK_ROOT>/bin/
```

The mechanism is verified: a pristine Qt 6.8.3 application with zero HyRemote code started this way stays alive, listens
on `0.0.0.0:5921`, and prints
`HyRemote automatic application access active on "0.0.0.0" 5921 remote input: false security profile: insecure`.

## Current capability boundary (stated, not fudged)

HyRemote V0.3.0's reference lane is **Qt 6.8.3**. Qt 5.15 is **not qualified** in the current product line
(`docs/compatibility.md` records Qt 5.15 LTS as a **V0.4 qualification** item and forbids describing it as supported in
the meantime).

That matters directly here: **KeePassXC 2.7.12 is a Qt 5.15 application** (`find_package(Qt5 ...)` in its own
`CMakeLists.txt`). So:

```text
KEEEPASSXC_STATUS=SELECTED REPRESENTATIVE, BUT NOT CURRENTLY QUALIFIED ON V0.3.0
```

It stays in the frozen set as a representative, and its study says plainly that it cannot provide current-lane Generic
evidence until the Qt 5.15 product lane exists. It is not marked green, and nothing is faked to make it green.

QGroundControl `v5.0.6` and Shotcut `v26.8.1` are Qt 6 applications and sit on the current reference lane, so they are
where current-lane Generic integration evidence is produced.

## Evidence discipline

Each study records the exact upstream repository, revision, licence, star count at selection, build system facts read
from the pinned tree, the real executable/plugin-name situation, how the application is obtained, how the Generic payload
is deployed, how to launch, how the listener is confirmed, what a viewer connects to, and what could not be completed
here. A study that stops short says so; none of them say "it should work".

Star counts were read from the GitHub API when the set was frozen (2026-09-23) and are recorded so they can be
re-verified.

## Licences and attribution

| Upstream | Licence (as reported upstream) |
| --- | --- |
| KeePassXC | GPL-2.0-or-later / GPL-3.0-or-later (the tree ships `COPYING`, `LICENSE.GPL-2`, `LICENSE.GPL-3` plus other component licences; GitHub reports `NOASSERTION`) |
| QGroundControl | Apache-2.0 (`LICENSE-APACHE`; the tree also carries `LICENSE-GPL`) |
| Shotcut | GPL-3.0 (`COPYING`) |

Nothing from these projects is copied into this repository, and nothing here changes their branding. Deploying the
HyRemote Generic payload next to an upstream binary is a runtime arrangement, not a redistribution of that project.
