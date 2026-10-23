# HyRemote 版本语义与产品里程碑

> 语言 / Language：**中文** ｜ [English](en/versioning.md)

HyRemote 使用四段产品版本：

```text
Major.Minor.Feature.Maintenance
```

版本号描述外部可理解的产品能力和兼容边界，不编码内部架构工作、接入前端名称、Qt 版本、平台或 CI/WBS 顺序。

## 版本字段

### Major：产品代际或非兼容边界

跨越实质非兼容产品边界时增加 Major，例如：

- 不兼容的公共 API/ABI 代际；
- 无法保持既有契约的产品架构变更；
- 要求不兼容应用模型的 Qt/框架迁移。

增加 SoC、操作系统、传输后端或加速后端，不会仅因实现变化就自动增加 Major。

### Minor：连贯的产品能力阶段

Minor 表示用户可以理解的新产品能力线。

GA 后的 Minor 只为已激活的连贯产品结果开放。任何技术主题都不预分配 Minor 或 Feature 版本。

### Feature：有界、可独立交付的能力

Feature 用于 Major/Minor 产品线内可独立交付和验证的有界能力。

版本数字不编码：

- C++、QML、Generic、QPA；
- Windows、Linux、嵌入式 Linux；
- Widgets、Qt Quick；
- Qt 版本；
- 源码目录或实现层。

这些属于兼容/能力维度，单独记录。

### Maintenance：修复和非能力变更

Maintenance 用于保持已发布能力边界不变的改动，例如：

- 缺陷修复；
- 不改变公共产品契约的安全修复；
- 已支持范围内的兼容性修正；
- 不新增产品能力的性能调优；
- 打包、文档、CI 修正。

Maintenance 不能用来隐藏新平台或重大产品能力。

## 预 GA 产品线

| 版本 | 名称 | 用户承诺 |
| --- | --- | --- |
| **V0.1.0.0** | Developer Preview | 开发者可以接入 HyRemote 并建立早期、有限的远程访问路径 |
| **V0.2.0.0** | First User Trial | 真实用户可以在第二台 LAN 机器上使用应用级远程查看/输入，默认关闭访问，并明确未加密边界 |
| **V0.3.0.0** | Productize It / Product Preview | 四种接入前端完成打包、部署、文档和教学，成为产品路径 |
| **V0.4.0.0** | Qualify It / Release Candidate | 功能冻结、Qt/应用兼容、性能、物理资格和 RC 硬化 |
| **V1.0.0.0** | Stabilize It / First GA | 对成熟 V0.4 产品线提供正式稳定支持契约 |

V0.x 是产品成熟度版本，不是按前端拆分的版本。已退休的 `V0.0.1.0 = C++`、`V0.0.2.0 = QML`、`V0.0.3.0 = QPA` 规划不再使用。

## V0.1：Use It

V0.1 范围窄但可用：

- C++、QML、Generic、QPA 是并列接入技术；
- Windows x86_64 和 Linux x86_64 是桌面参考平台；
- Qt 6.8.3 是当前参考 SDK；
- Widgets 和 Qt Quick 共享一套 Runtime；
- 提供基础 SDK、部署和上手路径；
- 安全为未认证、未加密，不适合生产公网。

## V0.2：First User Trial

V0.2.0.0 交付了真实第二台机器上的应用级远程查看/输入、listener 生命周期和重连。

已接受的安全/访问事实：

- 默认 listener 为 `0.0.0.0:5921`，在主机 IPv4 接口可达；
- 默认 `Insecure` / RFB `SecurityType None`，未认证、未加密；
- 远程输入默认关闭，只有操作者显式启用才开启；
- 可选构建能力启用并配置后，可以使用 RFB VNC 口令认证，但传输仍未加密；
- 不可用的更强 `AuthenticatedEncrypted` 配置失败关闭，不静默降级；
- 只面向可信 LAN，不适合 Internet。

原生 TLS/VeNCrypt、证书生命周期、认证 Session Registry、角色、准入和每会话终止不属于 V0.2 能力。V0.2.0.1 是同一 First User Trial 线的维护修复，不是安全或会话 Feature。

## V0.3：Productize It

V0.3 把四种接入前端变成完整可交付路径：

- C++ API；
- QML API；
- Generic Plugin；
- QPA。

这条线还完成部署矩阵、任务导向自助文档/示例（#240）、四前端 SDK/部署产品化（#264）和最小自助诊断（#335）。

V0.3.0.0 还发布了 **HyRemoteTool 访问面配套客户端**：通过显式 IP/端口连接远端 HyRemote 应用，提供画面、只读/控制输入、FPS/吞吐遥测和连接诊断。管理面能力不属于 V0.3.0.0。见 [`internal/hyremote-tool-plan.md`](internal/hyremote-tool-plan.md)。

V0.3 家族分两类工作：

### 强制 GA 前 V0.3 基线

**Windows x86_64 为唯一必须有实际运行/物理验证证据的参考平台**：Qt 6.8.3 已验证基线；Qt 5.15、Qt 6.5 的 Windows 适配若纳入支持声明，需独立完成证据。实用压缩/增量、维护中 Viewer 互操作、#370 Windows 性能及正式 SDK/包质量是候选门槛。Linux x86_64、Wayland、ARM64/AArch64、EGLFS 以 Qt 跨平台路径供用户验证，不是开发、V0.4 或 V1.0 发布阻塞条件（Owner #563）。

**只有已承诺 Windows 支持单元的功能和真实证据必须在 V0.4 RC 进入前收敛。** Linux/嵌入式未验证不等于失败也不等于 Supported；用户问题通过 Issue 复现、评估及相应维护/hotfix 处理。任何一项都不预分配固定 Feature 版本（#333）。

### 可选、质量或差异化结果

Session Registry/Session Operations、完整品牌扩展、HyRemoteTool 管理面增强、原生 TLS，以及声明 GA 参考单元之外的 BSP/SoC/平台扩展，分别作为连贯产品结果激活。V0.3.0.0 的访问面客户端基线属于该版本，不是可选增强。

### 已激活的 V0.3 家族 Feature

**V0.3.3.0：Delivery Efficiency & Viewer Interoperability** 是 V0.3.0.0 之后第一个激活的 Product breadth Feature：

- 实用 damage、压缩和增量交付；profiling 已选择 Raw + TRLE，ZRLE 不是当前要求；
- #370 性能基线；
- 维护中标准 Viewer 互操作；
- Windows x86_64、Qt 6.8.3 是验收参考；Linux x86_64 可按 Qt 跨平台机制尝试，待用户验证，四前端各自保持真实的适用状态。

`V0.3.1.0`（Qt5 槽位）和 `V0.3.2.0`（Bandwidth/ZRLE 槽位）是已退休历史记录，不能重新选择。V0.3.3.0 是新的连贯结果。详见 [`internal/v0.3.3.0-plan.md`](internal/v0.3.3.0-plan.md)。

**2026-10-22 Owner 决策 #563（覆盖 #558 的平台验证硬门槛）：** V0.3.3.0 的 #144/#175 改以 Qt 6.8.3 **Windows x86_64** 真实 Viewer/性能为准；Linux、嵌入式 ARM64/EGLFS、Wayland 依赖 Qt 跨平台能力并接受用户验证，不再阻塞 V0.3.3、V0.4 或 V1。#236/#154/#203 保留为用户反馈和后续可验证适配工作，不宣称已经通过或通用支持；实际用户提交 Issue 后按可复现问题走 hotfix/维护版本，不擅自修改历史 tag。V0.4 仍仅资格认证，但只对已声明 Windows 支持单元实施硬资格。

后续平台/Qt LTS 必需成果与可选管理面、TLS 等成果均**不预占 V0.3.4.0/V0.3.5.0 或其他准确版本号**，须按 #24/#95 在独立用户成果与验收要求确定后才激活；Qt 6.12 LTS 仅为预检，未改变 V1.0 的 #343/#33 硬要求。

## V0.4：Qualify It

V0.4 是 Release Candidate 线，只做资格认证。它在以下条件满足后才可选择：

- V0.3.0.0 自助采用基础完成；
- 已声明 Windows 范围的 V0.3 家族 GA 前基线（#343/#370）完成；Linux/嵌入式未验证不阻塞；
- GA 支持矩阵冻结。

激活权威是 #235，lineage 是已完成、已激活的 V0.3 家族结果，不能是预分配技术槽位。大型架构变更和重大新能力在 V0.4 停止。

重点：

- Qt LTS 兼容资格，包括 Qt 5.15 LTS；
- 真实应用资格；
- 性能测量和阻塞驱动优化；
- **Windows** 物理本地+远程共存；Linux/嵌入式由用户验证并报告 Issue，不作为 RC 进入或发布硬条件；
- 发布包和支持声明硬化。

已知 GA 阻塞项清除前可以使用维护迭代：

```text
V0.4.0.1
V0.4.0.2
...
```

## V1.0：First GA

V1.0 是第一个正式稳定支持承诺。

它不应在最后一刻引入新 Runtime 架构、前端、协议族或大型公共 API。成熟的 V0.4 产品被稳定为覆盖 API、SDK、兼容声明、打包、文档和已知限制的 GA 契约。

## GA 后产品结果

GA 后工作作为冻结 GA 基线之外的连贯用户/产品结果，由各自权威 issue 激活。版本号不预留给技术主题（#333）。

已退休的固定槽位 `V1.1 = Embedded`、`V1.2 = Acceleration`、`V1.3 = Differentiate` 不再使用，也不发明替代槽位。

声明 GA 参考单元之外的平台/BSP 扩展、仍有用的证据驱动加速、高级可编程能力分别准入。它们必须保持一套逻辑 Runtime，不能产生独立的公共 `RuntimeQuick`/`RuntimeWidgets` 产品。

## 发布权威

当前可选择的发布列车、强制子项、候选前提、证据和 lineage 由 `.github/release/release-trains.json` 定义。版本号不表示自动授权；未记录版本、退休槽位和未激活的 V0.4 都会失败关闭。

内部工程工作归属于它支持的产品线，不单独获得版本号。
