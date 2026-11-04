# ADR-0008：阶段02已批准产品架构决策与适用边界

> 语言 / Language：**中文** ｜ [English](../en/adr/0008-stage02-human-approved-architecture-decisions.md)

状态：**Human 局部决策已批准（2026-11-04）；完整产品架构未冻结；设计记录，非实现/发布证明。**  
归档任务：[HyRemote #814](https://github.com/skawu/HyRemote/issues/814)  
产品基线：《HyRemote 产品设计文档》，`HYR-PD-001 / V14`（Human 已批准，2026-11-03；§2.2、§3.2–3.4、§4、§5、§6、§10、§14）。

## 决策

| 编号 | Human 选择 | 边界 |
| --- | --- | --- |
| D-02 | A | 允许多个客户端在传输层并发接入和输入协调；**不代表**在同一应用级控制范围内默认授予多人控制。V14 §6.1 要求同一 Scope 默认一名远程操作者，其他可查看，独立不冲突 Scope 经宿主批准可分别授权。 |
| D-08 | A | 保留原 RFB 端点的配置默认值 `0.0.0.0:5921`。它是配置值，不是无需授权即可开放全网卡的许可。Native 地址/端口和安全配置独立。 |
| AR-04 | A | 共享 Runtime 的 Qt 窗口系统远程输入/高 DPI 专用内部单元可受控使用 Qt Private（当前 `qt_window_system_input.cpp` 与 `Qt6::GuiPrivate`）。Core、公开 SDK 类型和其他 Runtime 实现保持隔离；QPA 自有的私有平台入口兼容边界不变，按精确 Qt Kit/平台认证。详见 [ADR-0007](0007-four-integration-runtime-architecture.md)。 |
| AR-05 | A | 标准 RFB 与**完整独立** Native 协议长期并存。Native 自行承担设备信任、会话、获准目标枚举/订阅、画面、输入、控制申请/撤销及基础协商，不依赖 RFB 画面。原 HyRemoteView G2 所设独立 Enhanced TLS 控制/诊断侧通道不再是基础必需路径；可选增强必须另经宿主授权和能力协商。 |
| H-02 | a | **新架构部署**在任何协议监听打开之前执行可信 DeploymentAdmission。核验应用实例与来源、授权目标、实际网络端点、协议安全、初始只读权限和可信策略来源；缺项/无效则拒绝绑定，不自动降级或换端口。保留旧 RFB getter 默认值，但接受无部署准入时旧 `start()` 不再成功监听的行为变化。**没有历史兼容绕过开关**。 |

## 强制跨层契约

1. **设备端策略权威**：分别检查设备身份、Session、Target、ViewGrant、ControlGrant、Scope 与有效期；连接成功或启用全局输入不构成授权。输入接纳前、Qt 目标实际交付前均须执行相应撤权检查。撤权清除旧队列保持键/触点及尚未发送的受限画面。
2. **隐私与输出**：共享基础采集是允许的，但按会话的遮罩、目标授权、水印/输出政策及待发缓存必须正确隔离；未授权像素不能因双协议共享帧而泄漏。
3. **协议与安全**：RFB 标准互通保持独立；当前 RFB VNC Authentication **不加密**。Native TLS 用户名密码、可信设备身份、证书变化暂停、禁止认证失败自动回退到明文 RFB 是新架构的**待实现设计要求**，不得写入已发布功能表。
4. **生命周期及资源**：三个构建模式（RFB-only、Native-only、Dual）及 C++/QML/Generic/QPA 四入口使用一个 Core/Runtime；协议可局部故障隔离，设备端统一权限生效；慢客户端、输入队列和共享帧存储有界。技术预算是设计目标，不是实测能力承诺。
5. **产品责任**：Framework 实施设备端远程权限、目标与协议；HyRemoteView 负责操作员会话、客户端信任和自身更新；HyRemoteTool 服务集成诊断；Gateway/企业服务是可选路径，不能变成离线 Direct 基础远控的必需条件。

## 替代关系与事实边界

- 本文件**修订** [ADR-0007](0007-four-integration-runtime-architecture.md) 的“Runtime 完全排除 Qt Private”绝对表述，只允许明确列出的受控例外；不重写 ADR 的历史背景。
- [HyRemoteView G2 #4](https://github.com/skawu/HyRemoteView/issues/4) 的独立 Enhanced Side Channel **基础通信选型**被 AR-05=A 替代；保留此前 G2 证据及 G3 PoC 历史。
- [HyRemote #246](https://github.com/skawu/HyRemote/issues/246) 的 HRC-5 独立增强端点从**基础架构依赖**中撤出；诊断及商业增强能力仍按产品分期治理。
- H-02a **只适用于实现了新准入机制的未来架构产品版本**；旧版已发布二进制的真实监听、安全行为和兼容性声明不被追溯改写。新架构实施时需明确迁移和失败提示。
- 阶段02唯一技术协议候选为 R15（21 类 Native 消息、50 正向/48 异常向量、15 状态场景），结合 R16–R18 安全、授权、资源签收；测试向量仅为设计结构自检，**没有**证明 Native、TLS 或客户端互操作已实现。

## 状态与下一道门槛

本 ADR 是 **Human 已批准的局部架构取舍**的正式追溯记录，不是整个架构冻结令、研发开工令、软件发布或设备资格认证。G6 还要求本文件及相关文档真正按仓库受保护 PR 工作流合入 `develop`，并完成两仓 Issue 的历史 supersession 追踪；G7 **必须**由 Human 另行明确批准。
