# 安装、构建与集成准备

> 语言 / Language：**中文** ｜ [English](../en/guide/install.md)

本文档说明如何获得 HyRemote、如何构建它，以及如何把它接入一个 Qt 应用（已安装 SDK 或源码两种方式）。
面向最终用户与产品最终形态；验收过程、里程碑跟踪与内部工程约定不在此文，见 [`docs/README.md`](../README.md) 的"内部文档"一节。

## 1. V1 参考支持矩阵

| 维度 | V1 承诺 |
| --- | --- |
| 操作系统 | Windows x86_64、Linux x86_64（桌面）。嵌入式 Linux/EGLFS **不在** V1 支持范围内 |
| Qt | **精确 Qt 6.8.3** 为参考版本；Bounded RFB 传输与三类集成方式均以该版本为验收基线 |
| 编译器 | Windows：MSVC x64（C++17）；Linux：GCC x86_64（C++17） |
| 集成方式 | ① Embedded C++（唯一共享库）② Declarative QML（`import HyRemote`）③ Transparent QPA Proxy（`-platform hyremote`） |
| QPA 约束 | Transparent QPA 与 Qt 6.8.3 的私有 QPA ABI 精确耦合；Windows 复用 `qwindows`、Linux 复用 `qxcb` 原生委托 |
| 查看器 | 任意标准 VNC 客户端，默认连接 `127.0.0.1:5921` |

> **产品状态**：V1.0.0.0 验收尚未完成。候选实现只有在必需的可执行证据与物理证据实际通过后，才构成 **Supported** 声明；
> 在此之前请把它当作候选版本看待。当前状态见 [`docs/compatibility.md`](../compatibility.md)。

## 2. 两种获取方式：一次配置只能选一种

HyRemote 提供两条获取路径，二者的**应用 API、产物形态与部署助手完全相同**，但获取步骤不同：

| 方式 | 使用场景 | 入口 |
| --- | --- | --- |
| **已安装 SDK** | 使用预构建/安装前缀，团队共享一个 SDK | `find_package(HyRemote CONFIG REQUIRED)` |
| **源码接入** | 随应用一并 vendored、交叉编译、需要改源码 | `add_subdirectory(third_party/HyRemote)` |

**硬性约束**：同一次 CMake 配置中只能存在**一个** HyRemote 获取源——要么一个已安装前缀，要么一棵源码树。
不要在同一次配置里同时 `find_package(HyRemote)` 与 `add_subdirectory(HyRemote)`，也不要混用两个不同的已安装前缀。
V1 对这类混用**直接失败**，而不是把某一处的运行时与另一处构建出的 QML/QPA 元数据拼在一起。（对同一个已安装前缀重复调用 `find_package` 是允许的。）

## 3. 构建 HyRemote 本身

### 3.1 普通产品构建

默认配置刻意**只构建产品**：仅构建标准 C++ 路径（`HyRemote::RemoteAccess`），不构建仓库测试、示例与研究代码。

Windows（x64 MSVC 开发者环境）：

```bat
cmake -S . -B build -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --parallel
```

Linux：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.3/gcc_64
cmake --build build --parallel
```

### 3.2 生成可安装的 SDK

追加安装前缀并执行安装：

```bash
cmake -S . -B build-hyremote -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<toolchain> \
  -DCMAKE_INSTALL_PREFIX=/path/to/hyremote-sdk
cmake --build build-hyremote --parallel
cmake --install build-hyremote
```

生成的安装前缀即可按第 4 节的方式被 `find_package(HyRemote CONFIG REQUIRED)` 消费。

### 3.3 可选集成包

QML 与 Transparent QPA 是**可选载荷**，按需开启：

```text
-DHYREMOTE_BUILD_QML_API=ON      # 提供 import HyRemote
-DHYREMOTE_WITH_QPA_PROXY=ON     # 提供 qhyremote 平台插件（需精确 Qt 6.8.3 及匹配的私有 Gui 开发包）
```

正常 C++ 使用**不需要**这两个选项，也不需要 Qt 私有开发包。开启可选项不会扩大普通 C++ 应用的链接面。

### 3.4 维护者/验收构建

仓库自身的验证是显式开启的，不会隐藏在普通产品构建里。下面给出**完整的**配置、构建与测试链（Windows）：

```bat
cmake -S . -B build-test -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64 ^
  -DHYREMOTE_BUILD_TESTS=ON ^
  -DHYREMOTE_BUILD_EXAMPLES=ON
cmake --build build-test --parallel
set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%CD%\build-test\remoteaccess;%PATH%
ctest --test-dir build-test --output-on-failure
```

Linux：

```bash
cmake -S . -B build-test -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.3/gcc_64 \
  -DHYREMOTE_BUILD_TESTS=ON \
  -DHYREMOTE_BUILD_EXAMPLES=ON
cmake --build build-test --parallel
export LD_LIBRARY_PATH=/opt/Qt/6.8.3/gcc_64/lib:${LD_LIBRARY_PATH}   # 仅当本地 Qt 套件未提供合适的运行时查找时
ctest --test-dir build-test --output-on-failure
```

`-DHYREMOTE_BUILD_QML_API=ON` 与 `-DHYREMOTE_WITH_QPA_PROXY=ON` 可按需加入同一次配置，以覆盖 QML/QPA 的验证范围。

上面的 `PATH` / `LD_LIBRARY_PATH` 增补是**构建树测试关注点**，不是部署契约：部署后的应用必须通过部署机制获得 Qt/HyRemote 运行时文件，不得依赖原始 SDK 或构建树。

## 4. 消费方式 A：已安装 SDK

### 4.1 契约

V1 已安装 SDK 只暴露**一个正常的 C++ 产品目标**：`HyRemote::RemoteAccess`（共享库）。

- Core 静态组合在门面之后，**不作为 SDK 目标安装/导出**；
- QML 模块通过 `import HyRemote` 消费，其支撑库不是第二个 C++ SDK 目标；
- Transparent QPA 通过 `hyremote_deploy(... QPA)` 消费，**不是应用链接目标**（安装的 SDK 不导出 `HyRemote::QpaPlatform`）。

应用代码不需要、也不应该逐个发现或链接 Core/传输/采集/输入/QPA 内部目标。

### 4.2 最小应用

```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.8.3/<toolchain>;/path/to/hyremote-sdk"
```

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
find_package(HyRemote CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE Qt6::Widgets HyRemote::RemoteAccess)
```

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&window);
remote.start();
```

默认值即安全默认值：监听回环地址、端口 5921、远程输入关闭。各项 setter 是**可选策略控制**，不是必需的初始化步骤。

默认端口可由集成方在**构建期**定义：配置时传 `-DHYREMOTE_DEFAULT_PORT=<端口>`（不传即 `5921`）。三种接入方式共用这一个默认值——C++ 与 QML 都从 Core 的默认值起步，QPA 代理在平台串未给出 `hyremote-port` 时也使用同一数字；运行期仍可逐进程覆盖：`RemoteAccess::setPort()`、QML 的 `port` 属性、`-platform "hyremote:hyremote-port=<端口>"`。
使用 Qt Quick 时，按应用自身需要请求 Qt Quick 组件，HyRemote 目标保持不变；`find_package(HyRemote)` 不会强迫应用解析它并不使用的 Widgets/Quick/QML 模块。

### 4.3 `find_package(HyRemote)` 的语义

它解析 `HyRemote::RemoteAccess` 的公开依赖、加载该导出目标、发布可选的 QML/QPA 载荷元数据，并提供 `hyremote_deploy()`。

它**不会**把 Core、QML 支撑库或 `qhyremote` 变成应用可选的链接目标。它由 HyRemote 的安装/导出规则生成，
**不应**因为某个源码目录被 `add_subdirectory` 进别的项目就假定它存在。

## 5. 消费方式 B：源码接入

源码接入适用于 vendored、交叉编译或需要改源码的场景；它**不要求**为 HyRemote 源码树本身调用 `find_package(HyRemote)`。

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)

add_subdirectory(third_party/HyRemote)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Qt6::Widgets HyRemote::RemoteAccess)
```

作为子项目被引入时，HyRemote 自身的测试、示例与研究代码**自动默认为 OFF**，应用不需要知道或覆盖这些仅开发者使用的开关。
`add_subdirectory(... EXCLUDE_FROM_ALL)` 同样受支持：HyRemote 的源码产物保持内部构建目标，部署助手只为落地所选载荷添加必要的本地构建依赖，
**不会**把它们变成应用的链接依赖（Transparent QPA 应用即使从源码构建 `qhyremote` 与 `HyRemoteRemoteAccess`，其自身仍只链接 Qt）。

按需开启可选集成包：

```cmake
set(HYREMOTE_BUILD_QML_API ON CACHE BOOL "" FORCE)       # import HyRemote
set(HYREMOTE_WITH_QPA_PROXY ON CACHE BOOL "" FORCE)      # 精确 Qt 6.8.3 的 qhyremote 插件
add_subdirectory(third_party/HyRemote EXCLUDE_FROM_ALL)
```

这些选项**不会**创建平行的 Core/传输栈：QML 仍是薄封装，QPA 仍是插件入口，二者共用同一个共享运行时。
源码接入继承调用方的编译器、sysroot、CMake 工具链文件与目标 Qt SDK，因此也是未来嵌入式 Linux 家族的扩展路径——
它不允许引入第二套项目专用构建系统或更复杂的应用 API。

## 6. 部署：`hyremote_deploy()`

部署由 HyRemote 拥有的**唯一入口**负责：

```cmake
hyremote_deploy(TARGET MyCppApp)          # Embedded C++
hyremote_deploy(TARGET MyQmlApp QML)      # Declarative QML
hyremote_deploy(TARGET ExistingQtApp QPA) # Transparent QPA
hyremote_deploy(TARGET ExistingQmlApp QML QPA)  # QML + QPA 组合
```

```cmake
install(TARGETS MyApp RUNTIME DESTINATION bin)
hyremote_deploy(TARGET MyApp)
```

助手负责所选载荷的落地（共享运行时、Qt 依赖、QML 插件文件、`qhyremote`）。
应用**不要**手工复制 DLL/SO、`qmldir`、插件文件，也不要设置 SDK 专用的 `QT_PLUGIN_PATH` / `QT_QPA_PLATFORM_PLUGIN_PATH` / `LD_LIBRARY_PATH` 覆盖。
完整部署契约见 [`docs/guide/deployment.md`](deployment.md)。

## 7. 干净部署要求

构建成功**不构成**验收证据。部署后的应用必须在自己的部署前缀下运行，不得依赖：HyRemote 构建树、原 SDK 安装前缀，
或 `PATH`、`LD_LIBRARY_PATH`、`QT_PLUGIN_PATH`、`QT_QPA_PLATFORM_PLUGIN_PATH`、`QML2_IMPORT_PATH`、`QML_IMPORT_PATH` 之类的手工覆盖。

Linux 上的 V1 夹具还会核对 HyRemote/Qt 共享对象的实际解析来源，避免残留的 SDK/构建树 RUNPATH 伪装成干净部署；
Windows 上的干净运行检查会把 `PATH` 收窄到部署目录加必需的系统目录。

## 8. 平台要点

| 主题 | Windows x86_64 | Linux x86_64 |
| --- | --- | --- |
| 参考 Qt | `C:\Qt\6.8.3\msvc2022_64` | `/opt/Qt/6.8.3/gcc_64` |
| 运行测试的构建树补充 | 把 Qt `bin` 与构建树的 `remoteaccess` 目录加入 `PATH` | 必要时设置 `LD_LIBRARY_PATH` |
| QPA 原生委托 | `qwindows` | `qxcb` |
| 离屏/软件渲染 | 可用于验证"协议→应用"路径，**不**构成本地可见显示/输入共存的证据 | 同左（`offscreen` 等） |

桌面 Linux 的结果**不**推导出嵌入式 Linux/EGLFS 支持。最终平台验收必须记录精确的 Qt/编译器/QPA/查看器配置。

## 9. 运行示例与查看器

开启 `-DHYREMOTE_BUILD_EXAMPLES=ON` 后，V1 候选中包含 `widgets-basic`、`quick-basic`、`qml-basic`、
`qpa-proxy-existing-app`、`remote-support-showcase`（按已启用的集成包构建）。

E1/E2 使用同一个公开门面 `HyRemote::RemoteAccess`，默认**仅观看**；只有受控测试环境才使用显式的远程输入选项。
查看器连接见 [`viewer-connection.md`](viewer-connection.md)。

## 10. 安全与支持边界

当前 Bounded RFB 正确性传输使用 **SecurityType None**：既无传输认证，也无传输加密。
默认绑定回环地址、远程输入默认关闭——不要把该基线直接暴露到不可信网络或公网。详见 [`docs/security.md`](../security.md) 与 [`SECURITY.md`](../../SECURITY.md)。

支持边界以证据为准：托管/离屏构建本身不构成"本地可见显示/输入与远程共存"的证明。
未通过验收的能力不会被宣称为 Supported，明确限制见 [`docs/known-limitations.md`](../known-limitations.md)。

## 11. 相关文档

- 集成方式（选择其一）：[Embedded C++](../getting-started/cpp.md) ｜ [Declarative QML](../getting-started/qml.md) ｜ [Transparent QPA](../getting-started/qpa-proxy.md)（迁移至 `guide/` 后路径会同步更新）
- 部署与打包：[`deployment.md`](deployment.md)
- 兼容与限制：[`compatibility.md`](../compatibility.md) ｜ [`known-limitations.md`](../known-limitations.md)
- 排错：[`troubleshooting.md`](troubleshooting.md)
