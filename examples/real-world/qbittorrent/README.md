# qBittorrent - Qt Widgets / C++ API study

An editable, actively maintained Qt Widgets application with ~40k stars: the case where the C++ API fits, because the
application can be changed and its main window is a real `QMainWindow`.

```text
UPSTREAM_REPOSITORY=github.com/qbittorrent/qBittorrent
UPSTREAM_REVISION=705708085ccd957f42a315df7ef97aa27b0a85a6   (master, 2026-09-22, "Avoid potential redundant byte array copy")
UPSTREAM_LICENSE=GPL-2.0-or-later (GitHub reports NOASSERTION: COPYING + COPYING.GPLv2 + COPYING.GPLv3)
STAR_COUNT_AT_SELECTION=40266   (forks 4882, read from the GitHub API 2026-09-23)
MAINTAINED=yes (pushed 2026-09-21, not archived)
UI_FAMILY=Qt Widgets / C++
INTEGRATION_ROUTE=C++ API (HyRemote::RemoteAccess)
UPSTREAM_QT_REQUIREMENT=Qt 6.6.0 or newer; upstream also requires Boost 1.76+, OpenSSL 3.0.2+, libtorrent 2.0.10+, zlib 1.2.11+
```

## Why this route

qBittorrent is editable and its top-level window is a real Qt Widgets window
(`class MainWindow final : public GUIApplicationComponent<QMainWindow>`, `src/gui/mainwindow.h`). The C++ API is the
honest fit: the application gets an explicit lifecycle and can decide when remote access runs. The zero-code routes
(Generic, QPA) would also work on the executable, but they would leave the lifecycle to a launch argument and give the
application no place to express policy, which is not what an editable application should settle for.

## What the patch changes

One file, `src/app/CMakeLists.txt`, where upstream already links the application target:

```text
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(qbt_app PRIVATE HyRemote::RemoteAccess)
hyremote_deploy(TARGET qbt_app)
```

- **Which `CMakeLists.txt` was changed:** `src/app/CMakeLists.txt` (inserted immediately after
  `target_link_libraries(qbt_app PRIVATE qbt_gui)`, the line that already links the GUI library into the executable).
- **Which `find_package(HyRemote ...)` was added:** `find_package(HyRemote CONFIG REQUIRED)`.
- **Which target is linked:** `HyRemote::RemoteAccess`. No Core, no internal target, no hand-written `.dll`/`.so` name.
- **How `hyremote_deploy()` is executed:** `hyremote_deploy(TARGET qbt_app)` runs at install time and puts the shared
  runtime plus the Qt runtime closure next to the deployed executable.
- **Window object the `RemoteAccess` is created on:** the application's main window —
  `MainWindow` (`src/gui/mainwindow.h`, a `QMainWindow`). Application code change is 4 lines, added where the main
  window is constructed:

```cpp
#include <HyRemote/RemoteAccess.h>

auto remote = std::make_unique<HyRemote::RemoteAccess>(mainWindow);
remote->start();   // before app.exec(); the object must outlive the run
```

- **Runnable tree / how to launch:** the install prefix (`cmake --install` output) holds `bin/` with the executable and
  its runtime closure; launch that deployed executable, not the one in the build tree.

## Verification actually performed, and its limit

```text
PATCH_APPLIES=PASS        git apply --check on the pinned revision 7057080 -> exit 0
CONFIGURE=FAIL            the upstream dependency check refuses on this machine:
                          CMake Error at cmake/Modules/CheckPackages.cmake:35 (find_package)
                          upstream's declared minimums are Boost 1.76+, OpenSSL 3.0.2+,
                          libtorrent 2.0.10+ and zlib 1.2.11+, none of which are installed here
BUILD=NOT REACHED         blocked by the configure failure above, not by HyRemote
LISTENER=NOT REACHED
VIEWER_CONNECT=NOT REACHED
```

**This study was not taken to a remote-viewable qBittorrent window on this machine.** That is a real limitation of this
study, not a claim about HyRemote: the integration is three CMake lines against targets that were verified to exist
(`qbt_app` is the real executable target), and the patch was verified to apply to the pinned revision, but the
third-party dependency set (Boost, OpenSSL, libtorrent) is not installed here and building them was out of scope for
this slice. Nothing here should be read as "it should work" - it is "the integration is prepared and validated up to the
point where the upstream dependency set is required".

## How a viewer would connect (once built)

After a successful build and `cmake --install`:

1. launch the deployed `<prefix>/bin/qbittorrent` (the C++ route opens the listener from `RemoteAccess::start()`);
2. confirm the listener on the host: `Get-NetTCPConnection -LocalPort 5921 -State Listen` (Windows) or
   `ss -ltnp | grep 5921` (Linux);
3. point any standard VNC/RFB viewer at `<HOST_LAN_IP>:5921`.

Default facts, unchanged by this integration: listener `0.0.0.0:5921`, authentication off, transport encryption off,
remote input **off** (qBittorrent is windowed and input-capable, so enabling `setRemoteInputEnabled(true)` is the
interesting case), trusted LAN only, not Internet-safe.

## Known limitations

- Upstream licence is GPL-2.0-or-later: any distributed build of a patched qBittorrent stays under that licence.
- This study does not change upstream branding or identity.
- Remote input is opt-in and application-scoped in focus, as documented in
  [`docs/known-limitations.md`](../../../docs/known-limitations.md).
