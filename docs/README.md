# HyRemote 文档

> 语言 / Language：**中文** ｜ [English](en/README.md)

HyRemote 是面向现有 Qt 应用的远程访问框架。产品提供**一个共享 Runtime**，以及四种并列的接入方式：C++ API、QML API、Generic Plugin 和 QPA。Qt Widgets 与 Qt Quick 是 Runtime 的目标类型，不是两套产品。

这里集中提供产品接入、部署、安全、兼容性和能力边界说明。

## 从这里开始

按任务流走，不要先读架构：

1. **安装 / 构建 HyRemote** → [`guide/install.md`](guide/install.md) ｜ [English](en/guide/install.md)
2. **把它接入你自己的项目（唯一默认入口）** → [`guide/integrate-your-project.md`](guide/integrate-your-project.md)
   ｜ [English](en/guide/integrate-your-project.md)
3. **运行并连接 viewer** → [`guide/viewer-connection.md`](guide/viewer-connection.md)

真实开源项目接入案例 → [`../examples/real-world/`](../examples/real-world/)

| 你的场景 | 该读哪条路线 |
| --- | --- |
| 新增少量 C++ 代码，显式控制远程访问生命周期 | [`getting-started/cpp.md`](getting-started/cpp.md) |
| 不修改应用业务代码，保持原生 Qt platform | [`getting-started/generic.md`](getting-started/generic.md) |
| Qt Quick 应用偏好声明式配置 | [`getting-started/qml.md`](getting-started/qml.md) |
| 需要 `-platform hyremote` 的专用零代码入口 | [`getting-started/qpa-proxy.md`](getting-started/qpa-proxy.md) |
| 安装、构建和 SDK 接入 | [`guide/install.md`](guide/install.md) ｜ [English](en/guide/install.md) | 产品指南 |
| 打包与部署 | [`guide/deployment.md`](guide/deployment.md) ｜ [English](en/guide/deployment.md) | 产品指南 |
| 连接查看器、远程控制、重连 | [`guide/viewer-connection.md`](guide/viewer-connection.md) | 产品指南 |
| 安全模型与部署边界 | [`security.md`](security.md) ｜ [English](en/security.md) | 产品指南 |
| 排错 | [`guide/troubleshooting.md`](guide/troubleshooting.md) | 产品指南 |

## 产品概览

先阅读 [`product-overview.md`](product-overview.md) 了解产品定位、四种接入方式、当前版本能力和长期方向。

当前版本（V0.2 LAN trial）的实际事实：

- 四种接入方式是**并列**的，没有主次；都走同一条
  [获得 → 接入 → 部署 → 运行 → 连接](guide/integrate-your-project.md) 流程；
- Windows x86_64 与 Linux x86_64、Qt 6.8.3 是参考环境；
- Widgets 与 Qt Quick 共用一个 Runtime；
- 默认监听 `0.0.0.0:5921`（本机 IPv4 接口，不只是 loopback），远程输入默认关闭；
- 默认**未认证、未加密**，仅适用于**可信局域网**，不适合暴露到 Internet。

## 产品参考

| 文档 | 说明 |
| --- | --- |
| [`product-overview.md`](product-overview.md) | 产品定位、集成方式、能力边界与路线 |
| [`architecture.md`](architecture.md) | Core、Shared Runtime、四个 integration frontend 的产品架构 |
| [`performance-optimization.md`](performance-optimization.md) | 长期性能专项：交互时延 SLO、Freshness-first、自适应采集/传输与回归 Gate |
| [`compatibility.md`](compatibility.md) | 当前平台、Qt、接入方式兼容矩阵 |
| [`security.md`](security.md) | 当前安全行为、默认值和不支持的安全能力 |
| [`known-limitations.md`](known-limitations.md) | 已知限制 |
| [`versioning.md`](versioning.md) | 产品版本语义与 V0.1 → V1.x 演进 |
| [`v1-api-stability.md`](v1-api-stability.md) | 公开 API 稳定性边界 |
| [`widgets-capture.md`](widgets-capture.md) | Widgets 采集模型 |
| [`quick-capture.md`](quick-capture.md) | Qt Quick 采集模型 |
| [`input-model.md`](input-model.md) | 远程输入模型 |
| [`dependency-policy.md`](dependency-policy.md) | 产品依赖策略 |
| [`release-package-manifest.md`](release-package-manifest.md) | SDK / 运行时载荷定义 |

## 四种接入方式的关系

```text
C++ API ---------\
QML API ----------\
Generic Plugin ----> Shared Runtime -> Core
QPA --------------/
```

四种入口之间没有父子关系：

- **C++ API**：应用链接 `HyRemote::RemoteAccess`；
- **QML API**：`import HyRemote`，薄封装同一个 Runtime；
- **Generic Plugin**：Qt public plugin 路径，保持 native QPA/platform identity；
- **QPA**：使用 Qt private ABI 的 Factory Trampoline，委托给原生 platform integration。

无论从哪个入口进入，都不会创建第二套 Session、capture、input 或 transport 架构。

## 能力边界

四种接入方式都是对等路线，各自有明确的适用条件（QPA 需要与 Qt 私有 ABI **精确**一致）。未实现的能力会被直接说明，而不是标记为"待办"占位。

安全、兼容性和平台支持以对应产品矩阵为准，不从相似环境自动推导。

## 维护者资料

架构决策、仓库治理、发布流程、研究记录和验收资料位于 `docs/adr/`、`docs/internal/`、`docs/releases/` 与 `docs/proposals/`。普通应用接入无需阅读这些内容。
