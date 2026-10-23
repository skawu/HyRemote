# HyRemote 产品概览

> 语言 / Language：**中文** ｜ [English](en/product-overview.md)

HyRemote 是面向现有 Qt 应用的远程访问框架。应用接入后可以获得远程画面和可选的远程控制，不需要改成远程桌面架构，不需要替换 UI 技术栈，也不需要把 HyRemote 的内部实现暴露给业务代码。

产品由一个共享 Runtime 和四个并列的接入前端组成。Qt Widgets 与 Qt Quick 是 Runtime 支持的目标类型，不是两套产品版本。

## 1. 产品定位

HyRemote 面向已经拥有 Qt 应用、希望把远程访问做成应用能力的团队。它不要求团队另外部署一套桌面远程控制产品。

产品重点：

- 以较低侵入方式接入现有 Qt 应用；
- 提供远程画面和可选的远程输入；
- 保持本地渲染和本地输入行为；
- 通过一套 Runtime 同时支持 Qt Widgets 和 Qt Quick；
- 支持多种接入方式，但不复制 Session、采集、输入和传输实现；
- 通过 CMake 和 Qt 约定提供 SDK 式使用方式；
- 在 GA 前建立可移植的正确性基线和可用的交互性能基线。硬件或厂商相关的加速只有在测量证明必要时才启用。

HyRemote 不替代通用操作系统远程桌面服务，也不替代专用视频流平台。

## 2. 四种接入前端

四种前端进入同一个 Runtime：

| 前端 | 应用接入方式 | 产品角色 | 当前状态 |
| --- | --- | --- | --- |
| **C++ API** | 链接 `HyRemote::RemoteAccess` | 显式控制生命周期、策略和目标 | 并列路线 |
| **QML API** | `import HyRemote` | 同一 Runtime 上的声明式前端 | 并列路线 |
| **Generic Plugin** | Qt 通用插件机制，不链接 HyRemote 应用库 | 保留原生 Qt 平台的零代码路径 | 并列路线 |
| **QPA** | `-platform hyremote` | 使用 Qt private ABI Factory Trampoline 的专用零代码路径 | 并列路线 |

四种前端是并列关系。QML、Generic 和 QPA 不以 C++ 前端为实现父级，它们直接接入共享 Runtime。

### C++ API

```cmake
find_package(HyRemote CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE HyRemote::RemoteAccess)
```

```cpp
#include <HyRemote/RemoteAccess.h>

HyRemote::RemoteAccess remote(&window);
remote.start();
```

应用需要自己控制远程访问策略、启动/停止、目标或配置时，使用 C++ API。

### Generic Plugin

应用保持普通 Qt 应用：

```cmake
target_link_libraries(MyApp PRIVATE Qt6::Widgets)

find_package(HyRemote CONFIG REQUIRED)
hyremote_deploy(TARGET MyApp GENERIC)
```

通过 Qt 的通用插件机制启动，例如：

```text
MyApp -plugin hyremote
```

原生平台仍然负责窗口和输入。Generic Plugin 应用继续运行在 `qwindows`、`qxcb` 或其他 Qt 提供的平台上，不切换到 HyRemote 的平台插件。

### QML API

```qml
import HyRemote

RemoteAccess {
    target: mainWindow
    enabled: true
}
```

QML 对象是同一 Runtime 上的一层薄封装，行为与 C++ API 一致。

### QPA

```text
MyApp -platform hyremote
```

QPA 与只替换平台的 `qvnc`/offscreen 方案不同。`qhyremote` 作为 Factory Trampoline，把正常平台行为交给经过资格认证的原生 Qt 平台实现。

QPA 依赖 Qt private ABI，因此只支持精确版本。当前参考版本是 Qt 6.8.3。

## 3. 一套产品 Runtime

```text
C++ API ---------\
QML API ----------\
Generic Plugin ----> Shared Runtime -> Core -> transport -> viewer
QPA --------------/        |            ^
                           |            |
                    Widgets / Quick   normalized input
                    target adapters
```

共享 Runtime 负责跨接入方式保持一致的产品行为：

- Qt 目标适配器；
- 自动 surface 发现与合成；
- 具体 RFB 传输接入；
- 安全配置处理；
- Runtime 服务和诊断；
- Core、采集、输入和传输之间的协作；
- 所有前端共用的 viewer 感知采集与新鲜度策略。

Core 负责与传输无关的语义，包括帧生命周期、调度与背压、规范化输入路由、状态/错误行为和能力抽象。

正常产品只暴露一个共享 Runtime 产物：`HyRemote::RemoteAccess` / `HyRemoteRemoteAccess`。Core 保持内部实现。

## 4. Widgets 与 Qt Quick

Widgets 和 Qt Quick 都是 Runtime 的一等目标：

- 受支持的 Widgets 目标使用 Widgets 适配器；
- 受支持的 Qt Quick 目标使用 Quick 适配器；
- Generic 和 QPA 通过共享 Runtime 的自动组合逻辑发现受支持的应用顶层 surface。

应用不直接选择采集类、协议对象、输入 sink、编码器、damage 模式或 GPU 性能后端。

QOpenGLWidget、QQuickWidget、Quick3D、自定义 FBO 或特殊原生窗口归属等图形场景需要单独做兼容性资格认证，不能从基础 Widgets/Quick 支持中推断。

## 5. SDK 与部署模型

HyRemote 通过 SDK 或源码使用。安装后的 SDK 使用标准 CMake 包发现：

```cmake
find_package(HyRemote CONFIG REQUIRED)
```

部署统一通过一个 helper family：

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp GENERIC)
hyremote_deploy(TARGET ExistingQtApp QPA)
```

只有所选接入语义允许时才组合 payload。Generic 和 QPA 是两种并列的零代码平台策略，不能合并成同一个模式。

应用不要手工复制 HyRemote 内部库/插件，也不要依赖源码树或构建树的运行时路径。

## 6. 当前产品线：V0.3.0.0 Product Preview

V0.3.0.0 是当前已发布版本，目标是让 Qt 开发者完成自助接入，并得到可运行的远程访问路径。版本包含：

- Windows x86_64 和 Linux x86_64；
- Qt 6.8.3 参考 SDK；
- Widgets 和 Qt Quick 目标适配器；
- RFB 3.8 正确性传输；
- C++、QML、Generic、QPA 四条并列前端；
- 配套 HyRemoteTool：通过显式 IP/端口连接远端 HyRemote 应用，提供画面、只读/控制输入、FPS/吞吐和连接诊断。

HyRemoteTool 的管理面能力不在 V0.3.0.0 范围内。版本语义和后续产品线见 [`versioning.md`](versioning.md)。

## 7. 安全模型

HyRemote 当前的未认证/未加密模式不构成面向互联网的安全承诺。

当前行为：

- 默认监听 `0.0.0.0:5921`；
- 远程输入默认关闭；
- `Insecure` 未认证、未加密，只应部署在可接受该暴露范围的可信 LAN；
- `Authenticated` 在构建和配置具备时可以提供 viewer 兼容的 VNC 认证，但不代表传输加密；
- `AuthenticatedEncrypted` 尚未实现，在加密后端不可用时会失败关闭。

完整安全边界见 [`security.md`](security.md)。

## 8. 产品路线

HyRemote 按用户价值演进，不按内部子系统完成度排版本。**当前唯一产品成果路线图**是 [`internal/product-version-delivery-roadmap-2026-10-23.md`](internal/product-version-delivery-roadmap-2026-10-23.md)（#594/#605）；[`versioning.md`](versioning.md) 只定义四段版本语义，`.github/release/release-trains.json` 才能激活精确版本及 mandatory children。技术名称不预留 Feature 版本号。

性能遵循同一原则：RC/GA 前必须具备可用的交互性能基线；更低层的硬件或厂商加速只有在测量证明必要时才启用。

## 9. 长期技术方向

架构把若干未来能力留在内部扩展点之后，使它们可以在不改变普通应用接入方式的情况下演进：

- 替代采集路径；
- 外部或 GPU 后备帧存储；
- DMA-BUF/GBM 与嵌入式图形集成；
- RKMPP/VAAPI/D3D 等硬件编码；
- 面向高动态或媒体负载的额外传输；
- 更丰富的会话和应用控制 API。

这些是产品改进方向，不要求普通应用理解 HyRemote 内部实现。

### 长期性能专项

性能是 HyRemote 的长期产品属性，不是一次性的优化里程碑。已接受的模型：

- **新鲜度优先**：优先发送最新有效状态，不排队重放历史帧；
- **默认自动**：用户不选择编码器、压缩级别、damage 模式、采集深度或流控窗口；
- **按需工作**：无 viewer 或静态场景避免不必要的采集、编码和网络工作；
- **交互优先**：远程输入应优先得到新的画面响应；
- **自适应交付**：普通局部 GUI 可以使用区域/damage 交付，持续高动态 Quick 可以使用整帧压缩；
- **有界交付**：采集、Core 交接、待编码工作和每个 viewer 的网络状态都有上界；
- **可移植基线优先**：只有可移植基线未达到测量目标时，才引入平台/GPU 加速。

普通直连 LAN 参考场景由 #370 定义交互延迟目标：P50 不超过 200 ms，P95 不超过 500 ms，前提是声明的资格条件成立。这是有边界的参考 SLO，不是对所有网络或媒体型负载的承诺。

详细模型见 [`performance-optimization.md`](performance-optimization.md)。

## 10. 产品原则

- **现有应用优先**：远程访问不要求重写 UI 或业务逻辑。
- **一套 Runtime**：接入便利不能产生重复的产品栈。
- **公共 API 优先**：稳定的应用接入使用公共契约，Qt private ABI 只出现在 QPA。
- **接入技术并列**：C++、QML、Generic、QPA 在技术上不同，但共享一套 Runtime，不按实现便利性排名。
- **本地行为优先**：远程访问增强应用，不重新定义应用的本地平台行为。
- **新鲜度优先于吞吐**：显示最新有效状态比发送每个过期中间帧更重要。
- **性能默认自动**：普通用户不应为了获得合格体验去学习传输、采集或网络调优。
- **可移植基线优先于加速**：硬件/私有优化必须有测量到的产品阻塞，不能只凭理论机会。
- **兼容性和性能基于证据**：与另一个 Qt 版本、操作系统、图形路径、SoC 或基准相似，不自动构成支持或性能承诺。
- **不锁定协议**：RFB 是当前传输基线，不是永久产品边界。

## 11. 性能证据与回归

性能资格记录精确候选 SHA、环境、负载和分布指标。功能 CI 全绿不代表性能可以退化；如果改动明显破坏已接受的交互延迟、画面新鲜度或资源上界，仍应阻止发布。

确定性的合成/协议/x86 证据优先放在 GitHub Workflow。RK3588/EGLFS/Wayland/GPU/本地 HMI 的结论需要在真实参考环境验证。

长期专项和机器回归证据计划见 #370 和 #374。

## 12. 当前待办

当前缺口由 GitHub 路线跟踪，不通过虚构技术版本槽位表达。性能相关的 GA 前工作包括：

- 实用压缩与增量交付（#261/#144）；
- RFB Continuous Updates/Fence 与每 viewer 交付控制（#371）；
- 按需、输入触发和自适应 Runtime 采集（#372）；
- 可重复的性能证据与回归门禁（#374）；
- 最终生产与物理参考资格（#9 及相关平台 issue）。

精确环境状态见 [`compatibility.md`](compatibility.md)、[`known-limitations.md`](known-limitations.md) 和 [`performance-optimization.md`](performance-optimization.md)。
