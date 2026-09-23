# Integrate HyRemote into your Qt project

This is HyRemote's **single default entry point**. Follow it in order and an ordinary Qt application becomes remotely
viewable with a VNC viewer - and controllable when you ask for it - without reading the architecture, input-model or
repository-layout documents first.

**Every command below was actually executed against a real installed tree** (Windows x86_64 / Qt 6.8.3 / MinGW /
CMake 3.21+ / C++17; the POSIX equivalent is given alongside). Every path and file name comes from the real artifacts,
not from an illustration.

---

## The names this guide uses (and the only ones it uses)

| Name | Meaning |
| --- | --- |
| `<HYREMOTE_REPO>` | the HyRemote **source repository** root (only exists if you build from source) |
| `<HYREMOTE_SDK_ROOT>` | the **installed SDK / runtime tree**. The one thing you consume |
| `<MY_APP_SOURCE>` | your own project's source directory |
| `<MY_APP_BUILD>` | your own project's build directory |
| `<MY_APP_DEPLOY>` | your own project's deployment directory (the runnable tree) |

**For a source install this identity always holds:**

```text
<HYREMOTE_SDK_ROOT> == <HYREMOTE_REPO>/build/install
```

It is never silently swapped for some other path later in this document. To install elsewhere, redefine
`<HYREMOTE_SDK_ROOT>` once and keep using the name in every command. On Windows, for example:

```powershell
$env:HYREMOTE_SDK_ROOT = "D:\sdk\HyRemote"
```

Likewise `HyRemote_DIR` is always written `<HYREMOTE_SDK_ROOT>/lib/cmake/HyRemote`, and the runnable program is always
`<MY_APP_DEPLOY>/bin/MyApp`.

---

## 0. What you need

| Item | Requirement |
| --- | --- |
| Qt | **6.8.3** (the reference version; HyRemote and your application must use the same Qt kit) |
| CMake | 3.21 or newer |
| C++ | C++17 |
| Platforms | Windows x86_64, Linux x86_64 |
| Network | HyRemote is **unauthenticated and unencrypted** by default: **trusted LAN only**, not Internet-safe |

---

## STEP 1 - Get HyRemote

You need exactly one thing: an **installed SDK directory**, `<HYREMOTE_SDK_ROOT>`.

### Option A - download the official Release artifact, and pick the right asset

The assets on a Release page are **not interchangeable**:

| Asset | What it is | Can it be `<HYREMOTE_SDK_ROOT>`? |
| --- | --- | --- |
| `*-source.tar.gz` / `*-source.zip` | a **source** archive (a `git archive` snapshot of the tag) | **No.** No build output, no CMake package, no runtime |
| `*-trial-windows-x86_64-<sha>.zip` and other **binary / runtime / SDK** archives | the **real install tree** (`include/`, `lib/`, `bin/`, `plugins/`, `qml/`) | **Yes** - extracted, it *is* `<HYREMOTE_SDK_ROOT>` |
| `SHA256SUMS` | the checksum list for the assets above | not an artifact |

The rule is the **binary SDK/runtime artifact contract**: only an archive that contains a real install tree may be
defined as `<HYREMOTE_SDK_ROOT>`. If asset names change in future, the judgement does not: look at the content, not the
name.

After extracting the binary SDK archive, confirm you have an install tree:

```powershell
# Windows PowerShell (<extracted> is the directory you extracted)
Get-ChildItem <extracted>
```

You should see these top-level entries (a real install tree):

```text
bin/  include/  lib/  plugins/  qml/  share/  translations/  HYREMOTE-MANIFEST.txt
```

**Verify it is really the SDK** (do not continue if this fails):

```powershell
Test-Path <extracted>/lib/cmake/HyRemote/HyRemoteConfig.cmake
```

```sh
# POSIX
test -f <extracted>/lib/cmake/HyRemote/HyRemoteConfig.cmake && echo "SDK ok"
```

Then define it:

```powershell
$env:HYREMOTE_SDK_ROOT = "<extracted>"
```

### Option B - build from source

```powershell
# Windows PowerShell
git clone https://github.com/skawu/HyRemote.git <HYREMOTE_REPO>
cd <HYREMOTE_REPO>
.\build.cmd install
```

```sh
# POSIX shell
git clone https://github.com/skawu/HyRemote.git <HYREMOTE_REPO>
cd <HYREMOTE_REPO>
sh ./build.cmd install
```

Afterwards, by the identity stated above:

| Directory | Meaning |
| --- | --- |
| `<HYREMOTE_REPO>/build/` | **build tree.** CMake's workspace, not something an application consumes |
| `<HYREMOTE_SDK_ROOT>` = `<HYREMOTE_REPO>/build/install/` | **the real SDK / runtime tree** |

**Do not consume** the `src/` tree, targets inside `build/`, or an `examples/` build tree. Your application depends on
`<HYREMOTE_SDK_ROOT>` only.

> To install elsewhere, pass an install prefix at `install` time and then define that directory as
> `<HYREMOTE_SDK_ROOT>`. Details in `docs/guide/install.md`.

**Both options converge here.** Every later step uses `<HYREMOTE_SDK_ROOT>` and never cares which route produced it.

---

## STEP 2 - Where the library actually is

Look at the real tree. **Do not memorise this page - list it yourself at any time:**

```powershell
# Windows PowerShell
Get-ChildItem -Recurse -Depth 2 <HYREMOTE_SDK_ROOT> | Select-Object FullName
```

```sh
# POSIX
find <HYREMOTE_SDK_ROOT> -maxdepth 3 | sort
```

The real installed tree (measured on Windows):

```text
<HYREMOTE_SDK_ROOT>/
  HYREMOTE-MANIFEST.txt          # this SDK's identity and configuration facts
  include/
    HyRemote/
      RemoteAccess.h             # the single public header
      RemoteAccessExport.h
  lib/
    libHyRemoteRemoteAccess.dll.a             # import library used for linking
    cmake/HyRemote/
      HyRemoteConfig.cmake                    # the find_package entry point
      HyRemoteConfigVersion.cmake
      HyRemoteTargets.cmake
      HyRemoteTargets-release.cmake
      HyRemoteDeploy.cmake                    # implements hyremote_deploy()
    HyRemote/plugins/generic/libqhyremote.dll
    HyRemote/plugins/platforms/libqhyremote.dll
    HyRemote/qml/HyRemote/hyremote-qmlplugin.dll
  bin/
    libHyRemoteRemoteAccess.dll               # runtime shared library
    ...Qt runtime and qt.conf...
  plugins/
    generic/libqhyremote.dll                  # Generic plugin
    platforms/libqhyremote.dll                # QPA plugin
    platforms/qwindows.dll                    # native platform plugin
  qml/HyRemote/qmldir                         # QML module definition
  share/  translations/
```

### The six questions everyone asks, answered

| Question | Answer (measured on Windows) | POSIX equivalent |
| --- | --- | --- |
| Where are the headers? | `<HYREMOTE_SDK_ROOT>/include/HyRemote/RemoteAccess.h` | same |
| Where is the CMake package? | `<HYREMOTE_SDK_ROOT>/lib/cmake/HyRemote/HyRemoteConfig.cmake` | same |
| Where are the DLLs / .so? | runtime `<HYREMOTE_SDK_ROOT>/bin/libHyRemoteRemoteAccess.dll`; link-time `<HYREMOTE_SDK_ROOT>/lib/libHyRemoteRemoteAccess.dll.a` | `<HYREMOTE_SDK_ROOT>/lib/libHyRemoteRemoteAccess.so` (one file for both) |
| Where is the Generic plugin? | `<HYREMOTE_SDK_ROOT>/plugins/generic/libqhyremote.dll` | `<HYREMOTE_SDK_ROOT>/plugins/generic/libqhyremote.so` |
| Where is the QPA plugin? | `<HYREMOTE_SDK_ROOT>/plugins/platforms/libqhyremote.dll` | `<HYREMOTE_SDK_ROOT>/plugins/platforms/libqhyremote.so` |
| Where is the QML module? | `<HYREMOTE_SDK_ROOT>/qml/HyRemote/` (`qmldir` + `hyremote-qmlplugin.dll`) | `<HYREMOTE_SDK_ROOT>/qml/HyRemote/` |

> POSIX follows the same naming contract (`lib` + name + platform suffix). On any platform, **the authoritative answer
> is the tree you listed yourself**.

---

## STEP 3 - Make your CMake find HyRemote

HyRemote is a standard CMake package: one `HyRemoteConfig.cmake` exporting `HyRemote::RemoteAccess`. Two legal methods;
either works.

### Method 1 - give the SDK root (recommended)

```powershell
# Windows PowerShell
cmake -S <MY_APP_SOURCE> -B <MY_APP_BUILD> -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_INSTALL_PREFIX="<MY_APP_DEPLOY>" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64;$env:HYREMOTE_SDK_ROOT"
```

```sh
# POSIX
cmake -S <MY_APP_SOURCE> -B <MY_APP_BUILD> -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="<MY_APP_DEPLOY>" \
  -DCMAKE_PREFIX_PATH="/opt/Qt/6.8.3/gcc_64:$HYREMOTE_SDK_ROOT"
```

`CMAKE_PREFIX_PATH` is a **list**: the Qt kit directory plus `<HYREMOTE_SDK_ROOT>`, semicolon-separated (quote the whole
value in PowerShell). CMake then finds the package through the standard layout (`lib/cmake/<name>/`).

### Method 2 - point straight at the package directory

```powershell
cmake -S <MY_APP_SOURCE> -B <MY_APP_BUILD> -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_INSTALL_PREFIX="<MY_APP_DEPLOY>" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64" `
  -DHyRemote_DIR="$env:HYREMOTE_SDK_ROOT/lib/cmake/HyRemote"
```

```sh
cmake -S <MY_APP_SOURCE> -B <MY_APP_BUILD> -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="<MY_APP_DEPLOY>" \
  -DCMAKE_PREFIX_PATH="/opt/Qt/6.8.3/gcc_64" \
  -DHyRemote_DIR="$HYREMOTE_SDK_ROOT/lib/cmake/HyRemote"
```

### The difference, spelled out

| Variable | Points at |
| --- | --- |
| `CMAKE_PREFIX_PATH` | **`<HYREMOTE_SDK_ROOT>`**; CMake looks for `lib/cmake/HyRemote/HyRemoteConfig.cmake` inside it |
| `HyRemote_DIR` | **`<HYREMOTE_SDK_ROOT>/lib/cmake/HyRemote`** - the directory holding `HyRemoteConfig.cmake` itself |

### Prove it works immediately

In your `CMakeLists.txt`:

```cmake
find_package(HyRemote CONFIG REQUIRED)
message(STATUS "HyRemote found: ${HyRemote_DIR}")
message(STATUS "HyRemote version: ${HyRemote_VERSION}")
```

Configure should print the real path and version (measured: `HyRemote found: <HYREMOTE_SDK_ROOT>/lib/cmake/HyRemote`,
`HyRemote version: 0.2.0.0`). If not, see the first row of STEP 8.

---

## STEP 4 - One complete copy-pasteable C++ project

No fragments. Copy this whole project.

```text
<MY_APP_SOURCE>/
  CMakeLists.txt
  main.cpp
```

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyApp LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
find_package(HyRemote CONFIG REQUIRED)

add_executable(MyApp main.cpp)
target_compile_features(MyApp PRIVATE cxx_std_17)
target_link_libraries(MyApp PRIVATE
    Qt6::Widgets
    HyRemote::RemoteAccess
)

install(TARGETS MyApp RUNTIME DESTINATION bin)

# Deploys the shared runtime, the Qt runtime closure and the HyRemote plugin payload next to the application.
hyremote_deploy(TARGET MyApp)
```

`main.cpp`:

```cpp
#include <QApplication>
#include <QMainWindow>

#include <HyRemote/RemoteAccess.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("MyApp"));
    window.resize(900, 600);
    window.show();

    // Bound to the top-level window. Construction is inert: no listener is opened here.
    HyRemote::RemoteAccess remote(&window);

    // Remote control is opt-in; it is disabled by default.
    remote.setRemoteInputEnabled(true);

    if (!remote.start()) {
        // Startup failure is a product-level diagnostic; nothing degrades silently.
        qWarning("HyRemote failed to start");
    }

    const int code = app.exec();
    remote.stop();   // stop once the run is over
    return code;
}
```

Four things you must understand:

1. `HyRemote::RemoteAccess` is the **CMake target** you link; the name is literally `HyRemote::RemoteAccess`.
2. **Do not link Core.** Core/Session/Transport are private implementation and not a public contract.
3. **Do not name `.lib` / `.dll` / `.so` by hand and do not copy internal libraries.** Linking goes through the target,
   deployment through `hyremote_deploy()`.
4. The `remote` object's lifetime must **cover the whole run**: it is the window's remote-access host.

---

## STEP 5 - Build your project

### Windows PowerShell

```powershell
cmake -S <MY_APP_SOURCE> -B <MY_APP_BUILD> -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_INSTALL_PREFIX="<MY_APP_DEPLOY>" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64;$env:HYREMOTE_SDK_ROOT"
cmake --build <MY_APP_BUILD>
cmake --install <MY_APP_BUILD>
```

### POSIX

```sh
cmake -S <MY_APP_SOURCE> -B <MY_APP_BUILD> -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="<MY_APP_DEPLOY>" \
  -DCMAKE_PREFIX_PATH="/opt/Qt/6.8.3/gcc_64:$HYREMOTE_SDK_ROOT"
cmake --build <MY_APP_BUILD>
cmake --install <MY_APP_BUILD>
```

> **`<MY_APP_DEPLOY>` must be absolute.** `cmake --install <MY_APP_BUILD> --prefix deploy` (a relative path) fails on
> Qt 6.8 with `CMake Error at .../Qt6CoreDeploySupport.cmake:38 (message): Given qt.conf path is not an absolute path`.
> Give the prefix at **configure** time via `-DCMAKE_INSTALL_PREFIX=<absolute path>` and then run plain
> `cmake --install <MY_APP_BUILD>`. Measured: absolute prefix exits 0, relative prefix fails reproducibly.

### Which executable you actually run

`hyremote_deploy()` puts the runnable tree under `<MY_APP_DEPLOY>`, not in the build directory:

```text
<MY_APP_DEPLOY>/
  bin/MyApp.exe                     <-- this is the one you run
  bin/libHyRemoteRemoteAccess.dll   <-- plus the shared runtime
  bin/Qt6*.dll  plugins/  qml/ ...  <-- Qt runtime closure and HyRemote payloads
```

```powershell
<MY_APP_DEPLOY>\bin\MyApp.exe
```

```sh
<MY_APP_DEPLOY>/bin/MyApp
```

There is also an executable in `<MY_APP_BUILD>`, but that one needs the SDK on its search path to start. For a person,
or for another machine, run `<MY_APP_DEPLOY>/bin/...`: it is self-contained.

---

## STEP 6 - Run and connect

### The real defaults (V0.2+, stated truthfully)

| Fact | Default |
| --- | --- |
| Listener | `0.0.0.0:5921` (the host's IPv4 interfaces, **not just loopback**) |
| Authentication | **off** (`none`) |
| Transport encryption | **off** (`none`) |
| Remote input | **off by default** (enable with `setRemoteInputEnabled(true)`) |
| Suitability | **trusted LAN only**, **not Internet-safe** |

### Step by step

1. **Find the host's LAN IPv4**

   ```powershell
   Get-NetIPAddress -AddressFamily IPv4 |
     Where-Object { $_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.254.*" } |
     Format-Table InterfaceAlias, IPAddress, PrefixLength
   ```

   ```sh
   ip -4 addr show scope global
   ```

   Exclude `127.*` (loopback) and `169.254.*` (APIPA).

2. **Confirm the port is listening** (on the host)

   ```powershell
   Get-NetTCPConnection -LocalPort 5921 -State Listen
   ```

   ```sh
   ss -ltnp | grep 5921
   ```

3. **On another machine on the same LAN, open a maintained VNC viewer** (TigerVNC, RealVNC VNC Viewer, any standard
   RFB viewer).
4. **Connect** to `<HOST_LAN_IP>:5921`.
5. You should see the application window.
6. If remote input is enabled: pointer, buttons, wheel, keyboard and text should all work.
7. Disconnect. 8. Reconnect - the application is still running and a second connection should succeed.
9. Stop: `remote.stop()` (or quit); confirm `:5921` is no longer listening.

> HyRemote ships no viewer of its own. Any standard RFB viewer works.

---

## STEP 7 - The four integration routes (four peers)

The four routes have **no ranking**. All follow the same flow: **get `<HYREMOTE_SDK_ROOT>` -> point CMake at it ->
build -> deploy to `<MY_APP_DEPLOY>` -> run -> connect a viewer**.

| Route | Application change | Deploy | Run |
| --- | --- | --- | --- |
| **C++ API** | link `HyRemote::RemoteAccess`, call the API | `hyremote_deploy(TARGET MyApp)` | normal launch |
| **QML API** | `import HyRemote` | `hyremote_deploy(TARGET MyApp QML)` | normal launch |
| **Generic Plugin** | **zero code** | `hyremote_deploy(TARGET MyApp GENERIC)`, or deploy the SDK payload directly (below) | `MyApp -plugin hyremote` |
| **QPA (Transparent)** | **zero code** | `hyremote_deploy(TARGET MyApp QPA)` | `MyApp -platform hyremote` |

### C++ API

STEP 4 is this route.

### QML API

```cmake
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE Qt6::Quick HyRemote::RemoteAccess)
hyremote_deploy(TARGET MyApp QML)
```

```qml
import QtQuick
import HyRemote

Item {
    HyRemote.RemoteAccess {
        target: window
        remoteInputEnabled: true
    }
}
```

### Generic Plugin (zero code)

The application links **nothing** from HyRemote, includes no HyRemote header and calls no HyRemote API.

**Your own project** uses the deployment statement:

```cmake
find_package(HyRemote CONFIG REQUIRED)   # only to obtain hyremote_deploy()
hyremote_deploy(TARGET MyApp GENERIC)
```

**A third-party application you cannot modify** uses the SDK payload directly, with zero upstream changes:

```powershell
# Windows PowerShell
$env:QT_PLUGIN_PATH = "$env:HYREMOTE_SDK_ROOT/plugins"       # generic/libqhyremote.dll lives here
$env:PATH           = "$env:HYREMOTE_SDK_ROOT/bin;$env:PATH"  # libHyRemoteRemoteAccess.dll lives here
<UpstreamApp>.exe -plugin hyremote
```

```sh
# POSIX
export QT_PLUGIN_PATH="$HYREMOTE_SDK_ROOT/plugins"
export LD_LIBRARY_PATH="$HYREMOTE_SDK_ROOT/bin:$LD_LIBRARY_PATH"
<UpstreamApp> -plugin hyremote
```

Measured on a pristine Qt 6.8.3 application with zero HyRemote code: the process stays alive, listens on
`0.0.0.0:5921`, and the application itself prints
`HyRemote automatic application access active on "0.0.0.0" 5921 remote input: false security profile: insecure`.
Without `-plugin hyremote` the same binary is an ordinary application (native platform stays `windows`/`xcb`).

### QPA (Transparent QPA Proxy, zero code)

An **additional** route, for exactly-qualified Qt only:

```cmake
hyremote_deploy(TARGET MyApp QPA)
```

```powershell
<MY_APP_DEPLOY>\bin\MyApp.exe -platform hyremote
```

QPA is bound to **Qt's exact private ABI**: the Qt used for deployment must be exactly the same version and kit as your
application's (reference: Qt 6.8.3 MinGW/x86_64).

---

## STEP 8 - The most common failures

| Symptom | Cause | Fix |
| --- | --- | --- |
| `Could not find a package configuration file provided by "HyRemote"` | CMake cannot see the SDK | Add `<HYREMOTE_SDK_ROOT>` to `CMAKE_PREFIX_PATH`, or pass `-DHyRemote_DIR=<HYREMOTE_SDK_ROOT>/lib/cmake/HyRemote` |
| Qt version/kit mismatch | Different Qt kits | Use one Qt 6.8.3 kit on both sides; QPA is strictest |
| Deploy fails with `Given qt.conf path is not an absolute path` | `<MY_APP_DEPLOY>` is relative | Pass `-DCMAKE_INSTALL_PREFIX=<absolute>` at configure time, then `cmake --install <MY_APP_BUILD>` |
| Windows cannot find a DLL at launch | You ran the `<MY_APP_BUILD>` executable | Run `<MY_APP_DEPLOY>/bin/MyApp.exe` |
| `no Qt platform plugin could be initialized` | `plugins/platforms/` missing from the deploy tree | Deploy with `hyremote_deploy()` |
| QML: `module "HyRemote" is not installed` | QML module not deployed | `hyremote_deploy(TARGET MyApp QML)`; check `<MY_APP_DEPLOY>/qml/HyRemote/qmldir` |
| `-plugin hyremote` changes nothing | Qt cannot find the Generic payload | Point `QT_PLUGIN_PATH` at `<HYREMOTE_SDK_ROOT>/plugins`; check `plugins/generic/libqhyremote.dll` |
| `-platform hyremote` unavailable | No QPA payload, or inexact Qt | QPA needs the proxy built and an **exact** Qt match |
| `:5921` not listening | Application not running, `start()` failed, or state is `Unavailable` | Check logs and `remote.lastError()` |
| `:5921` in use | Another process listening | `remote.setPort(...)` while `Stopped`, or stop the other process |
| Viewer cannot connect, locally it works | Loopback used, or wrong address | Use `<HOST_LAN_IP>`; confirm `0.0.0.0:5921` |
| Another machine cannot connect at all | Windows Firewall, or different LAN | Allow inbound for the application (or TCP 5921); same subnet |
| Picture works, input does nothing | Remote input is off by default | `remote.setRemoteInputEnabled(true)` |
| Odd focus behaviour after switching applications locally | application-scoped focus limitation | Known limitation: focus semantics are application-scoped |

---

## Update, disable and roll back

| Goal | How |
| --- | --- |
| Upgrade the SDK | Re-configure, rebuild and redeploy against the new `<HYREMOTE_SDK_ROOT>` |
| Upgrade only the runtime | Overwrite the runtime and plugin payloads in `<MY_APP_DEPLOY>` from the new SDK |
| Disable remote access | Do not call `start()`, or call `stop()` while running |
| Remove entirely | Drop `hyremote_deploy()`, the link and the include, then rebuild and redeploy |

Your project depends on exactly one external input, **`<HYREMOTE_SDK_ROOT>`**.

---

## Next

- The four routes in depth: `docs/getting-started/cpp.md`, `qml.md`, `generic.md`, `qpa-proxy.md`
- Deployment detail: `docs/guide/deployment.md`
- Troubleshooting: `docs/guide/troubleshooting.md`
- Real open-source integration studies (pristine upstream + Generic payload): `examples/real-world/`
- Architecture and input model (reference, not entry points): `docs/architecture.md`, `docs/input-model.md`
