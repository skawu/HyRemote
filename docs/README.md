# HyRemote 文档

> 语言 / Language：**中文** ｜ [English](en/README.md)

HyRemote 是面向现有 Qt 应用的远程访问框架。产品提供**一个共享 Runtime**，以及四种并列的接入方式：C++ API、QML API、Generic Plugin 和 QPA。Qt Widgets 与 Qt Quick 是 Runtime 的目标类型，不是两套产品。

本文档区只回答产品使用问题：HyRemote 是什么、如何接入、如何部署、当前支持到什么程度、有什么安全边界和已知限制。

## 从这里开始

| 你的场景 | 建议入口 | 当前状态 |
| --- | --- | --- |
| 新增少量 C++ 代码，显式控制远程访问生命周期 | [`getting-started/cpp.md`](getting-started/cpp.md) | **V0.1 主路径** |
| 不修改应用业务代码，保持原生 Qt platform | [`getting-started/generic.md`](getting-started/generic.md) | **V0.1 主路径** |
| Qt Quick 应用偏好声明式配置 | [`getting-started/qml.md`](getting-started/qml.md) | **Preview** |
| 需要 `-platform hyremote` 的专用零代码入口 | [`getting-started/qpa-proxy.md`](getting-started/qpa-proxy.md) | **Preview** |
| 安装、构建和 SDK 接入 | [`guide/install.md`](guide/install.md) ｜ [English](en/guide/install.md) | 产品指南 |
| 打包与部署 | [`guide/deployment.md`](guide/deployment.md) ｜ [English](en/guide/deployment.md) | 产品指南 |
| 连接查看器、远程控制、重连 | [`guide/viewer-connection.md`](guide/viewer-connection.md) | 产品指南 |
| 排错 | [`guide/troubleshooting.md`](guide/troubleshooting.md) | 产品指南 |

## 产品概览

先阅读 [`product-overview.md`](product-overview.md) 了解产品定位、四种接入方式、当前版本能力和长期方向。

V0.1 Developer Preview 的重点是**先让用户能够用起来**：

- C++ API 与 Generic Plugin 是主接入路径；
- Windows x86_64 与 Linux x86_64、Qt 6.8.3 是当前参考环境；
- Widgets 与 Qt Quick 共用一个 Runtime；
- 默认回环监听，远程输入默认关闭；
- QML API 与 QPA 已存在，但仍按 Preview 标识；
- 未完成能力在产品文档中直接标记为 **TODO / 待办**，而不是用 issue、PR、验收批次或历史过程来描述。

## 产品参考

| 文档 | 说明 |
| --- | --- |
| [`product-overview.md`](product-overview.md) | 产品定位、集成方式、能力边界与路线 |
| [`architecture.md`](architecture.md) | Core、Shared Runtime、四个 integration frontend 的产品架构 |
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

## 文档口径

用户与产品参考文档遵循以下规则：

- 只描述**产品当前行为、支持状态和明确的未来 TODO**；
- 不使用 issue 编号、PR 编号、runner 状态、验收批次、历史迁移过程作为产品说明；
- 已完成能力按事实描述；
- 已存在但尚未达到正式产品化程度的能力标记 **Preview**；
- 尚未实现的能力标记 **TODO / 待办**，不写成已经具备；
- 安全、兼容性和平台支持只按明确声明的范围描述，不从相似环境推导支持。

## 维护者资料

仓库治理、发布流程、ADR、研究记录、执行路线和验收资料位于：

```text
docs/internal/
docs/adr/
docs/releases/
docs/proposals/
```

这些文件服务于开发与发布，不属于普通用户接入 HyRemote 的必读路径。产品文档不会把其中的执行过程复制给用户。