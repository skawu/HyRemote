# 将 HyRemote 集成到你的 Qt 项目

这是 HyRemote 的**唯一默认入口**。按顺序读完并执行，你就能把一个普通的 Qt 应用变成可被 VNC viewer 远程查看、并在需要时远程控制的应用——不需要先读架构、输入模型或仓库布局文档。

你已经有自己的 Qt 项目。下面从"我现在去拿 HyRemote"开始，一步不漏。

**本文所有命令都在真实的 `build/install` 安装树上实际执行过**（Windows x86_64 / Qt 6.8.3 / MinGW / CMake 3.21+ / C++17；Linux 侧给出对应写法）。文档里出现的每个路径与文件名都取自真实产物，不是示意。

---

## 0. 你需要准备什么

| 项目 | 要求 |
| --- | --- |
| Qt | **6.8.3**（参考版本；HyRemote 与你的应用必须使用同一个 Qt kit） |
| CMake | 3.21 或更高 |
| C++ | C++17 |
| 平台 | Windows x86_64、Linux x86_64 |
| 网络 | HyRemote 默认是**未认证、未加密**的，只适用于**可信局域网**，不适合暴露到 Internet |

---

## STEP 1 — 获得 HyRemote

你只需要两种东西之一：一个**已安装的 SDK 目录**。两者都得到同一个东西：`<HYREMOTE_SDK_ROOT>`。

### 方案 A：下载官方 Release 制品（推荐给只想用的人）

到 GitHub Releases 下载对应平台的归档并解压。解压出来的目录就是你的 `<HYREMOTE_SDK_ROOT>`——它里面已经同时包含**头文件、CMake package、共享运行库和插件载荷**，不需要你再去凑 Qt 的插件。

### 方案 B：从源码构建

```powershell
# Windows PowerShell
git clone https://github.com/skawu/HyRemote.git
cd HyRemote
.\build.cmd install
```

```sh
# Linux / macOS(仅作构建宿主) POSIX shell
git clone https://github.com/skawu/HyRemote.git
cd HyRemote
sh ./build.cmd install
```

完成后：

| 目录 | 含义 |
| --- | --- |
| `<repo>/build/` | **构建中间目录**。这是 CMake 的工作区，不是给应用消费的 |
| `<repo>/build/install/` | **真正的 SDK / runtime tree**。这就是 `<HYREMOTE_SDK_ROOT>`，你的应用只消费这个 |

**不要消费**：`src/` 源码树、`build/` 里的内部 target、`examples/` 的构建树。它们都不是稳定契约，换一个版本或换一台机器就会消失。你的应用只应该依赖 `build/install/`（或发布制品）。

> 需要把 SDK 放到别处？给 `install` 传一个 install prefix（例如 `.\build.cmd install --cmake=CMAKE_INSTALL_PREFIX=D:/sdk/HyRemote`），或在 CMake 里使用 `-DCMAKE_INSTALL_PREFIX`。参数细节见 `docs/guide/install.md`。

---

## STEP 2 — 库到底在哪里

先看真实目录。**不要背文档，随时可以自己列**：

```powershell
# Windows PowerShell
Get-ChildItem -Recurse -Depth 2 <HYREMOTE_SDK_ROOT> | Select-Object FullName
```

```sh
# Linux
find <HYREMOTE_SDK_ROOT> -maxdepth 3 | sort
```

真实的安装树长这样（Windows 实测）：

```text
<HYREMOTE_SDK_ROOT>/
  HYREMOTE-MANIFEST.txt          # 这份 SDK 的身份与配置事实
  include/
    HyRemote/
      RemoteAccess.h             # 唯一的公开头文件
      RemoteAccessExport.h
  lib/
    libHyRemoteRemoteAccess.dll.a             # 链接用的 import library
    cmake/HyRemote/
      HyRemoteConfig.cmake                    # find_package 的入口
      HyRemoteConfigVersion.cmake
      HyRemoteTargets.cmake
      HyRemoteTargets-release.cmake
      HyRemoteDeploy.cmake                    # hyremote_deploy() 的实现
    HyRemote/plugins/generic/libqhyremote.dll
    HyRemote/plugins/platforms/libqhyremote.dll
    HyRemote/qml/HyRemote/hyremote-qmlplugin.dll
  bin/
    libHyRemoteRemoteAccess.dll               # 运行时共享库
    ...Qt 运行库与 qt.conf...
  plugins/
    generic/libqhyremote.dll                  # Generic 插件
    platforms/libqhyremote.dll                # QPA 插件
    platforms/qwindows.dll                    # 原生平台插件
  qml/HyRemote/qmldir                         # QML 模块定义
  share/  translations/
```

### 六个最常见的问题，直接回答

| 问题 | 答案（Windows 实测） | Linux 对应 |
| --- | --- | --- |
| HyRemote 的头文件在哪里？ | `<SDK>/include/HyRemote/RemoteAccess.h` | 同名 |
| CMake package 在哪里？ | `<SDK>/lib/cmake/HyRemote/HyRemoteConfig.cmake` | 同名 |
| DLL / .so 在哪里？ | 运行时 `<SDK>/bin/libHyRemoteRemoteAccess.dll`；链接用 `<SDK>/lib/libHyRemoteRemoteAccess.dll.a` | `<SDK>/lib/libHyRemoteRemoteAccess.so`（同目录即是链接与运行时库） |
| Generic plugin 在哪里？ | `<SDK>/plugins/generic/libqhyremote.dll` | `<SDK>/plugins/generic/libqhyremote.so` |
| QPA plugin 在哪里？ | `<SDK>/plugins/platforms/libqhyremote.dll` | `<SDK>/plugins/platforms/libqhyremote.so` |
| QML module 在哪里？ | `<SDK>/qml/HyRemote/`（`qmldir` + `hyremote-qmlplugin.dll`） | `<SDK>/qml/HyRemote/` |

> Linux 侧文件名沿用同一命名契约（`lib` + 名字 + 平台后缀）。**任何平台上唯一权威的说法是你自己列出来的那棵树**——用上面两条命令即可。

---

## STEP 3 — 让你的 CMake 找到 HyRemote

HyRemote 是一个标准 CMake package：一个 `HyRemoteConfig.cmake`，导出目标 `HyRemote::RemoteAccess`。有两种合法方法，任选其一。

### 方法 1：给 SDK 根（推荐，最省事）

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

`CMAKE_PREFIX_PATH` 是**一串目录**：Qt 的 kit 目录 + HyRemote 的 **SDK 根**。分号分隔（Windows PowerShell 里用引号包住整串）。CMake 会在每个目录下按标准布局（`lib/cmake/<name>/`）找到 package。

### 方法 2：直接指定 package 目录

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64" `
  -DHyRemote_DIR="D:/sdk/HyRemote/lib/cmake/HyRemote"
```

### 两者的区别（不要猜）

| 变量 | 指向 |
| --- | --- |
| `CMAKE_PREFIX_PATH` | **SDK 根**（`.../HyRemote-SDK`）。CMake 自己在里面找 `lib/cmake/HyRemote/HyRemoteConfig.cmake` |
| `HyRemote_DIR` | **`HyRemoteConfig.cmake` 所在的那个目录本身**（`.../lib/cmake/HyRemote`） |

### 立刻验证它成功

在你的 `CMakeLists.txt` 里：

```cmake
find_package(HyRemote CONFIG REQUIRED)
message(STATUS "HyRemote found: ${HyRemote_DIR}")
message(STATUS "HyRemote version: ${HyRemote_VERSION}")
```

配置阶段应当打印出真实路径与版本。若失败，见 STEP 8 第一条。

---

## STEP 4 — 一个完整可复制的 C++ 项目

不要用片段。下面这个项目可以直接整个复制走。

```text
MyApp/
  CMakeLists.txt
  main.cpp
```

`CMakeLists.txt`：

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

# 把共享运行库、Qt 运行时闭包和 HyRemote 插件载荷一起部署到可运行目录。
hyremote_deploy(TARGET MyApp)
```

`main.cpp`：

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

    // 绑定到顶层窗口。构造是惰性的：此时不会打开任何监听。
    HyRemote::RemoteAccess remote(&window);

    // 需要远程控制时才打开（默认是关闭的）。
    remote.setRemoteInputEnabled(true);

    if (!remote.start()) {
        // 启动失败是产品级诊断，不会静默降级。
        qWarning("HyRemote failed to start");
    }

    const int code = app.exec();
    remote.stop();   // 运行期结束再停
    return code;
}
```

必须理解的四点：

1. `HyRemote::RemoteAccess` 是你要链接的 **CMake target**，名字就是 `HyRemote::RemoteAccess`。
2. **不要链接 Core**。Core/Session/Transport 都是私有实现，不构成公开契约。
3. **不要手动指定 `.lib` / `.dll` / `.so`，也不要复制任何内部库**。链接靠上面的 target，部署靠 `hyremote_deploy()`。
4. `remote` 对象的生命周期必须**覆盖整个运行期**（它是窗口的远程访问宿主）；把它建在 `main` 的栈上、在 `app.exec()` 之后才析构，是最简单正确的写法。

---

## STEP 5 — 构建你自己的项目

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

> **部署前缀必须是绝对路径。** `cmake --install build --prefix deploy`（相对路径）在 Qt 6.8 上会失败：
> `CMake Error at .../Qt6CoreDeploySupport.cmake:38 (message): Given qt.conf path is not an absolute path`。
> 把前缀放在 **configure 阶段**用 `-DCMAKE_INSTALL_PREFIX=<绝对路径>` 给出，然后直接 `cmake --install build`。
> 这一步在我们的 clean consumer 上实测通过；相对前缀会稳定失败。

### 现在到底该运行哪个 exe

`hyremote_deploy()` 把可运行的东西放到了**安装前缀下**，不是构建目录里：

```text
deploy/
  bin/MyApp.exe                     <-- 这就是你要运行的那个
  bin/libHyRemoteRemoteAccess.dll   <-- 以及共享运行库
  bin/Qt6*.dll  plugins/  qml/ ...  <-- Qt 运行时闭包与 HyRemote 载荷
```

```powershell
.\deploy\bin\MyApp.exe
```

```sh
./deploy/bin/MyApp
```

`build/` 里也有一个 exe，但那个**依赖 SDK 在搜索路径上**才能启动。要给真人、给另一台机器用，就运行 `deploy/` 里的那个——它是自洽的：不带 SDK 也不会去找 SDK。

---

## STEP 6 — 运行与连接

### 默认实际事实（V0.2+，如实写在这里）

| 事实 | 默认值 |
| --- | --- |
| 监听 | `0.0.0.0:5921`（本机所有 IPv4 接口，**不只是 loopback**） |
| 认证 | **关闭**（`none`） |
| 传输加密 | **关闭**（`none`） |
| 远程输入 | **默认关闭**（需要时用 `setRemoteInputEnabled(true)` 打开） |
| 适用范围 | **仅可信局域网**，**不适合暴露到 Internet** |

### 一步一步连上去

1. **找到 Host 的局域网 IPv4**

   ```powershell
   # Windows PowerShell：列出 IPv4 及接口状态
   Get-NetIPAddress -AddressFamily IPv4 |
     Where-Object { $_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.254.*" } |
     Format-Table InterfaceAlias, IPAddress, PrefixLength
   ```

   ```sh
   # Linux
   ip -4 addr show scope global
   ```

   排除 `127.*`（loopback）与 `169.254.*`（APIPA）。虚拟机网卡（VMware/VirtualBox/Hyper-V）只有在确实用它时才选。

2. **确认端口在监听**（在 Host 上）

   ```powershell
   Get-NetTCPConnection -LocalPort 5921 -State Listen
   ```

   ```sh
   ss -ltnp | grep 5921
   ```

   看不到监听就是应用没起来或启动失败——见 STEP 8。

3. **在另一台同一局域网的机器上打开一个仍在维护的 VNC viewer**
   （TigerVNC、RealVNC VNC Viewer 等任意标准 RFB viewer 均可。）

4. **连接**

   ```text
   <HOST_LAN_IP>:5921
   ```

5. 应当看到 Host 上应用的窗口画面。
6. 若启用了远程输入：鼠标移动/按键、滚轮、键盘输入与文本都应生效。
7. 断开 viewer。
8. 重新连接——应用仍在运行，应当能再次连上。
9. 停止远程访问：`remote.stop()`（或直接退出应用）；确认 `:5921` 不再监听。

> HyRemote 不提供自己的 viewer。任何标准 RFB viewer 都可以；实际用哪个由你决定，本文不假定某一家。

---

## STEP 7 — 四条 integration route（四条对等路线）

四条路线**没有主次**，都走同一套流程：**获得 SDK → CMake 找 SDK → 构建 → 部署 → 运行 → viewer 连接**。区别只在于"应用需要改多少"。

| 路线 | 应用要改什么 | 部署 | 运行 |
| --- | --- | --- | --- |
| **C++ API** | 链接 `HyRemote::RemoteAccess`，调用 API | `hyremote_deploy(TARGET MyApp)` | 正常启动 |
| **QML API** | `import HyRemote` | `hyremote_deploy(TARGET MyApp QML)` | 正常启动 |
| **Generic Plugin** | **零代码**（应用不链接 HyRemote） | `hyremote_deploy(TARGET MyApp GENERIC)` | `MyApp -plugin hyremote` |
| **QPA（Transparent）** | **零代码**（应用只链 Qt） | `hyremote_deploy(TARGET MyApp QPA)` | `MyApp -platform hyremote` |

### C++ API

STEP 4 就是它：`find_package(HyRemote CONFIG REQUIRED)` → 链接 `HyRemote::RemoteAccess` → `remote.start()` / `remote.stop()`。

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

### Generic Plugin（零代码）

应用**完全不链接 HyRemote**、不包含 HyRemote 头文件。`CMakeLists.txt` 里只有部署语句：

```cmake
find_package(HyRemote CONFIG REQUIRED)   # 只为拿到 hyremote_deploy()
hyremote_deploy(TARGET MyApp GENERIC)
```

```powershell
.\deploy\bin\MyApp.exe -plugin hyremote
```

不发生 HyRemote 代码时，同一个二进制就是普通应用（原生平台仍是 `windows`/`xcb`）。

### QPA（Transparent QPA Proxy，零代码）

应用本身**只用 Qt**；HyRemote 作为平台插件接管，原生平台仍是代理目标：

```cmake
hyremote_deploy(TARGET MyApp QPA)
```

```powershell
.\deploy\bin\MyApp.exe -platform hyremote
```

**QPA 的硬约束**：它与 **Qt 的私有 ABI 精确绑定**。部署时用的 Qt 必须与你的应用**完全同一个版本与同一个 kit**（本参考为 Qt 6.8.3 MinGW/x86_64）。版本或 kit 不一致就是不可用，不存在"差不多能用"。

---

## STEP 8 — 最常见的错误

| 症状 | 原因 | 处理 |
| --- | --- | --- |
| `Could not find a package configuration file provided by "HyRemote"` | CMake 没看到 SDK | 给 `CMAKE_PREFIX_PATH` 加 **SDK 根**，或用 `-DHyRemote_DIR=<SDK>/lib/cmake/HyRemote`。注意 `HyRemote_DIR` 要指到 `HyRemoteConfig.cmake` 所在目录本身 |
| 链接或运行期报 Qt 版本/kit 不匹配 | 应用与 SDK 用了不同的 Qt kit（MSVC vs MinGW，或不同版本） | 两边用同一个 Qt 6.8.3 kit；QPA 路线尤其严格 |
| 部署阶段报 `Given qt.conf path is not an absolute path` | 安装前缀是相对路径 | 在 configure 阶段用 `-DCMAKE_INSTALL_PREFIX=<绝对路径>`，再 `cmake --install build`（不要用相对 `--prefix`） |
| Windows 启动报找不到 DLL | 运行了 `build/` 里的 exe，而不是部署后的 | 运行 `deploy/bin/MyApp.exe`（`hyremote_deploy()` 已把运行库闭包放到旁边） |
| `This application failed to start because no Qt platform plugin could be initialized` | 部署目录里缺 `plugins/platforms/` | 用 `hyremote_deploy()` 部署，不要手工只拷 exe |
| QML 报 `module "HyRemote" is not installed` | 没有部署 QML 模块，或没有用 `QML` 关键字 | `hyremote_deploy(TARGET MyApp QML)`，并确认 `deploy/qml/HyRemote/qmldir` 存在 |
| 加了 `-plugin hyremote` 却毫无变化 | Generic 载荷没部署 | 用 `hyremote_deploy(TARGET MyApp GENERIC)` 部署；确认 `deploy/plugins/generic/` 里有 `libqhyremote.dll` |
| `-platform hyremote` 报不可用 | SDK 里没有 QPA 载荷，或 Qt 不精确匹配 | QPA 要求构建 HyRemote 时启用 QPA proxy，且 Qt **精确**一致 |
| `:5921` 没有监听 | 应用没起来、`start()` 失败、或状态是 `Unavailable` | 看应用日志与 `remote.lastError()`；`Unavailable` 表示配置的接口当前没有可用 IPv4，接口恢复后监听会自动回来 |
| `:5921` 被占用 | 别的进程已在监听 | 换端口：`remote.setPort(...)`（仅在 `Stopped` 时可改），或先停掉占用者 |
| viewer 连不上，但本机能连 | 选了 loopback，或连错 IP | 用宿主 LAN IPv4；确认 `0.0.0.0:5921` 在监听（不是只绑 `127.0.0.1`） |
| 另一台机器彻底连不上 | Windows Firewall 拦截，或不在同一 LAN | 在 Host 上允许该应用的入站连接（或放行 TCP 5921）；确认两台机器在同一子网 |
| 能看画面，但鼠标键盘无效 | 远程输入默认是关的 | `remote.setRemoteInputEnabled(true)`（QML 下 `remoteInputEnabled: true`） |
| 本机切到别的应用后再回来，远程端对焦行为异常 | application-scoped focus 限制 | 这是已知限制：焦点语义按应用作用域工作，不承诺跨应用的 OS 级焦点跟随 |

---

## 更新、停用与回滚

| 目标 | 做法 |
| --- | --- |
| 升级 SDK | 用新的 SDK 根重新 configure/构建/部署；运行时库与插件随部署一起更新 |
| 只升级运行时 | 用新 SDK 的部署输出覆盖 `deploy/` 里的运行库与插件载荷（不改应用代码） |
| 停用远程访问 | 不调用 `start()`；或在运行期 `stop()` |
| 完全移除 | 去掉 `hyremote_deploy()`、去掉 `HyRemote::RemoteAccess` 链接与头文件引用，重新构建部署 |

全程不需要假设源码树在哪里——你的项目只依赖 **SDK 根**这一个外部输入。

---

## 下一步

- 四条路线的背景与差异：`docs/getting-started/cpp.md`、`qml.md`、`generic.md`、`qpa-proxy.md`
- 部署细节：`docs/guide/deployment.md`
- 排查手册：`docs/guide/troubleshooting.md`
- 真实开源项目接入案例：`examples/real-world/`
- 架构与输入模型（参考资料，不是入口）：`docs/architecture.md`、`docs/input-model.md`
