# Integrate HyRemote into your Qt project

This is HyRemote's **single default entry point**. Follow it in order and an ordinary Qt application becomes remotely
viewable with a VNC viewer - and controllable when you ask for it - without reading the architecture, input-model or
repository-layout documents first.

You already have your own Qt project. This starts at "go and get HyRemote" and leaves nothing implicit.

**Every command below was actually executed against a real `build/install` tree** (Windows x86_64 / Qt 6.8.3 / MinGW /
CMake 3.21+ / C++17; the Linux equivalent is given alongside). Every path and file name comes from the real artifacts,
not from an illustration.

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

You need exactly one thing: an **installed SDK directory**. Both routes below produce the same `<HYREMOTE_SDK_ROOT>`.

### Option A - download the official Release artifact

Download the archive for your platform from GitHub Releases and extract it. The extracted directory **is** your
`<HYREMOTE_SDK_ROOT>`: it already contains the headers, the CMake package, the shared runtime and the plugin payloads.

### Option B - build from source

```powershell
# Windows PowerShell
git clone https://github.com/skawu/HyRemote.git
cd HyRemote
.\build.cmd install
```

```sh
# POSIX shell
git clone https://github.com/skawu/HyRemote.git
cd HyRemote
sh ./build.cmd install
```

Afterwards:

| Directory | Meaning |
| --- | --- |
| `<repo>/build/` | **Build tree.** CMake's workspace, not something an application consumes |
| `<repo>/build/install/` | **The real SDK / runtime tree.** This is `<HYREMOTE_SDK_ROOT>` |

**Do not consume** the `src/` tree, targets inside `build/`, or an `examples/` build tree. None of them is a stable
contract. Your application depends on `build/install/` (or on the published artifact) only.

> To install elsewhere, pass an install prefix (for example `.\build.cmd install --cmake=CMAKE_INSTALL_PREFIX=D:/sdk/HyRemote`). See `docs/guide/install.md`.

---

## STEP 2 - Where the library actually is

Look at the real tree. **Do not memorise this page - list it yourself at any time:**

```powershell
# Windows PowerShell
Get-ChildItem -Recurse -Depth 2 <HYREMOTE_SDK_ROOT> | Select-Object FullName
```

```sh
# Linux
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

| Question | Answer (measured on Windows) | Linux equivalent |
| --- | --- | --- |
| Where are the headers? | `<SDK>/include/HyRemote/RemoteAccess.h` | same |
| Where is the CMake package? | `<SDK>/lib/cmake/HyRemote/HyRemoteConfig.cmake` | same |
| Where are the DLLs / .so? | runtime `<SDK>/bin/libHyRemoteRemoteAccess.dll`; link-time `<SDK>/lib/libHyRemoteRemoteAccess.dll.a` | `<SDK>/lib/libHyRemoteRemoteAccess.so` (one file for both) |
| Where is the Generic plugin? | `<SDK>/plugins/generic/libqhyremote.dll` | `<SDK>/plugins/generic/libqhyremote.so` |
| Where is the QPA plugin? | `<SDK>/plugins/platforms/libqhyremote.dll` | `<SDK>/plugins/platforms/libqhyremote.so` |
| Where is the QML module? | `<SDK>/qml/HyRemote/` (`qmldir` + `hyremote-qmlplugin.dll`) | `<SDK>/qml/HyRemote/` |

> Linux follows the same naming contract (`lib` + name + platform suffix). On any platform, **the authoritative answer
> is the tree you listed yourself** with the command above.

---

## STEP 3 - Make your CMake find HyRemote

HyRemote is a standard CMake package: one `HyRemoteConfig.cmake` exporting `HyRemote::RemoteAccess`. Two legal methods;
either works.

### Method 1 - give the SDK root (recommended)

```powershell
# Windows PowerShell
cmake -S . -B build `
  -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64;D:/sdk/HyRemote"
```

```sh
# Linux
cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/opt/Qt/6.8.3/gcc_64;/opt/hyremote-sdk"
```

`CMAKE_PREFIX_PATH` is a **list**: the Qt kit directory plus the HyRemote **SDK root**, separated by semicolons (quote
the whole value in PowerShell). CMake then finds the package through the standard layout (`lib/cmake/<name>/`).

### Method 2 - point straight at the package directory

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64" `
  -DHyRemote_DIR="D:/sdk/HyRemote/lib/cmake/HyRemote"
```

### The difference, spelled out

| Variable | Points at |
| --- | --- |
| `CMAKE_PREFIX_PATH` | the **SDK root**; CMake looks for `lib/cmake/HyRemote/HyRemoteConfig.cmake` inside it |
| `HyRemote_DIR` | **the directory holding `HyRemoteConfig.cmake` itself** (`.../lib/cmake/HyRemote`) |

### Prove it works immediately

In your `CMakeLists.txt`:

```cmake
find_package(HyRemote CONFIG REQUIRED)
message(STATUS "HyRemote found: ${HyRemote_DIR}")
message(STATUS "HyRemote version: ${HyRemote_VERSION}")
```

Configure should print the real path and version. If not, see the first row of STEP 8.

---

## STEP 4 - One complete copy-pasteable C++ project

No fragments. Copy this whole project.

```text
MyApp/
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
4. The `remote` object's lifetime must **cover the whole run**: it is the window's remote-access host. Constructing it
   on `main`'s stack and letting it die after `app.exec()` is the simplest correct shape.

---

## STEP 5 - Build your project

### Windows PowerShell

```powershell
cd MyApp
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_INSTALL_PREFIX="$PWD/deploy" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64;D:/sdk/HyRemote"
cmake --build build
cmake --install build
```

### Linux

```sh
cd MyApp
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/deploy" \
  -DCMAKE_PREFIX_PATH="/opt/Qt/6.8.3/gcc_64;/opt/hyremote-sdk"
cmake --build build
cmake --install build
```

> **The deploy prefix must be absolute.** `cmake --install build --prefix deploy` (a relative path) fails on Qt 6.8
> with `CMake Error at .../Qt6CoreDeploySupport.cmake:38 (message): Given qt.conf path is not an absolute path`.
> Give the prefix at **configure** time via `-DCMAKE_INSTALL_PREFIX=<absolute path>` and then run plain
> `cmake --install build`. This was measured on our clean consumer; a relative prefix fails reproducibly.

### Which executable you actually run

`hyremote_deploy()` puts the runnable tree under the **install prefix**, not in the build directory:

```text
deploy/
  bin/MyApp.exe                     <-- this is the one you run
  bin/libHyRemoteRemoteAccess.dll   <-- plus the shared runtime
  bin/Qt6*.dll  plugins/  qml/ ...  <-- Qt runtime closure and HyRemote payloads
```

```powershell
.\deploy\bin\MyApp.exe
```

```sh
./deploy/bin/MyApp
```

There is also an executable in `build/`, but that one needs the SDK on its search path to start. For a person, or for
another machine, run the one in `deploy/`: it is self-contained and does not need the SDK.

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
   # Windows PowerShell
   Get-NetIPAddress -AddressFamily IPv4 |
     Where-Object { $_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.254.*" } |
     Format-Table InterfaceAlias, IPAddress, PrefixLength
   ```

   ```sh
   # Linux
   ip -4 addr show scope global
   ```

   Exclude `127.*` (loopback) and `169.254.*` (APIPA). Pick a virtual adapter (VMware/VirtualBox/Hyper-V) only if that
   is genuinely the path you want.

2. **Confirm the port is listening** (on the host)

   ```powershell
   Get-NetTCPConnection -LocalPort 5921 -State Listen
   ```

   ```sh
   ss -ltnp | grep 5921
   ```

   No listener means the application is not running or its start failed - see STEP 8.

3. **On another machine on the same LAN, open a maintained VNC viewer**
   (TigerVNC, RealVNC VNC Viewer, or any standard RFB viewer.)

4. **Connect**

   ```text
   <HOST_LAN_IP>:5921
   ```

5. You should see the application window.
6. If remote input is enabled: pointer motion and buttons, wheel, keyboard and text should all work.
7. Disconnect the viewer.
8. Reconnect - the application is still running, and a second connection should succeed.
9. Stop remote access: `remote.stop()` (or quit the application); confirm `:5921` is no longer listening.

> HyRemote ships no viewer of its own. Any standard RFB viewer works; which one you use is your choice and this guide
> does not assume a particular vendor.

---

## STEP 7 - The four integration routes (four peers)

The four routes have **no ranking**. All of them follow the same flow: **get the SDK -> point CMake at it -> build ->
deploy -> run -> connect a viewer**. Only the amount of application change differs.

| Route | Application change | Deploy | Run |
| --- | --- | --- | --- |
| **C++ API** | link `HyRemote::RemoteAccess`, call the API | `hyremote_deploy(TARGET MyApp)` | normal launch |
| **QML API** | `import HyRemote` | `hyremote_deploy(TARGET MyApp QML)` | normal launch |
| **Generic Plugin** | **zero code** (application links nothing from HyRemote) | `hyremote_deploy(TARGET MyApp GENERIC)` | `MyApp -plugin hyremote` |
| **QPA (Transparent)** | **zero code** (application links Qt only) | `hyremote_deploy(TARGET MyApp QPA)` | `MyApp -platform hyremote` |

### C++ API

STEP 4 is this route: `find_package(HyRemote CONFIG REQUIRED)` -> link `HyRemote::RemoteAccess` -> `remote.start()` /
`remote.stop()`.

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

The application links **nothing** from HyRemote and includes no HyRemote header. The `CMakeLists.txt` only carries the
deployment statement:

```cmake
find_package(HyRemote CONFIG REQUIRED)   # only to obtain hyremote_deploy()
hyremote_deploy(TARGET MyApp GENERIC)
```

```powershell
.\deploy\bin\MyApp.exe -plugin hyremote
```

Without the argument the same binary is an ordinary application (the native platform stays `windows`/`xcb`).

### QPA (Transparent QPA Proxy, zero code)

The application uses **Qt only**; HyRemote takes over as a platform plugin while the native platform remains the
delegate:

```cmake
hyremote_deploy(TARGET MyApp QPA)
```

```powershell
.\deploy\bin\MyApp.exe -platform hyremote
```

**QPA's hard constraint**: it is bound to **Qt's exact private ABI**. The Qt used for deployment must be **exactly the
same version and kit** as your application's (the reference is Qt 6.8.3 MinGW/x86_64). A version or kit mismatch means
unavailable - there is no "close enough".

---

## STEP 8 - The most common failures

| Symptom | Cause | Fix |
| --- | --- | --- |
| `Could not find a package configuration file provided by "HyRemote"` | CMake cannot see the SDK | Add the **SDK root** to `CMAKE_PREFIX_PATH`, or pass `-DHyRemote_DIR=<SDK>/lib/cmake/HyRemote`. `HyRemote_DIR` must be the directory holding `HyRemoteConfig.cmake` |
| Link or runtime Qt version/kit mismatch | The application and the SDK used different Qt kits (MSVC vs MinGW, or different versions) | Use one Qt 6.8.3 kit on both sides; QPA is the strictest |
| Deploy fails with `Given qt.conf path is not an absolute path` | The install prefix is relative | Pass `-DCMAKE_INSTALL_PREFIX=<absolute path>` at configure time, then plain `cmake --install build` (do not use a relative `--prefix`) |
| Windows cannot find a DLL at launch | You ran the `build/` executable instead of the deployed one | Run `deploy/bin/MyApp.exe`; `hyremote_deploy()` already placed the runtime closure next to it |
| `This application failed to start because no Qt platform plugin could be initialized` | `plugins/platforms/` is missing from the deploy tree | Deploy with `hyremote_deploy()`; do not hand-copy just the exe |
| QML: `module "HyRemote" is not installed` | The QML module was not deployed, or the `QML` keyword was omitted | `hyremote_deploy(TARGET MyApp QML)` and check that `deploy/qml/HyRemote/qmldir` exists |
| `-plugin hyremote` changes nothing | The Generic payload was not deployed | Deploy with `hyremote_deploy(TARGET MyApp GENERIC)`; check `deploy/plugins/generic/` contains `libqhyremote.dll` |
| `-platform hyremote` reports unavailable | The SDK has no QPA payload, or Qt is not an exact match | QPA requires HyRemote built with the QPA proxy and an **exact** Qt match |
| `:5921` is not listening | The application is not running, `start()` failed, or the state is `Unavailable` | Check the application log and `remote.lastError()`; `Unavailable` means the configured interface has no usable IPv4 right now - the listener returns by itself when it does |
| `:5921` is already in use | Another process is listening | Change the port with `remote.setPort(...)` (only while `Stopped`), or stop the other process |
| The viewer cannot connect, but locally it works | Loopback was used, or the wrong address was given | Use the host LAN IPv4; confirm `0.0.0.0:5921` is listening rather than only `127.0.0.1` |
| Another machine cannot connect at all | Windows Firewall is blocking, or the machines are not on the same LAN | Allow the application's inbound connection on the host (or allow TCP 5921); confirm both machines are on the same subnet |
| The picture works but pointer and keyboard do nothing | Remote input is off by default | `remote.setRemoteInputEnabled(true)` (`remoteInputEnabled: true` in QML) |
| After switching to another application locally, focus behaves oddly remotely | The application-scoped focus limitation | Known limitation: focus semantics are application-scoped and OS-level focus following across applications is not promised |

---

## Update, disable and roll back

| Goal | How |
| --- | --- |
| Upgrade the SDK | Re-configure, rebuild and redeploy against the new SDK root; the runtime and plugins update with the deployment |
| Upgrade only the runtime | Overwrite the runtime library and plugin payloads in `deploy/` from the new SDK (no application change) |
| Disable remote access | Do not call `start()`, or call `stop()` while running |
| Remove entirely | Drop `hyremote_deploy()`, the `HyRemote::RemoteAccess` link and the header include, then rebuild and redeploy |

None of these steps require any assumption about where the source tree is: your project depends on exactly one external
input, the **SDK root**.

---

## Next

- The four routes in depth: `docs/getting-started/cpp.md`, `qml.md`, `generic.md`, `qpa-proxy.md`
- Deployment detail: `docs/guide/deployment.md`
- Troubleshooting manual: `docs/guide/troubleshooting.md`
- Real open-source integration studies: `examples/real-world/`
- Architecture and input model (reference, not an entry point): `docs/architecture.md`, `docs/input-model.md`
