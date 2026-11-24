---
name: 本机 #949/Qt 验证夹具与环境要点
description: 本机 Windows 端跑 HyRemote Quick 采集 A/B 对照时的环境与夹具前提（MSVC/ninja/Qt Kit、CPU 探针开关、C++ API 消费方式、IPv4 绑定）
type: feedback
---

在本机做 HyRemote Quick 采集/延迟对照（如 #949）时，以下前提不是仓库里能看出来的，先按此设置再动手：

- 本机 shell 已注入 MSVC 14.44 的 INCLUDE/LIB/PATH，`cl.exe` 可直接用，不需要 vcvars64.bat（`C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\` 下并无 vcvars 可调）。
- 独立 CMake 夹具工程需要手动把 `C:\Qt\Tools\Ninja`（和 `C:\Qt\Tools\CMake_64\bin`）加进 PATH，否则 `-G Ninja` 报找不到构建程序。
- Kit：`C:\Qt\6.8.3\msvc2022_64`、`C:\Qt\5.15.2\msvc2019_64`（Qt5 侧含 Quick）。
- 要拿到 GPU→CPU 分阶段 CPU/wall 计数（grab / gl_gpu_map_copy / d3d11_staging_reuse / fallback / stale_drop）必须构建时加 `--cmake=HYREMOTE_ENABLE_CPU_PROBE=ON`；否则只有像素与延迟，没有阶段数据，A/B 对照会不对称。
- 产品 Generic 插件硬依赖 Qt6::Gui，Qt 5.15 只能用 `HyRemote::RemoteAccess` C++ API 路线（自建监听），不能套 `-plugin`。
- 独立工程 `find_package(HyRemote CONFIG)` 必须指向安装树 `build/install/lib/cmake/HyRemote`：build 树里的 HyRemoteConfig.cmake 会 include 只在 install 阶段生成的 HyRemoteTargets.cmake / HyRemoteDeploy.cmake；且后续 `build.cmd build --clean` 会把 build/install 一并删掉，需要重新 `build.cmd install`。
- 监听地址只接受 IPv4，`QHostAddress::Any` 会被拒（`listen address must be an IPv4 address`），要 `QHostAddress::AnyIPv4`。

**Why:** 这些点在 2026-11-11 的 #949 本机验证里各浪费过一次构建/运行周期，且失败信息（vcvars not recognized、Ninja 缺失、缺 HyRemoteTargets.cmake、IPv4 拒绝）都不指向真正原因。
**How to apply:** 任何在本机新建 Qt/HyRemote 采集夹具或复跑 A/B 对照前，先按上面五条把环境与构建开关对齐，再开始测量。
