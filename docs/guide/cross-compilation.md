# 交叉编译到嵌入式目标

本页说明如何用项目自带的构建脚本与 **CMake toolchain 文件**，在 Windows 或 Linux 主机上为嵌入式目标
交叉编译 HyRemote。

> **先读这一条**：交叉编译成功只证明**能构建**，不证明能在目标板上运行。本项目对嵌入式 Linux/EGLFS 的
> 运行行为**尚未验证**（`docs/compatibility.md` 中 RK3588 / EGLFS + OpenGL ES 一栏标为 *Unverified*），
> 因此交叉编译产物**不是**兼容性或支持声明，也不能作为 V1 x86 参考平台的发布证据。

## 1. 构建入口：一个脚本，两个平台

仓库根目录的 `build.cmd` **同时是 POSIX shell 脚本和 Windows 批处理脚本**，并且是仓库**唯一的构建/安装入口**：

```text
Windows:  .\build.cmd build --integrations=qpa --qt-prefix C:/Qt/6.8.3/mingw_64
Linux:    sh ./build.cmd build --integrations=qpa --qt-prefix /opt/Qt/6.8.3/gcc_64
```

配置优先级是 **内置默认值 < `build.yml` < 命令行**：`build.yml` 保存仓库的开发者配置档，命令行参数覆盖它：

| 参数 | 含义 |
| --- | --- |
| `--mode=qpa` | 透明 QPA 代理接入（`-platform hyremote`） |
| `--mode=cpp` | 仅嵌入式 C++（`HyRemote::RemoteAccess`） |
| `--mode=qml` | 声明式 QML（`import HyRemote`） |
| `--mode=generic` | 零代码 `QGenericPlugin` 接入（`-plugin hyremote`） |
| `--mode=all` | 全部接入前端 |
| `--mode=runtime` | 仅共享运行时，不含任何前端 |
| `--mode=minimal` | 最小构建（不含示例/测试/QML/QPA） |
| `--integrations=cpp,qml,generic,qpa` | 精确选择前端；不会自动补上 C++ |
| `--toolchain=FILE.cmake` | **指定交叉编译工具链文件** |
| `--qt-prefix=PATH` | 目标平台的 Qt 6.8.3 安装前缀 |
| `--build-type=Release\|Debug` | 构建类型（默认 Release） |
| `--build-dir=DIR` | 构建目录（默认 `build`） |
| `--tests` / `--run-tests` | 构建测试 / 构建并运行测试 |
| `--no-examples` | 不构建示例 |
| `--security` | 传输安全能力（VNC Authentication） |
| `--clean` | 先删除构建目录再重新配置 |
| `--cmake=KEY=VALUE`、`--env=KEY=VALUE` | 额外 configure 缓存项 / 环境变量，可重复 |
| `-v` | 详细输出（默认把日志写入 `build/configure.log` 与 `build/build.log`） |

两种写法都可用：`--mode qpa` 与 `--mode=qpa`。

### 只允许一个构建目录

本项目**只保留一个构建目录 `build/`**（它是 `.gitignore` 忽略的目录）：

- 构建产物一律落在 `build/`，**不再按模式分子目录**，日志也写在 `build/` 内，仓库根目录不会出现构建文件；
- **换接入方式或需要重新构建时，先清理**：`build.cmd clean` 会删除整个 `build/`（连同其中的安装根）；
- 若 `build/` 里已存在另一种模式的配置，脚本会**拒绝混用**并提示确切命令（`build.cmd rebuild --integrations=<新模式>`），
  而不是悄悄复用可能已失效的缓存。

清理入口：`build.cmd clean`（删除 `build/`）。

## 2. 指定交叉编译工具链

```text
sh ./build.cmd build --integrations=qpa \
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

## 4. 部署与运行

交叉编译产物包含共享运行时、`qhyremote` 平台插件与原生 delegate 的依赖关系，需按与桌面相同的部署契约摆放
（见 `docs/guide/deployment.md` 与 `hyremote_deploy()`）。QPA 接入在目标板上的启动方式与桌面一致：

```text
MyApp -platform hyremote
```

## 5. 常见问题

| 现象 | 原因与处理 |
| --- | --- |
| `Could not find a package configuration file provided by Qt6` | `--qt-prefix` 指向了主机 Qt，或目标 Qt 未安装；改为目标 Qt 前缀 |
| 链接时报主机架构的库 | toolchain 文件里的 `CMAKE_FIND_ROOT_PATH_MODE_*` 被改坏了；恢复 `PROGRAM NEVER` / 其余 `ONLY` |
| `qhyremote` 插件加载失败 | 目标 Qt 版本与限定版本不一致（QPA 负载使用 Qt 私有 QPA ABI，限定 **6.8.3**，见 `docs/compatibility.md`） |
| 构建成功但目标板上不显示 | 这属于运行时行为；嵌入式平台族当前**未验证**，需要目标板的显示与输入栈证据 |
