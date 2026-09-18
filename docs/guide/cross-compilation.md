# 交叉编译到嵌入式目标

本页说明如何用项目自带的构建脚本与 **CMake toolchain 文件**，在 Windows 或 Linux 主机上为嵌入式目标
交叉编译 HyRemote。

> **先读这一条**：交叉编译成功只证明**能构建**，不证明能在目标板上运行。本项目对嵌入式 Linux/EGLFS 的
> 运行行为**尚未验证**（`docs/reference/compatibility.md` 中 RK3588 / EGLFS + OpenGL ES 一栏标为 *Unverified*），
> 因此交叉编译产物**不是**兼容性或支持声明，也不能作为 V1 x86 参考平台的发布证据。
>
>
> **另有一条与"未验证"不同的硬事实**：**Transparent QPA 在 EGLFS / Wayland 类 Linux 目标上没有 delegate**，
> 因此 `-platform hyremote` 在那里**不能启动应用**。这是"不可用"，不是"未验证"。

## 1. 构建入口：一个脚本，两个平台

仓库根目录的 `compile.cmd` **同时是 POSIX shell 脚本和 Windows 批处理脚本**：

```text
Windows:  compile.cmd --mode qpa --qt-prefix C:/Qt/6.8.3/mingw_64
Linux:    sh compile.cmd --mode qpa --qt-prefix /opt/Qt/6.8.3/gcc_64
```

脚本的默认接入方式是 **QPA**（`HYREMOTE_DEFAULT_MODE`），但**交叉编译到嵌入式目标时必须显式改掉它**：
透明 QPA 需要目标平台存在**已资格的原生 delegate**，而嵌入式目标没有（详见第 4 节）。

| 参数 | 含义 |
| --- | --- |
| `--mode qpa` | 透明 QPA 代理接入（`-platform hyremote`）。**只在有已资格 delegate 的目标上可用**：V1 已资格的 delegate 是 Windows `qwindows` 与 Linux x86_64 `qxcb`；**EGLFS / Wayland 类 Linux 目标没有任何 delegate**，`-platform hyremote` 在那里**无法启动应用**，不是"尚未验证" |
| `--mode cpp` | 仅嵌入式 C++（`HyRemote::RemoteAccess`） |
| `--mode qml` | 声明式 QML（`import HyRemote`） |
| `--mode all` | 三种接入方式全部构建 |
| `--mode minimal` | 最小构建（不开示例/测试/QML/QPA） |
| `--toolchain <file>.cmake` | **指定交叉编译工具链文件** |
| `--qt-prefix <path>` | 目标平台的 Qt 6.8.3 安装前缀 |
| `--build-type Release\|Debug` | 构建类型（默认 Release） |
| `--tests` | 同时构建测试 |
| `--no-examples` | 不构建示例 |
| `--clean` | 先删除构建目录再重新配置 |
| `-v` | 详细输出（默认把日志写入 `build/configure.log` 与 `build/build.log`） |

两种写法都可用：`--mode qpa` 与 `--mode=qpa`。

### 只允许一个构建目录

本项目**只保留一个构建目录 `build/`**（它是 `.gitignore` 忽略的目录）：

- 构建产物一律落在 `build/`，**不再按模式分子目录**，日志也写在 `build/` 内，仓库根目录不会出现构建文件；
- **换接入方式或需要重新构建时，先清理**：`clean.cmd` 会删除整个 `build/`；
- 若 `build/` 里已存在另一种模式的配置，脚本会**拒绝混用**并提示一条命令，而不是悄悄复用可能已失效的缓存；
  该提示目前打印的是 `compile.cmd --clean --mode <新模式>`，但模式闸门在清理**之前**判定，这条命令**实际会被拒绝**
  （已记录在 #141）。**可用序列是先清理、再按新模式配置**：

```text
Windows:  clean.cmd  &&  compile.cmd --mode cpp --qt-prefix C:/path/to/target-qt
Linux:    sh clean.cmd && sh compile.cmd --mode cpp --qt-prefix /opt/qt-6.8.3-aarch64
```

清理入口：`clean.cmd`（删除 `build/`）。

## 2. 指定交叉编译工具链

```text
sh compile.cmd --mode qpa \
  --toolchain cmake/toolchains/aarch64-linux-gnu.cmake \
  --qt-prefix /opt/qt-6.8.3-aarch64
```

仓库自带两个示例工具链文件（详见 `cmake/toolchains/README.md`）：

| 文件 | 目标 |
| --- | --- |
| `cmake/toolchains/aarch64-linux-gnu.cmake` | 64 位 ARM 嵌入式 Linux（如 RK3588 类板卡） |
| `cmake/toolchains/arm-linux-gnueabihf.cmake` | 32 位 ARM 硬浮点嵌入式 Linux |

自定义目标时，复制其中一份并修改三处：`CMAKE_SYSTEM_PROCESSOR`、交叉前缀与（如需要）`CMAKE_SYSROOT`。
**保留**四个 `CMAKE_FIND_ROOT_PATH_MODE_*` 设置（避免误用主机的头文件与库），并让 `PROGRAM` 保持 `NEVER`
（这样 `moc`、`rcc`、`ninja`、`cmake` 仍在主机上运行）。

## 3. 必须准备的三样东西

1. **交叉工具链**：提供目标的 `gcc`/`g++`/`ar`/`ranlib`/`strip`；不在 `PATH` 默认位置时用
   `-DHYREMOTE_TOOLCHAIN_PREFIX=/opt/gcc-arm-13/bin/aarch64-none-linux-gnu-` 指定前缀。
2. **目标平台的 Qt 6.8.3**：`--qt-prefix` 必须指向**为目标架构编译的 Qt**，不能用主机 Qt
   （主机 Qt 提供的是主机二进制与主机库，无法链接进目标代码）。用 Qt 的 `qt-cmake` / `configure -qt-host-path`
   流程生成。
3. **目标 sysroot**（若工具链未内置）：示例默认 `/usr/<triple>`，可用 `-DCMAKE_SYSROOT=` 覆盖。

## 4. 嵌入式目标上的接入方式与部署

**先决定接入方式，因为它决定这里能不能跑**：

| 目标 | 可用接入方式 | 说明 |
| --- | --- | --- |
| Windows x86_64 | Embedded C++ / QML / **Transparent QPA**（delegate `qwindows`） | V1 参考平台 |
| Linux x86_64 | Embedded C++ / QML / **Transparent QPA**（delegate `qxcb`） | V1 参考平台 |
| 嵌入式 Linux / EGLFS 类 | **Embedded C++ / QML** | **无 QPA delegate**：`-platform hyremote` 无法启动应用 |
| 嵌入式 Linux / Wayland 类 | **Embedded C++ / QML** | 同上，无 delegate |

透明 QPA 的工作方式是"装饰一个**已资格的原生平台插件**"，所以它只在这些插件存在时有意义。嵌入式目标没有
它们，因此：

- **不要把 `-platform hyremote` 写进嵌入式启动脚本**；交叉编译该类目标请用 `--mode cpp` 或 `--mode qml`；
- QPA 的远程输入是**启动策略**（zero-code 模式没有运行时控制对象），需要运行中 `start()` / `stop()` 与策略切换的
  产品请用 **Embedded C++**（`HyRemote::RemoteAccess`）或 **Declarative QML**。

**部署契约（有 delegate 的目标适用）**：应用保持 Qt-only，插件放在**应用自己的插件目录**
（`<app>/plugins/platforms/`），由 `hyremote_deploy(TARGET MyApp QPA)` 摆放，**不需要** `QT_PLUGIN_PATH`、
`QT_QPA_PLATFORM_PLUGIN_PATH` 或 `LD_LIBRARY_PATH`。该 helper 加入的负载清单是：

1. `qhyremote`（Qt 平台 MODULE，按选定的 SDK/源码构建选取）；
2. 共享运行时 `HyRemoteRemoteAccess`（由该模块内部使用）；
3. Qt / 原生平台依赖（由 Qt 部署工具解析，即被装饰的原生 delegate 本身）。

清单里**没有任何 HyRemote 之外的 delegate**：那是目标 Qt 提供的东西，所以目标 Qt 必须自带一个可用平台插件。
版本不匹配或缺 QPA 负载时 `hyremote_deploy(... QPA)` **fail-closed** 报错（私有 QPA ABI 限定 Qt 6.8.3）。

## 5. 常见问题

| 现象 | 原因与处理 |
| --- | --- |
| `Could not find a package configuration file provided by Qt6` | `--qt-prefix` 指向了主机 Qt，或目标 Qt 未安装；改为目标 Qt 前缀 |
| 链接时报主机架构的库 | toolchain 文件里的 `CMAKE_FIND_ROOT_PATH_MODE_*` 被改坏了；恢复 `PROGRAM NEVER` / 其余 `ONLY` |
| `qhyremote` 插件加载失败 | 目标 Qt 版本与限定版本不一致（QPA 负载使用 Qt 私有 QPA ABI，限定 **6.8.3**，见 `docs/reference/compatibility.md`） |
| 构建成功但目标板上不显示 | 若用的是 `--mode qpa`：**该目标没有 QPA delegate**，`-platform hyremote` 无法启动，请改用 `--mode cpp` / `--mode qml`。若用的就是 C++/QML：这属于运行时行为，嵌入式平台族当前**未验证**，需要目标板的显示与输入栈证据 |
