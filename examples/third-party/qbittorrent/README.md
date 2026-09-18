# Running qBittorrent through the Transparent QPA path

**Third-party verification example - not a V1 compatibility/support claim.** This example is optional, it is not
part of the acceptance suite, it changes no compatibility row, no support statement and no GA matrix cell, and it
must never become a release blocker. It exists because a real application on the deployed zero-code path is the
most convincing way to see the Transparent QPA Proxy work, and because a user asked to reproduce it.

It also demonstrates the one rule that surprises people: the Transparent QPA payload delegates to the platform's
**native** Qt integration, so a deployment must contain both `qhyremote` and that native delegate.

## Pinned versions

| Component | Pinned value |
| --- | --- |
| qBittorrent | tag `release-5.2.3`, commit `70e16de46cd559a7db3e6d7faace5e40b86f63dc` |
| Qt | **exactly 6.8.3** - the same kit that built the HyRemote QPA payload, because the payload is qualified against one exact Qt private ABI |
| HyRemote | the revision that ships this example; record its exact commit SHA together with your observation |

Nothing here promises anything about other qBittorrent versions, other Qt versions, or other applications.

## Prerequisites

qBittorrent itself requires (from its own `CMakeLists.txt` / `INSTALL`): CMake >= 3.16, a C++17 compiler,
Boost >= 1.76, OpenSSL >= 3.0.2 development files, and libtorrent-rasterbar `2.0.10-2.0.x` (or `1.2.19-1.2.x`).
On Windows those are most easily obtained with a package manager such as vcpkg; on Linux use the distribution
packages.

HyRemote must be built with the Transparent QPA payload for that same Qt:

```text
cmake -S . -B build/ga -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<QtRoot> \
  -DHYREMOTE_WITH_QPA_PROXY=ON
cmake --build build/ga
```

## 1. Fetch and build the application unmodified

```text
examples/third-party/qbittorrent/fetch-and-build.sh <QtRoot>          # Linux
examples/third-party/qbittorrent/fetch-and-build.ps1 <QtRoot>         # Windows (Developer PowerShell)
```

The scripts clone the pinned tag into `build/third-party/qbittorrent` (an ignored build directory), check out the
pinned commit, and configure/build qBittorrent **without modifying its sources**. Their only inputs are the
pinned commit and the Qt kit; if your environment needs extra prefixes (Boost, OpenSSL, libtorrent), pass them the
way that environment documents, for example `-DCMAKE_PREFIX_PATH` with several entries.

## 2. Assemble the deployed application

Copy into the application's own deployment directory - never into the application's sources:

- the shared HyRemote runtime (`HyRemoteRemoteAccess`) and the QPA payload `qhyremote` into
  `plugins/platforms/`;
- the native Qt platform delegate for that OS: `plugins/platforms/libqxcb.so` on Linux, `plugins/platforms/qwindows.dll`
  on Windows, together with their Qt runtime dependencies (`libQt6XcbQpa.so.6` and friends on Linux);
- the Qt runtime the application already needs.

This mirrors exactly what `hyremote_deploy(... QPA)` produces for HyRemote's own consumers; the application itself
still links only Qt, and no HyRemote header, library or target is added to it.

## 3. Launch and observe

```text
qbittorrent -platform hyremote          # Linux
qbittorrent.exe -platform hyremote      # Windows
```

Then connect a viewer (for example `vncdotool 1.3.0`, the qualified viewer) to the configured port. Record for the
observation: the application tag and commit above, the exact HyRemote SHA, the Qt version, the OS and toolchain,
the exact commands, the viewer and its version, and what was observed. Remote input is not granted by default:
the zero-code QPA path uses an explicit startup policy, so a viewer that can see but not control the application
is the documented default rather than a defect.

## What this example does and does not show

- It shows: an unmodified third-party Qt Widgets application running on the deployed `-platform hyremote` path,
  visible and (under the explicit input policy) controllable, with the application's own local window unaffected.
- It does not show, and does not claim: capture of arbitrary foreign native surfaces. Menus, dialogs and secondary
  windows are separate native surfaces unless the platform composes them, which is the documented V1 boundary.
- Licensing: qBittorrent is GPL-licensed while HyRemote is Apache-2.0. Running this locally for verification is
  fine; do not distribute a combined work built this way, and do not attach it to a release.

## Troubleshooting

| Symptom | Cause / action |
| --- | --- |
| `HyRemote QPA Proxy could not create native delegate "xcb"` followed by `Could not load the Qt platform plugin "hyremote"` and an abort | The native delegate is missing from the deployment. Copy the OS delegate (`libqxcb.so` / `qwindows.dll`) and its Qt runtime closure into `plugins/platforms/` and `lib/` as described above. |
| `Could not load the Qt platform plugin "hyremote" ... even though it was found` | The shared HyRemote runtime is not resolvable from the plugin. Keep `qhyremote` and `HyRemoteRemoteAccess` in the same deployment tree and do not rely on `LD_LIBRARY_PATH`/`PATH` overrides. |
| The application starts but a viewer cannot control it | Expected: the zero-code path starts view-only and grants input through an explicit startup policy. |
| The application fails to configure | libtorrent, Boost or OpenSSL is missing or too old; see the requirements above. The pinned versions are the ones this recipe was written against. |
| The application links a different Qt than HyRemote | The QPA payload is qualified against exactly one Qt private ABI. Build qBittorrent against the same 6.8.3 kit that built the payload. |
