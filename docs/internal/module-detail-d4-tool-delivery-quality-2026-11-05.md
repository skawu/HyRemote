# D4｜HyRemoteTool、安装交付与全产品安全/质量合同（候选）

> **2026-11-05 决策状态更新**：[Human #843 决策记录](https://github.com/skawu/HyRemote/issues/843#issuecomment-6646988698) 已批准 H-T01/H-DUAL/H-REVOKE/H-EDGE/H-RFB 均选择 A。V15/V8 等效原文已取得，Native P-A 数值/TLS-first A 已获批准；其余内部接口、性能假设、原子验收与 Gate-D 仍待设计审签，不因上述决定自动冻结。

> **任务**：[全产品设计总控 #843](https://github.com/skawu/HyRemote/issues/843)，范围 T-01、X-01、X-02；对齐 D1 Framework、D2 HyRemoteView、D3 Edge/Gateway 设计候选。  
> **正式基线**：Human 批准 `HYR-PD-001/V15`、`HYR-ARCH-001/V8`，目前已获得经 Human 确认与正式版正文相同、历史送审稿 SHA 匹配的 V15/V8 完整原文，并完成 56/99/Native 21 类结构核验；正式版 DOCX 二进制尚未逐字节核对，原子合同审签未完成。现有 `hyremote-tool-workbench-prd-2026-10-22.md` 的 **D 综合工作台与 S1 先交付**曾经另获 Product Owner 认可；不能静默覆盖已批准的 S1 工作台规划或删除已经真实发布的基础 RFB 访问能力；S1 全量验收 #586 仍 OPEN。  
> **性质**：`DESIGN_CANDIDATE / T01_X01_X02_REVIEW / NO_PRODUCT_CODE / NO_VERSION_ACTIVATION / NOT_FROZEN`。现有产品交付状态须按真实 release/代码/精确平台，本文不重写历史发布。

## 1. 四个软件产品/责任必须明确

| 部件 | 唯一主要产品职责 | 安装所在 / 不应产生的依赖 |
| --- | --- | --- |
| HyRemote Framework (HMI Runtime/SDK) | Qt 宿主应用的 Core、Shared Runtime、四接入、RFB + 完整独立 Native、现场 Target/Scope/ViewGrant/ControlGrant/隐私 | 安装在 HMI/Qt 宿主环境；不能要求额外安装 View/Tool/Gateway 才能 Direct |
| HyRemoteView | 官方访问客户端，Direct 与 Native 设备信任/画面/输入/权限；**同一产品安装中内置**可选 Factory Edge；Managed 远端亦使用 View | 工厂 PC **只装 View** 即可共享指定 HMI；外地 PC 只需 View；不要求独立 Edge Agent、Support Bridge |
| HyRemoteTool | 既有 Windows 本地综合工作台（含已获批准的远程连接、基本诊断与计划 SDK 集成）；未来对外职责须与 View 明确分工 | 独立按真实交付级别使用；不是 HMI 实施所需的唯一管理协议，也不代替 Gateway |
| HyRemote Gateway（可选） | 租户/站点/Edge 可信登记、路由一次性 Pair、透明 FIFO 和受限审计、自托管/厂商部署 | 可选公网/客户部署；不会变成 Direct 依赖，当前 #190 POST_GA_CONDITIONAL/NOT_ACTIVATED |

“产品”与“内部进程/库”不同：HyRemoteView 允许研究本产品内部 Windows SCM 服务角色，但用户不再单独部署另一个 Edge 软件；Framework 的 `NativeListener` 不是 Gateway 管理端口；HyRemoteTool 不因此成为另一套 HMI Runtime。

## 2. T-01 HyRemoteTool 设计与 View 的产品范围冲突（必须 Human 看见）

### 2.1 已有权威事实

- 2026-10-22 `hyremote-tool-workbench-prd-2026-10-22.md` 记录 Owner 确认 **D 综合工作台**、**S1=远程连接+会话工作区+基础诊断**；S2=进阶诊断，S3=SDK 集成辅助，S4=远程主机管理必须另经安全审核。
- 最新 V15/V8 的产品总体分工倾向是：**Framework=设备端、HyRemoteView=官方完整访问客户端/内置 Edge、HyRemoteTool=开发者 SDK/Qt Kit 集成诊断和支持工具**。
- 两套历史描述曾对“未来谁负责官方远程会话 UI、是否继续维护 Tool 的独立 RFB 客户端”存在实质重叠；**现已由 H-T01=A 正式裁决**，本节保留冲突来源及其消解依据，不再把它列作待决产品方向。

### 2.2 Human 正式批准的产品分工 A（H-T01=A）

**保留 Tool 当前实际已发布的基础 RFB 访问能力及维护责任；S1 全量功能仍待 #586 真实验收，绝不宣称已交付**；将未来的高阶完整 View/Native/Managed/身份/共享界面作为 **HyRemoteView** 的单一产品演进主线；Tool 新增开发主线集中在本机 SDK 安装检查、四接入指导、真实/脱敏诊断及工单证据，不在 Tool 重新造一套 Native/Edge/企业工作区。对 RFB 客户端核心的复用，只在许可证、单一所有权、二进制 ABI 与依赖独立性通过设计评审后进行，不能借临时共享源码让两产品版本互相锁定。

### 2.3 历史备选方案（已裁决，非重新征求选择）

| 选项 | 功能归属与迁移 | 风险/成本 | 建议 |
| --- | --- | --- | --- |
| **A：存量保留，新增主线分工** | 保留 Tool 已发布基础 RFB；S1 未完事项按原批准路线与新产品分工协调；新高级访问/Native/Edge 进入 View；Tool 聚焦 SDK/诊断 | 一段时间双 RFB UI/安全补丁路径并存；须治理共用 parser 或替代计划 | **推荐**：不追溯剥夺现有功能，避免未来无限双轨 |
| B：Tool 逐步转纯 SDK/诊断 | View 接管所有 Remote UI，Tool 提供迁移/入口与兼容窗口，约定停止支持日期 | 变更已批准 D/S1 和用户安装/工作流，版本与迁移成本高 | 需要 Human 明确退役和已交付权益过渡 |
| C：Tool 和 View 永久各自全面 Remote | Tool 与 View 都持续实现 RFB/Native/权限/更新 | 双安全面、双测试矩阵、产品定位重叠且长期成本最高 | 不建议，除非明确不同客户群与商业理由 |

**执行 H-T01=A 的边界**：不得因为 A 批准就宣称 Tool 已支持 Native TLS 或 S1 完整交付（#586 仍 OPEN），也不得删除已发布 RFB 访问能力；后续新的完整 Native/Managed/Edge 官方远控主线归 HyRemoteView，Tool 聚焦 SDK/Kit/诊断。RFB 底层库复用仍需技术/许可证审查。

## 3. T-01 Tool 内部职责及具体可验收接口

| 内部模块/输入输出 | 主责 | 禁止边界 |
| --- | --- | --- |
| `ToolWorkspace / Navigation` | 已真实交付的连接/远程会话/基础诊断入口，与阶段许可匹配的 SDK 集成页 | 不堆砌永远不可用的 Enterprise/Native/主机管理按钮 |
| `ConnectionController` + legacy `RfbClient` | 管理 Tool 当前支持的 RFB3.8 连接/取消/重连与本地 framebuffer | 不能将 Tool 的 None client 冒充 TLS/Native；不得抢 View 的未来产品控制入口 |
| `SdkInspector` | 读取用户**明确选择**的 SDK 根路径、CMake targets、manifest、Qt Kit/ABI、部署助手；返回来源/时间/成功与未验证单元 | 不自动扫描修改工程/下载未知 Qt Kit，不按存在文件判最终运行/身份合格 |
| `IntegrationAdvisor` | 显示对等 C++、QML、Generic、QPA 的最小可复制操作，Qt Private 风险/Kit 与部署限制 | 不在后台替用户修改 CMake/系统配置/安全 listener |
| `DiagnosticsImporter` | 只读导入用户提供的 Host/SDK 日志和 manifest，区分本地实测/远端可信/用户陈述/不可用 | 不伪造远端 API、OS/CPU/运行时数据；敏感字段脱敏并警示来源 |
| `SupportReport` | 带固定 schema、timestamp、Kit/版本/输入来源、缺失项、脱敏预览的可复制证据 | 禁止把密码、token、TLS 私钥、屏幕图像、原始 input 事件写报告 |
| `ProfileStore` | 本地非秘密连接资料、SDK 路线偏好、语言 Auto/手动覆盖，原子写盘/迁移 | 与 View/Edge 机器级 TrustStore/企业账户凭据不共享普通明文 profile |
| `ManagementAdapter`（未来、不得先产品暴露） | 仅在设备端正式安全管理能力获批并实测后连接 | 不借 RFB 键鼠模拟“远程主机配置”，不私开第三设备 TLS listener |

Tool 依照当前真实 Windows Qt5.15/Qt6.8 和已交付安装单元单独验证；与 Qt6.8 View 技术栈相似并不意味着两种不同产品的许可证、打包、版本、升级目录能够混用。

### 3.1 T-01 当前已交付事实、证据等级与未来实现隔离

当前已核对 [Tool README](../../tools/hyremote-tool/README.md) 的 **V0.3.0.0 既有客户端范围**：通过用户输入的 IP/port 连接 RFB 3.8 `SecurityType None`；默认只读，操作者显式开启后才发送键盘/指针/滚轮；支持画面、FPS/吞吐/帧数/分辨率、连接错误和中文/English 切换。该 README 还明确排除当前 VNCAuth、TLS/VeNCrypt、剪贴板、文件传输、全屏、多显示器及管理面功能。**这些是仓库文档声明而非本轮独立实机复验**；未来 Inspector、IntegrationAdvisor、SupportReport 并未由这份当前 Tool README 证明已交付。Native DeviceTLS/Managed/Edge 的正式访问工作仍归 View；Tool 完整 S1 #586 OPEN。

本模块不将旧 Tool 的 `Connected`、RFB 无认证像素或操作者勾选键鼠解释成 HMI 个人身份、Native `ViewGrant/ControlGrant` 或公网安全证明。对已交付功能做回归时也须使用可复核的精确 Tool/Qt Kit/OS/设备版本，不以 README 或设计 PR 代替运行结果。

### 3.2 Tool 内部只读诊断和用户可导出报告的输入输出合同

| 数据对象/逻辑模块 | 唯一输入与 Owner | 输出与最小溯源字段 | 拒绝或不允许做的事 |
| --- | --- | --- | --- |
| `ToolInputSession` | T-01：用户明确选择的 SDK 根、Kit、日志/manifest；显式取消代次 | `sourceKind,selectedPath,observedAt,cancelGeneration,readStatus` | 不后台遍历无关目录、不自动下载或修改 Qt/CMake/系统配置 |
| `SdkInspector` | T-01：指定 SDK manifest/CMake targets/实际 Qt Kit 与平台工件 | `source,artifactSHAOrUnknown,kit,checkKind,method,result`；静态/实际运行分离 | 文件存在不等于 QPA/QtPrivate exact Kit 已运行；缺证据不能 PASS |
| `IntegrationAdvisor` | T-01 消费唯一 [F-03 INT-01..10](module-detail-d1-framework-boundaries-2026-11-05.md) 与真实已安装公开 API | C++/QML/Generic/QPA 的兼容路线、权限预检及复现条件 | 不发明 setter、新 Native wire、后台创建 listener 或偷偷修改客户工程 |
| `DiagnosticsImporter` | T-01：用户显式导入且限制大小/路径的脱敏证据 | `source,observedAt,trustLevel,sourceSHAOrUnknown,parseStatus` | 不因用户陈述或日志文本就推断远端设备真实成功，不自动上传 |
| `SupportReport` | T-01：上述只读检查的已预览、人工确认集合 | `reportVersion,toolSHA,kit,sourceKind,checks[],unknowns[],redactions[]` | 禁止导出口令/token/TLS 私钥/截屏/输入全文；无人工确认不外发 |
| `ToolProfileStore` | T-01：Tool 自己的语言、非秘密连接偏好和 SDK 路径 | 独立产品 `schemaVersion`、迁移来源和失败状态 | 不与 View DeviceTrust、Factory SitePolicy 或 Gateway 租户秘密共享明文持久层 |

`checks[]` 只允许 `PASS_OBSERVED / FAIL_OBSERVED / NOT_RUN / NOT_APPLICABLE / UNKNOWN_SOURCE` 五种**设计内部报告状态**；每项必须携带 `checkId,method,source,kitOrNA,artifactDigestOrUnknown,observedAt`。未知或导入证据不能冒充经独立签收的安全资格。名称及字段均是 **T-01 内部设计合同，不是已发布 Tool API、外部 wire 或现成界面功能**。

### 3.3 T-01 可实施验收的最小不可分割合同

| ContractID | 正常行为的责任归属 | 负向/边界测试（未来，均未运行） | 既有验收入口 | 当前 |
| --- | --- | --- | --- | --- |
| `TL-01` | T-01 保留已发布 Basic RFB3.8/None、默认只读与显式控制开关 | 非 TLS 不能显示“设备身份已核验”；未实现 Native/VNCAuth 按真实能力报告 | XP-02/15/20 | `DESIGN_ONLY` |
| `TL-02` | T-01 管理当前 RFB 键鼠事件、失焦/断线及帧遥测的真实 UI | 禁控/丢连接/重复 UP 不谎报业务已执行，旧 held 清理须以未来真实回归验证 | XP-05/15 | `DESIGN_ONLY` |
| `TL-03` | T-01 Inspector 明确 Kit/SDK/manifest 与来源等级 | Qt minor/ABI、QtPrivate/插件不符及缺运行证据均不可 PASS | XP-01/14 | `DESIGN_ONLY` |
| `TL-04` | T-01 Advisor 使用唯一 F-03 INT-01..10 和已发布 API | QML/Generic/QPA 参数错、C++ setter 不存在及 Host H-02a 不合格不能给成功步骤 | FW-01/10、XP-14 | `DESIGN_ONLY` |
| `TL-05` | T-01 日志/manifest 只读、限额、脱敏导入 | 超大输入、目录逃逸、伪造 DeviceTLS/设备成功来源不能提升资格 | XP-14/16 | `DESIGN_ONLY` |
| `TL-06` | T-01 SupportReport 的逐项证据和显式导出确认 | token/私钥/屏幕内容/输入全文过滤；`NOT_RUN` 不可转成 PASS | XP-14/16/20 | `DESIGN_ONLY` |
| `TL-07` | T-01 仅迁移自身非秘密 Profile 和语言/Kit 偏好 | 升级/回退不能覆盖或恢复 View/Edge/Gateway 另一 Owner 的权限 | DEP-07、XP-24 | `DESIGN_ONLY` |
| `TL-08` | T-01 与 V-01 按 H-T01=A 分工，维护旧 RFB；未来完整 Native/Managed 归 View | Tool 全量 S1 #586 OPEN，不能当已验收；不得删除存量入口或构造第二个 Gateway/Edge 产品 | XP-15/19/20 | `DESIGN_ONLY` |

以上八项将真实运行、许可证、Kit 和支持矩阵保留为 Gate-I 后的实施证据；**目前只完成可审查设计，不创建 Tool 新按钮/产品代码，也不由 T-01 改写已批准 H-T01=A**。

## 4. X-01 安装/配置/运行/回退的全链路产品矩阵

| 现场角色/目的 | 必需安装 | 可信配置和安全材料 | 升级/卸载的强制结果 |
| --- | --- | --- | --- |
| Qt HMI / 装机制造商 | HyRemote Framework SDK/Runtime 依目标 Kit 与选定前端（应用可随包部署） | H-02a 新架构 Host DeploymentProfile，Target/Scope、真实协议监听、安全材料 | 启动准入 fail closed；不能破坏现场 Qt 业务应用；版本/Qt ABI 与 public SDK 协议兼容真实记录 |
| 工厂 Direct 工程师 | HyRemoteView | 本地 DeviceProfile、Native HMI TrustBundle/本机秘密库，RFB 风险说明 | 断网继续使用 LAN Direct；不安装 Gateway/独立 Edge |
| 工厂可选 Edge 共享管理员 | **同一个 HyRemoteView 产品**，可选内部 Edge runtime/service role | Site/EdgeInstance/HMI DeviceId+AllowedEndpoint 白名单，受限管理权限，默认 OFF | 禁用/撤销即时本地拒 Route；升级不复活共享；卸载清理后台服务和有效密钥 |
| 异地服务人员 | HyRemoteView | Gateway Operator/可见站点、受信 HMI 设备身份证明、独立 Native HMI 用户认证 | 站点/Gateway 更新不让 Device Trust 默默改变；断线不重放业务操作 |
| 厂商或客户运维中心 | 可选 HyRemote Gateway（自主部署/托管） | Gateway TLS/租户/Edge 身份与 Pair 消费账本，备份/恢复/证书轮换 | 不存 HMI 画面或 Device TLS 私钥；更新保持租户隔离和不可复活已撤销路由 |
| Qt 开发者本地工具 | 可选 HyRemoteTool | 读取用户选定 SDK 路径、Kit、日志，不代表设备 TrustBundle | Tool 独立更新，不能强迫 Viewer/SDK 同版本，旧会话能力必须按真实支持 |

### 4.1 Qt/平台与证据说明

- Framework：当前源码承诺四种接入技术、Widgets/Qt Quick target；Qt5.15 和 Qt6.8 的每个 Windows x86_64 Kit/构建方式、CMake、部署和运行证据必须有**精确**准入单元；QPA 私有 ABI 受 exact Qt Kit 约束，Shared Runtime 的窄范围 Qt Private 输入例外同样影响实际二进制资格。
- HyRemoteView：当前 G2 选 `Qt 6.8/C++17/Qt Quick`，Windows/Linux G3 技术资格按 View #5/9 真实覆盖，**NOT PASSED**，不能拿 Framework Windows evidence 扩张 View 的 Supported 平台。
- Linux/ARM/Wayland/EGLFS：按各产品激活版本和适用 Kit/平台分别标 `SUPPORTED / LIMITED / EXPERIMENTAL / NOT_RUN / NOT_IN_SCOPE`，不通过泛化“Qt 支持”外推。Framework 旧 V0.3.x Windows-first Owner 决策不等于此未来产品线必须所有平台一次交付。
- Qt6.5/6.12 对当前 Framework 首次 GA 的产品路线属于 post-GA planned，具体变更必须服从 #605/版本激活；不因 D4 设计稿自动提前纳入。
- 双协议构建：RFB-only、Native-only、Dual 的编译依赖、Qt 插件、加密库/授权、安装包大小以及实际 kit 可重现测试；`Native-only` 必须证明没有 RFB 帧依赖，`RFB-only` 不强制携带 Native 密钥库。

### 4.2 交付包必须有的元数据

为每个独立产品或安装包记录 `product,version,source_sha,build_kit,compiler,Qt_version,platform_arch,enabled_protocols,security_build_capabilities,auth_mode,dependency_licenses,installed_artifacts,upgrade_from,rollback_constraints,qualification_ref`。所有字段指向可复核的产品事实；`enabled_protocols` 只写实际构建可用，不因已设计 Native 1.0 就把它填 `SUPPORTED`。

配置迁移独立设计：工厂 Edge 共享白名单/策略修订、View 的 Device Trust、Tool 的 SDK 偏好和 Gateway Tenant Registry **四种不同持久化合同**；不得把其中一类的 JSON 原样复制成另一类的权限来源。未知 schema 或旧版本回滚无法证明有效安全策略时必须 fail closed，而不是悄悄采用空密码或全网卡默认监听。

### 4.3 X-01 安装、迁移、升级与失败回退的实施级合同（不增加产品）

此处细化**现有四种产品/交付责任**，不是新发布计划、安装向导原型或正式产物。每个安装流程必须把**软件落盘、可信配置、启用远程监听、设备授权生效、外部可用状态**分成可证伪的独立结果。完成安装不等于安全监听已开放、用户已登录或 Gate-G3/产品正式资格已通过。

| 受控对象 | 写入/运行权威 | 持久状态与最小关联 | 升级/回滚不可破坏条件 |
| --- | --- | --- | --- |
| `InstallManifest` | 各产品 X-01 Installer/PackageOwner | ProductId、package version、source SHA、Qt Kit/ABI、协议构建能力、文件清单/hash、dependency licenses、部署路径 | 安装器只能声称 manifest 真正拥有的文件/组件；不能把 Native 仅有设计写为已实现 |
| `RuntimeDeploymentProfile` | **HMI 宿主受信部署管理员**，F-06 仅消费并准入 | Host identity、Target/Scope、required listeners、RFB/Native TLS 材料引用与策略代次 | RFB 默认地址配置 getter 不是有效准入；错误/缺失签名必须 fail closed，不因卸载 Tool/View 打开监听 |
| `ViewDeviceCatalog/TrustStore` | HyRemoteView V-03 当前产品凭据边界 | DeviceId、trust anchor/pin、profile 代际、用户自有偏好（与 trust 结论分离） | 地址/证书/设备身份证明变化后不得自动接受新设备；迁移凭据不得明文写入日志/备份 |
| `FactorySitePolicy/EdgeRunLease` | 同一 View 安装中的 E-01 本地管理员/运行角色 | 白名单、允许端点/期限、单调 SitePolicyRevision **可持久**；EdgeRunEpoch/登录态租约 **不持久** | H-EDGE=A：升级、崩溃/回滚、注销后不得自动复活旧 Route 或后台运行，关闭单个会话窗口不改共享 |
| `GatewayTenantRegistry/PairLedger` | 独立可选 Gateway G-01 受信系统管理员 | Tenant/Site/Operator/Edge identity、租户 ACL、设备路由准入和一次性配对记录 | 不跨租户恢复旧 credential，不把备份 Route/Permit 当新运行许可，密钥轮换按受信身份验证 |
| `ToolSdkPreferences` | 独立 Tool T-01 用户/本机权限 | SDK path、Qt Kit、诊断偏好及经许可导出报告 | Tool 更新不强制 View/HMI 同步升级，已发布基础 RFB 访问能力不被删除，完整 S1 #586 OPEN |

#### A. 安装与升级事务（按产品独立执行）

| 事务阶段 | 原子边界/执行责任 | 失败时必须出现的真实结果 |
| --- | --- | --- |
| `PACKAGE_VERIFY` | X-01 验证包签名/内容清单、依赖许可证、平台架构、精确 Qt Kit；任何可执行迁移脚本也必须同一受信来源 | 不可信包**不能执行脚本/替换文件**，记录失败原因，原有效安装不被破坏 |
| `PLAN` | 枚举当前组件所有权、安装冲突、关联服务/应用版本及配置 schema；严格区分「仅 Viewer/Tool 更新」与「HMI 宿主部署」 | ABI 不兼容/路径越权直接停止；不得借改安装包把单 View 拆成独立 Edge 产品 |
| `QUIESCE` | View 升级先让本机 Edge 运行许可与 Route 真正停止；Gateway 升级先停止新的 Route/Pair，清旧一次性凭据；Framework 必须由宿主控制合格停机窗口 | 非正常退出只能保证本机不再提供旧 socket/运行态；不能承诺跨 OS 强杀过程的完美日志通知。现场 HMI 业务不能被远程更新器强行终止 |
| `SNAPSHOT` | 为允许回滚的**产品自有**持久配置做受保护快照，记录 schema 及不可倒退的许可/撤权高水位 | 读/解密/备份失败不继续危险迁移；敏感秘密不写入普通备份/日志/用户数据导出 |
| `STAGE` / `SWITCH` | 先核对新二进制、插件/Qt Private exact ABI、可读取旧配置的 schema；受控路径原子替换/版本切换 | 部分文件落盘不宣布成功，旧进程/插件不混入新 Kit；失败回滚软件文件但**不恢复旧许可** |
| `MIGRATE` | 每产品只在自身身份/ACL 保护下迁移：Host Policy、View Trust、Factory SitePolicy、Gateway Tenant ACL、Tool Preferences 各自独立，保持单调策略撤销高水位 | 未知 schema、未来版本数据、旧回滚不能保守解释时标 `NEEDS_RECOVERY/DENY`，不自动采用无密码/共享全网策略 |
| `QUALIFY_AND_ACTIVATE` | 启动当前版本但先关闭网络/路由准入，验证配置/秘密、Qt Kit/证书/设备信任后由真实运行权威重新批准可服务状态 | `PACKAGE_INSTALLED`、`RUNTIME_RUNNING`、`DEVICE_TRUSTED`、`HMI_GRANTED`、`EDGE_ONLINE`、`QUALIFIED` 是不同状态，不得一键宣称成功 |
| `ROLLBACK/UNINSTALL` | 软件可退回经签名允许的版本；配置仅在不撤销更严格的新安全策略时可恢复；卸载撤运行租约/当前 socket 与应用自有秘密 | 回滚旧版本无法理解新撤权版本时保持离线/拒绝，**绝不恢复被禁用的共享或过期令牌**；不删除宿主业务无关的现场数据 |

**事务不是单一 OS 原子指令**：安装器可采用平台事务/目录交换/备份等不同机制，但**对外安全准入的原子性**必须由对应 Runtime/Policy 真实控制；不能因为文件复制完成或 Service 启动就自动接受 Native/RFB 会话。QPA/QtPrivate 例外必须在对应精确 Qt Kit 检查，不能仅靠文件名同名声称兼容。

#### B. 跨产品升级偏序与不允许的耦合

- HMI Framework 与 Qt 宿主按制造商签名/Qt ABI 资格独立发布；**安装升级不替宿主做业务授权选择**。无网络更新可仍使用已安装的合法 Direct 基础能力。
- HyRemoteView 作为单一产品统一更新自己的 Viewer、同产品 Edge 角色与可信资源；不另装 Sidecar，退出/注销后不保留未经额外审批的 Windows Service。升级后只恢复**已批准的持久配置**，不恢复 EdgeRunLease/PairCredential/旧 Grant。
- Gateway 是可选的自托管或供应商服务，有自己的滚动/备份和租户 ACL 演进，不能将 Gateway 更新变成 Direct 的硬依赖。Gateway 仅许可新 Route，不改变 HMI 现有 DeviceTLS/Target/View/ControlGrant。
- HyRemoteTool 维护已发布的基础 RFB 和 SDK/Kit 诊断，不因为 HyRemoteView 新功能进度去删除用户已发布入口，完整 Tool S1 真实状态继续 #586 OPEN。
- 各产品间通过**受控且已批准的**协议/接口版本兼容矩阵协作；本章节不为尚未交付的 Native/Gateway 擅造“已支持版本组合”或新的公开 API。

#### C. X-01 设计验收与 Owner 交付字段

| 设计合同 | 正/反向实施验收（未来） | 真正主责 | 状态 |
| --- | --- | --- | --- |
| `DEP-01` 包/依赖/Kit 准入 | 包损坏、签名错、Qt Private ABI 不符必须在替换/运行前拒绝；导出 manifest 可追溯 | X-01 Installer + 各组件 Owner | `DESIGN_ONLY` |
| `DEP-02` HMI 宿主业务隔离 | SDK/Plugin 升级出错时不强停 HMI 业务或无可信策略自动 bind listener | F-03/F-06 + X-01 | `DESIGN_ONLY` |
| `DEP-03` View 单产品/Edge 角色 | 安装/升级/卸载均无独立 Edge installer，登录态 RunLease 不跨重启；注销离线、单窗口关闭不撤共享 | V-03/E-01 + X-01 | `DESIGN_ONLY` |
| `DEP-04` 设备 Trust 迁移 | 同 IP 新证书/不同设备、未知 schema、凭据损坏时拒绝自动信任，不丢失可信历史回退轨迹 | V-03 + X-01 | `DESIGN_ONLY` |
| `DEP-05` 站点共享撤权与回滚 | 旧版本回滚后不能复活曾撤销的 HMI 分享、PairCredential、旧 Route/Grant；失败需 NEEDS_RECOVERY | E-01/G-01 + X-01 | `DESIGN_ONLY` |
| `DEP-06` Gateway Tenant/Key 轮换 | 不跨租户恢复 operator/edge token，丢密钥或 TTL 到期时现存 Route 安全关闭，不影响 Direct | G-01 + X-01 | `DESIGN_ONLY` |
| `DEP-07` Tool 兼容与独立更新 | SDK/Kit 不一致/Tool 版本不同不破坏 View/Framework；旧已发布 RFB 保留 | T-01 + X-01 | `DESIGN_ONLY` |
| `DEP-08` 版本/支持真实性 | 未通过真实 Kit/平台/许可证明的组合标 `NOT_RUN/NOT_SUPPORTED` 而非自动 SUPPORTED | X-01/X-02 | `DESIGN_ONLY` |

**设计出门条件**：安装角色、文件/配置/秘密的唯一 Owner、可信包/Qt Kit、取消与恢复、安全许可高水位及每一失败出口已审签。实际签名验证、OS 安装器、Windows/Linux Kit、无人值守、更新回滚资格和软件发行仍属于后续被独立批准的 Gate，不通过本文 Markdown 或 PR CI 自动通过。

## 5. X-02 安全、资源、可观测性、质量与全产品验收

### 5.1 安全性优先的失效序列

~~~mermaid
sequenceDiagram
  participant Host as HMI Host Policy
  participant Run as Framework / Runtime
  participant View as Remote View
  participant Edge as Factory View Edge
  participant GW as Gateway
  Host->>Run: 撤销某 Target/Scope View/ControlGrant
  Run->>Run: 提升设备 Policy/GrantEpoch，拒新输入/输出
  Run->>Run: 销毁旧未发送 Frame/Delta 与待投递输入
  Run-->>View: Protocol revoke / socket close
  View->>View: 清旧画面、输入焦点和持键
  GW->>Edge: 另一条 Route 仍可能保持网络连接
  Note over GW,Edge: Route online 永远不能推翻 HMI 权利撤销
  Host->>Host: 现场生产业务继续运行
~~~

全产品安全边界由 **三套身份**强制：Gateway Operator/Tenant/Site、工厂 Edge/HMI allowlist、HMI DeviceTLS/设备用户与设备授权。Tenant 配对只能申请 route，不代替 HMI Device Trust，所有组件遵守默认拒绝与最小披露。S05 控制 IDL 不携带 HMI Native 密钥、像素、按键、RFB VNC 口令。

### 5.2 资源/性能估算必须有一套统一账

`HMI_RSS = base_Qt_process + capture + Core_frame_mailbox + per_permission_frame + protocol_queues + TLS + Qt_input + OS_socket`

`View_RSS = base_QtQuick + network_parser/decoder + bounded_frame_buffers + GPU_textures + trust_store_cache + managed_adapter + optional_edge_role`

`Edge/Gateway_RSS = base_service + connection/auth + route_count × (2 × direction_queue + socket_TLS) + metadata/audit`

说明：HMI 的 V8 128MiB 是设备侧预算目标；Edge/Gateway/View 的数值必须另测。原则上所有进程按 `0/1/2 viewers`、静态/小更新/高动态、正常/慢端/断网/撤权、GUI 登录/后台模式记录 CPU（明确 % 核心基数）、RSS/PSS、网络吞吐、Frame age P95、输入→可见延迟、socket/队列水位。含共享 FrameStorage 时内存计数应避免双计，但不得忽略每会话遮罩或 TLS buffer。**不能把降低刷新或保留旧帧当性能优化。**

### Native 1.0 补充规范的验收输入

XP-03/04/05/06/07/10/20/21 必须复核 [Human 批准的 Native V8 数值/档位合同](module-detail-w00-native-v8-delta01-approved-2026-11-05.md) 和 [26 个黄金字节向量与 12 个异常规格](module-detail-w00-native-v8-golden-vectors-design-2026-11-05.md)。两个档位都必须设备 TLS-first；`plain` 只是不额外进行 Native 用户密码认证，不是开放匿名设备控制。验收要逐项记录独立 Host/Viewer 构建 SHA、Qt Kit/平台、可信设备身份、Host Scope 与是否真实执行。当前新增向量全部未运行，不能因为 Human 批准数字而写 PASS。

### F05/V01 正交状态机的验收映射补充

[Native AFS-01..AFS-12 原子设计合同](module-detail-d1-device-protocols-2026-11-05.md) 已覆盖 TLS-first/plain、认证与独立 ViewGrant、ControlState/Scope/现场接管、Target 撤销、FULL/DELTA、Qt 双栅栏和 Managed Route 失败。这些是原 `FW/DP/VIEW/XP` 的**细化设计入口**，不改动已有测试用例编号与 Native wire。QA 在正式实现后须采集 **HMI SHA + Viewer SHA + 平台 Kit + 真实 Policy/Grant epoch + 正负日志**，并记录每个 `AFS` 到测试 ID 的对应；尚无真实执行结果，全部 `DESIGN_ONLY`。不能让一条成功 TLS 或一张正确画面代替全部权限/隐私/撤权验收。


### F-02 / F-06 可实施合同的设计验收与未来产品验证分离

D1 已在 [F-02 RT-01..08 / F-06 POL-01..08](module-detail-d1-framework-boundaries-2026-11-05.md) 界定资源/对象唯一 Owner、per-session Filter/Full/Delta、PolicySnapshot、Qt 最终输入线性化、按来源 held 账本及撤权在途边界。其审查入口如下：

| 审查时机 | 需要什么证据 | 不可偷换为 |
| --- | --- | --- |
| **Gate-D 设计冻结** | 全部 RT/POL 合同有明确源条款、Owner、前后状态、线程所有者、失败闭合、预算边界和至少一个正/负 TestSpec；与 Native AFS-01..12、D2/D3 的消费合同一致且无未裁决关键冲突 | 不要求尚未获 Gate-I 的正式 Host/Viewer 产品代码与真机互通已经 PASS |
| 适用 **Gate-G3 技术 PoC** | 对真正威胁方案可行性的 TLS/Qt Render/Host policy 并发等假设，按已获准的 PoC 范围、精确 SHA/Kit/环境单独验证；不外推产品支持 | 不能拿设计合并或旧 PoC 局部结果冒充完整产品 |
| **Gate-I 后实现级测试** | `FW-02/03/04/05/06/07/08/09/12`、`AFS-01..12`、`XP-05/06/25/26/27` 按精确 Host/Viewer 构建测试来源会话隔离、Qt 抢占、隐私旧帧、资源上限与非法消息 | 不能把未运行的 XP 计划标为 PASS、不能拿单一 Qt Kit 外推所有平台 |

这些要求仍服从已批准 H-RFB/H-DUAL/H-REVOKE、Native P-A/A，既不增加第五种接入也不创建新的 Native wire。

### V-02 RemoteSurface 跨线程/图像/输入的设计验收入口

已有 D2 [SURF-01..08](module-detail-d2-hyremoteview-client-2026-11-05.md) 细化了 Qt Quick GUI/RenderThread、decoder staging/immutable FrameSnapshot、DisplayEpoch、FULL/DELTA、输入坐标及焦点/Grant 的唯一责任。Gate-D 需审签其**对象所有权、失败闭合、状态/时序和精确可证伪用例**，不要求提前编译完完整 V-02 产品；实施批准后再按适用 G3/Viewer 产品 Gate 记录 Qt6.8 Windows/Linux 精确 Kit 下的像素一致性、背景/屏幕切换、帧龄 P95、CPU/RSS/GPU 资源、恶意 RECT、撤权与按键释放证据。

**SURF-04/07 P2 负向设计**：保持某个 RFB/Native 控制会话的键/鼠标按钮 DOWN、再触发 HMI Target 尺寸/旋转/输入坐标几何变化；验证 View 先阻断新 DOWN、在仍有效的 Session/Scope/Grant 下按协议合法释放，无法安全释放时设备 F-06 `POL-08` 按其 per-source held ledger 保护性清理。旧 transform revision 的迟到 UP 不能导致卡键；设备不重复释放，也不释放其他合法 peer/现场来源。并发撤权时客户端不得以旧 Grant 发送释放，HMI 清理必须生效。**全是未来测试规格，目前 NOT_RUN**。

本轮关联 `VIEW-06/07/08/09/12`、`XP-02/17/21/25/26/27` 及 `AFS-05/07/09`；**所有新增 SURF 验收内容均是设计，不是已运行产品测试**。特别检查 RFB 没有 Native Grant wire、失败时客户端无权虚构设备已发送撤权 ACK，Scene Graph 本地 PresentAck 也不得冒充 Native 帧 ACK。

### F-03 四入口一致性与 SDK/Qt Kit 验收合同

[D1 的 F-03 `INT-01..10`](module-detail-d1-framework-boundaries-2026-11-05.md) 已明确 C++/QML/Generic/QPA **四个已有接入前端**的 bootstrap、配置归一化、线程所有权、Host admission、错误与部署资格。X-01/X-02 不得把“一个入口编译/运行成功”外推其他入口。

- **设计审签 Gate-D**：10 条 INT 都能追溯既有公共 facade/Qt Plugin/ADR-0007/0008、Target 与 Policy 权威及错误出口；Generic/QPA 共享自动访问控制器，不开第二个设备授权系统。无须预先完成正式产品改造/真机测试。
- **未来实测**：`FW-01/02/03/06/09/10`、`XP-01/14/19` 分别覆盖 C++ Widgets/Quick、QML 构建完成前/后 enabled、Generic address/interface 互斥、QPA native delegate 的窗口/输入与错误时独立存活，以及四入口对同 Host Target/Scope 的相同拒绝/撤权结果。
- **精确 Kit 和兼容**：Qt5.15/Qt6.8、平台/arch/编译器各产品所批准的实际单元验证，QPA private shim 和 Runtime `AR-04=A` 内部窗口输入例外不得进入 SDK public ABI；仅配置 getter `0.0.0.0:5921` 无法证明已获安全公网监听资格。
- **证据字段**：必须在实施后保存实际源 SHA、Kit/运行平台、原生 QPA 类型、插件参数/安全 profile/Host Policy 来源、实际 Listener/Target/held 结果和拒绝状态，不把 `DESIGN_ONLY` 写成 `PASS`。

### E-01/G-01 透明路由与首批 Edge 的验收责任

[D3 内置 Edge/Gateway `EG-01..12` 合同](module-detail-d3-managed-integration-2026-11-05.md) 是 X-02 的设计审签入口：管理权限与本地 SitePolicyRevision 持久化、Offer→Dial 的二次准入、一次性双侧 Pair、RouteEpoch CLOSED 终态、有界 FIFO（绝不裁剪 TLS 字节）、HMI 设备授权独立、用户登录期间产品生命周期。映射既有 `MG-01..16`、`XP-08..13/18/24/27` 与 Stage05 `CTRL` 负例；全部当前 `DESIGN_ONLY / NOT_RUN`。

**Gate-D** 审查身份/数据唯一 owner、原子撤权/持久化失败关闭、资源公式和负向规格及与 Stage05 唯一 IDL 的一致性，不先要求正式 Gateway/Public 产品代码。**Gate-I/产品实施之后**才在真正工厂 PC 与自托管目标平台逐项验证 OS/Qt Kit、慢端/断线、TLS E2E、IPC ACL、密钥迁移以及更新回退。Gateway #190 激活另有 Human Gate，不能由 EG 文档合并推出。

### V-03 设备目录、TrustStore、OS 秘密及 OEM 的验收设计

[D2 V-03 `IDT-01..10`](module-detail-d2-hyremoteview-client-2026-11-05.md) 直接覆盖本地地址/DeviceId/Operator 与 HMI Grant 分层、可追责的可信初始锚、证书换发/TrustRevision、OS SecretStore 按 purpose 隔离、Schema 原子迁移/降级、诊断真实性与 OEM/工厂共享边界。

Gate-D 审查 **Owner 与数据 schema、增权先安全验证并持久化/撤权先关闭准入、独立身份锚、失败关闭的状态/负例、凭据最小暴露与回退合同**；不以实际平台 OS SecretStore 成功读写为设计 Gate 的伪前置。正式获准实施后按 `VIEW-03/05/07/10/11/12`、`XP-11/14/16/19/23/24/28` 的实际 Windows/Linux Kit、身份/凭据/磁盘故障注入、升级回退和不串租户证据独立验收；全部当前 `DESIGN_ONLY/NOT_RUN`，不从已有 RFB 客户端测试推断 Native TrustStore 已实现。

### Factory Edge 首批生命周期的可证伪验收合同

D3 的 [EDGE-LC-01..08](module-detail-d3-managed-integration-2026-11-05.md) 与 D2 `VIEW-EDGE-01..05` 已定义登录态分享和用户操作边界。D4 必须将以下**不同事件**分别构成未来设计验收：关闭一个远程画面（共享不因此自动断）、关闭站点管理页面（共享不因此自动断）、退出整个 View、OS 用户注销、系统锁屏/睡眠、进程强杀、网络中断、撤销 HMI、策略持久化失败、升级回滚/凭据轮换。只有真实 OS/Qt Kit、带准确 RunEpoch/RouteId/SitePolicyRevision 及 Socket/Route 证据才能 PASS。

本轮冻结的是**验收口径**：首批 `H-EDGE=A` 仅登录状态并显式启用时工作；不得虚构 24×7 Service，也不得把关闭一个窗口等于退出整个 View。Direct 的设备信任和 HMI 授权仍不依赖 Gateway。全部 `DESIGN_ONLY/NOT_RUN`，不授 Gate-G3/Gate-I。


### 标准 RFB 传统用户权利与 F-06 Scope 的设备端验收合同

[设备 F-04 `RLG-01..09` 设计合同](module-detail-d1-device-protocols-2026-11-05.md) 要求维护中标准第三方 Viewer 对 RFB 3.8/None/VNCAuth 的**真实互通**，同时验证匿名/共享口令的固定 Target/Scope 查看资格、宿主明确启用的传统输入、双协议共 Scope Controller、现场抢占、Qt 最终输入栅栏、跨 peer held 不串、撤权旧帧与回调失效。Legacy 旧版“remoteInputEnabled=true”只能作为过去已发布能力事实，不代表未来新架构已测试单 Scope 强制仲裁。

XP-02/04/05/06/20/FW-04/05/06/07 和 `RLG-01..09` 的正反证须按真实 RFB 第三方客户端、F-04/06 HMI exact SHA、OS/Qt Kit/Target/Scope/PolicyEpoch 归档；不许用官方 Native Viewer 的 Grant 测试替代传统 RFB 兼容。全部仍是 `DESIGN_ONLY/NOT_RUN`，不增加新的 RFB wire，不重新审核 H-RFB=A 产品选择。

### 5.2.1 Gate-D 原子反证的设计签收接口

| 入口 | 设计冻结前必须唯一确定的事实 | 未来负向验证的观察与禁止输出 |
| --- | --- | --- |
| RACE-02 / FW-07/08 / AFS-07/08 | F-02 最后明文出队与完整 FULL 基线提交、逐 Session Policy/Privacy/Queue 版本 | 中途撤权、部分 TLS 写、DELTA 丢弃/慢端；旧许可不产生新增可控字节，不用 SocketWrite 声称 Viewer 已呈现 |
| RACE-03 / FW-06 / AFS-09 | 带可信 Authorization 的输入票据贯穿复合/叶子队列，Qt 最后投递许可与撤权真实线性化、按来源 held/136 保护预算 | 两级 GUI 队列停顿与现场抢占；旧 DOWN 失权后不得实际投递，保护性 UP 不释放其他来源；撤权未确认不报已生效 |
| RACE-05 / XP-24 | E-01 deny-first、持久提交资格及掉电新进程 OFF/管理员显式新授信 | fsync 失败、强杀/重启、备份旧 Policy/旧 Gateway Offer；无新批准零 Route，合法 Direct 不中断 |

以上只要求**可审查设计与负例判定方法**，不是在 Gate-D 前要求新功能 E2E 实测 PASS。实际 Qt Kit 末端 API、保护容量、持久恢复的技术可行性若仍无法证明，保留 Gate-D OPEN，不能留到实施时临时选型。

### 5.2.2 多来源 held 与保护性释放预算的审查判据

RACE-03 除“GUI 两级队列后撤权”外，还必须固定如下反证记录：**实际 Qt 叶子对象及 TargetIncarnation**、ScopeConflictGraph 冲突裁决、各会话的源感知 Held、物理 AggregateHeld、普通 mailbox 高水位（≤64）、保护性 Qt release 高水位（≤136）、每个 permit 的进入/完成代次及 RevokePending/Effective 时刻。测试必须区分“8×72 个逻辑来源-held 组合”与“最多 72 个物理已按下类别”，不得将每来源释放入队数与物理 Qt Up 上界混算。对于无法证明隔离的同叶子双 Scope，**期待拒绝第二 ControlGrant**，不得为达标临时放宽 Qt 输入语义或当场更改 V8 预算。具体 Qt Kit 独立运行与容量测量尚未进行，记录为 DESIGN_ONLY/NOT_RUN。

### 5.2.3 Native 五按钮与十七种小键盘键码的边界反证

把 V8 P-A 的合法值 1/2/3/4/5 与非法值 0/6 分为**协议接受**、**设备最终 Qt 投递**、**已投递 held 平衡**三层独立断言。F-05/V-01 真 parser 应识别全部合法值，不能将 X1/X2 无声映射为 None/Left，也不能使用 INVALID_FIELD 拒绝合法按钮。Qt5/Qt6 Widgets/Quick/Composite 必须有可验证的 X1/X2→对应叶子按钮 Down/Up、会话/Scope 撤权、重连/焦点转移保护释放、真实重复按键与单控制者判定。五种按钮、17 个独立 Keypad 与旧 69 逻辑键合计 91 个**可能类别**，但同一 leaf 实时 held 上限 72、普通 pending 上限 64、保护性释放上限 136，达到限额前拒绝新 DOWN 而不是丢旧 UP。负例：69 旧键 + 3 按钮已满时 X1/X2 或任意 Keypad 新 held 必须明确资源拒绝；后释放后再请求 X1/X2/Keypad 都必须正确执行并平衡。原值 `KP_0..KP_9`、`KP_ENTER` 与主键区对应键的 Qt 身份和 `KeypadModifier` 必须逐值验证，禁止错误映射或把 KEY 重复提交为 COMMITTED_TEXT。不得以旧三按钮单元测试 PASS 代替五按钮符合性；全项 NOT_RUN。

### 5.2.4 高动态画面与受管内存预算反证（V8 附录 C）

XP-17/27、FW-07/08/12 的测试计划必须包含持续**大面积变化**而非只看准静态画面。静态算术边界：1920×1080×4 字节完整 RGBA ≈7.91 MiB，8 个不同隐私用户的独立全帧约 63.28 MiB；若共享 RawPool 恰占 V8 96 MiB，另有 8 条 Native 待发各 512 KiB（共 4 MiB），仅这些受管对象就 **>163 MiB**，与 128 MiB 目标不能同时成立。未来正式验收要记录：Source/Target 真实画面变化率与损伤面积，真实 owned raw/遮罩/编码/Qt stage bytes，per Session 取消与 FULL 复原、Native 512KiB/连接待发、128MiB 受管水位，以及**另外**实际 RSS/CPU/GPU/帧龄 P50/P95。失败时必须证明**只拒绝/背压受影响会话，不冻结现场 GUI，不泄露不同遮罩、不裁剪加密中继字节、不发生旧基线重用**。本项为设计内存/算术反证，不是当前已有真实性能测量或客户 SLA。

### 5.3 测试资格分层（不混用门禁）

| Gate | 证明什么 | 明确**不能**证明什么 |
| --- | --- | --- |
| `DESIGN_REVIEW` | 模块合同、图、状态、差异/风险、条款追踪得到审查 | 不代表代码可用 |
| `G3_POC` | View 技术假设在当前 exact Kit 与工况可行 | 不代表正式产品完成或安全可发布 |
| `FRAMEWORK_UNIT` | Core/Runtime/协议 parser 正负向受控测试 | 不代表维护中第三方 Viewer 或真 HMI DeviceTLS |
| `HMI_DEVICE_NATIVE_E2E` | View↔真正 HMI Native DeviceTLS/Auth/Grant/撤权，在 relay 外亦成立 | 不代表 Gateway/Edge 的自托管/后台/权限全部合格 |
| `MANAGED_ROUTE_SECURITY` | Edge allowlist、once-pair、tenant、WAN 背压/撤销 fail closed | 不代表 HMI 最终 View/ControlGrant |
| `VIEW_PRODUCT` | 正式 Client RemoteSurface/Input/Trust 真实 Windows/Linux 相应支持单元 | 不代表 Framework/Tool/ARM 全平台 |
| `INSTALL_UPGRADE` | 厂内“只装 View”、HMI SDK/Tool/Gateway 各自依赖和回滚 | 不代表 24x7 注销后在线或性能门槛 |
| `FULL_PRODUCT_E2E` | 从工厂登记到异地看/控/撤权/故障恢复，对应 V15 旅程逐项过证据 | 不代表已由 Human 激活版本/发布 |
| `RELEASE_APPROVAL` | 正式版本、风险接受与支持范围有权审批 | 不能从单一 CI 绿灯自动推出 |

**每条证据记录** `scenario_id, product, exact_sha, Qt_Kit, compiler/OS, configuration, test_command, raw_result, negative_cases, environment, reviewer, limitations, source_link, decision_scope`；缺测试写 `NOT_RUN`，不编造 PASS。

### 5.4 全产品关键用户验收场景（超出 Edge12）

> **2026-11-05 W00 证据补齐**：V15 表6 实有 **31** 个场景，经 [逐行追溯](module-detail-w00-v15-table6-xp-coverage-2026-11-05.md) 对照既有 XP01..20；为业务文件/IME/OEM 离线委托/View 更新/旧帧/慢中继/永久 Direct 等未充分显式覆盖的条款补 **XP21..28**，均仅设计，未实施或运行。

| ID | 端到端用户成果与拒绝情形 | 责任模块 |
| --- | --- | --- |
| XP-01 | Qt SDK 四种接入不产生重复 Runtime/隐式网络 listener；权限缺失时准入失败 | F01/02/03/06 |
| XP-02 | 仅 RFB 的 Legacy Viewer 在已允许 LAN 中正常看画面且如实说明安全等级 | F04 / V01 |
| XP-03 | Native-only HMI 与 View 在真正 Direct 离线下能看、获准后控制 | F05/06 / V01/02 |
| XP-04 | RFB+Native 多 Peer 争同 Scope 默认一名远程控制者；现场覆盖生效 | F04/05/06 |
| XP-05 | 设备撤权后队列中旧输入不会在 Qt GUI 执行，held 状态清理一次 | F06 / V02 |
| XP-06 | 不同身份的 Target/Scope 遮罩与缓存独立，旧像素/Delta 不跨许可复活 | F02/05/06 / V02 |
| XP-07 | Native DeviceTLS 更换证书、HMI 密码和 Root 变化均失败关闭，不泄露秘密 | F05 / V01/03 |
| XP-08 | 工厂 PC 只安装 View，默认未开启 HMI 对外共享；管理员批准单设备后可撤销 | V03 / E01 / X01 |
| XP-09 | 未共享 HMI2/同名另一站/任意内网 TCP 不可经 Edge 被拨通 | E01/G01 |
| XP-10 | Gateway/Edge 透明中转不拥有设备明文；远端 HMI ViewGrant 被拒零像素 | F05/06 / V01 / E01/G01 |
| XP-11 | Gateway/Tenant/Route 撤销或云断网不影响合法 LAN Direct/现场业务 | 全域 |
| XP-12 | H-EDGE=A 首批：关闭单个 RemoteSurface 窗口不误关闭已获准共享；用户明确退出整个 View、注销或结束登录会话时 Edge 真实离线；升级回退后状态准确。注销后 SCM 无人值守在线不属于首批支持 | V03/E01/X01 |
| XP-13 | 工厂 Edge 复制/重放 ticket 与旧 SitePolicyRevision 无权路由 | E01/G01 |
| XP-14 | Tool 选择真实 SDK/Qt Kit 生成四路线准确步骤和脱敏证据，无伪远程诊断 | T01 |
| XP-15 | Tool 已有基础 RFB 与未完 S1 规划、View 新官方访问产品各自独立安装/升级且不倒退 | T01/V01/X01，**H-T01=A 已由 Human 批准，原 Tool S1 #586 仍 OPEN** |
| XP-16 | OEM 多品牌不绕 TLS/设备许可，不改变 Native wire、不串持久资料 | V03/T01/X01 |
| XP-17 | View/HMI/Tool/Edge/Gateway 逐进程 1080p CPU/帧龄/RSS、慢端和队列均可复现 | X02 |
| XP-18 | 自托管 Gateway 证书轮换、备份/迁移、升级/回滚不会复活已撤 Route/共享 | G01/X01 |
| XP-19 | 无公网的工厂可独立使用 RFB/Native Direct，未安装 Gateway 时 UI 不报错 | V01/X01 |
| XP-20 | 权限/凭据/界面功能尚未实现时统一显示不支持/未知，禁止假按钮和假资格 | 全域 |
| XP-21 | 中文/IME/已提交文本完整，远端触控能力与本地手势明确区分；Native 1.0 不伪称原生多点触控已支持 | F05/F06/V02 |
| XP-22 | **H-O01=A 当前期负例**：文件/Host 回执功能明确 UNSUPPORTED，不能借 INPUT/剪贴板/普通 RFB 假上传或宣称已生效；未来扩展期才分别验证设备收件、校验、Host 业务回执和路径越界 | F06/V03/X02 |
| XP-23 | **H-O02=A 当前期事实验证**：离线委托撤销待送达不等于设备已收件/已生效，有限期限以受信本地到期为准；不存在受控设备回执时不能显示已确认，合法永久离线 Direct 不受影响 | F06/V03/X02 |
| XP-24 | HyRemoteView 自身可信更新/离线安装/档案和凭据迁移/失败回退；同产品 Edge 无假在线 | V03/E01/X01 |
| XP-25 | 第三方 VNC Viewer 不能远程删除已存副本；设备撤权后不新输出旧权限像素，第一方 View 自有缓存清除 | F02/F06/V02/X02 |
| XP-26 | 断线旧画面按宿主政策分敏感隐藏和普通仅可标过期；重连/在线撤权不复活旧帧、输入或业务命令 | F02/F06/V02/V03 |
| XP-27 | 慢 Viewer 与多 HMI 下 Edge/Gateway 仅做有界透明 FIFO/背压；不删加密字节、不做 HMI 第二次采集 | F02/E01/G01/X02 |
| XP-28 | 已合法获得永久基础 Direct 使用权的客户在托管/售后到期时仍依设备安全授权离线使用，不被订阅失效误关 | F06/V03/X01/X02 |

上述 **XP-01..XP-28 全部属于设计验收合同，不是已运行 PASS**。与 V15 表6 的 31 个真实源场景逐项对应见 [W00 S01..S31 ↔ XP 映射](module-detail-w00-v15-table6-xp-coverage-2026-11-05.md)。**31/31 是验收入口设计覆盖，不等于可执行测试完成或 56 项 V15 复合要求已逐原子签收。**

### 5.5 XP-22/23 业务结果与离线撤销的事实等级和失败闭合（不擅造实现接口）

根据 V15 §9.2/§12.1 和 V8 §13.3/§14.3 的来源追踪，以下仅确定 **XP-22 业务文件/宿主结果**及 **XP-23 离线委托撤销**的可信状态、来源和错误闭合，**不声称**已实现文件远程管理、设备 ACK 或离线送达；**产品分期已由 Human H-O01=A/H-O02=A 批准**，本期仅安全能力边界与事实等级，后续可信文件/回执通道单独受控设计。V8 Native 1.0 附录 A.2 既有 **21 种消息中没有文件上传/业务生效回执/离线撤销设备回执消息**；不能将 `INPUT/COMMITTED_TEXT`、普通 RFB 键鼠或 TCP ACK 偷偷重新解释为新协议功能。改变已批准的 wire/正式产品范围须走 Human 受控流程。

| 事实层级（内部设计记号，不是协议 enum） | 唯一可信来源 | 严禁被提升成 | 失败闭合与真实 UI |
| --- | --- | --- | --- |
| `CAPABILITY_UNAVAILABLE` | V-01/V-03 当前经过验证的端点协议能力；Native1.0 无文件/业务结果消息 | 已支持安全文件传输 | 隐藏/禁用假功能，明确 UNSUPPORTED；不使用未经允许的剪贴板/键鼠替代文件接口 |
| `CLIENT_QUEUED/STREAM_WRITTEN` | 发送端当前 Session 本机有界队列/网络写入结果 | 设备已接收/宿主已执行 | 丢连接、超时、重连后不得自动重放可能有副作用的命令 |
| `DEVICE_RECEIVED/INTEGRITY_VERIFIED` | 仅未来 HMI 受信文件接收/校验证据，精确 DeviceId/操作 Id/摘要/会话代次 | 业务宿主接受或已生效 | 没有受信接收证据显示 OUTCOME_UNKNOWN，不让客户端摘要验证冒充设备确认 |
| `HOST_ACCEPTED/BUSINESS_EFFECT_APPLIED` | 仅未来 HMI 业务应用真实回执/持久状态读取；F-06 Host 独立 Owner | TLS 已写入就等于参数生效/告警已确认 | 无 Host 可信回执只可显示 OUTCOME_UNKNOWN，需现场复核，不自动重试业务动作 |
| `REVOKE_ISSUED/PENDING_DEVICE_DELIVERY` | 客户/售后委托签发方自己的受信变更及持久日志 | 离线 HMI 已撤销授权 | HMI 离线/无可信设备回执时显示待送达或未知，Gateway/Edge 不得代为推进 GrantEpoch |
| `DEVICE_RECEIPT_VERIFIED/POLICY_EFFECTIVE` | 仅实际 HMI 已认证的设备收件证明和当前本地 F-06 PolicyRevision/GrantEpoch 生效证据 | Gateway 配对/RouteClose 就是设备撤销 | 设备身份/签名/代际不符则不能显示“已生效”；旧拒绝和 held/帧撤销仍以 Host 最终栅栏为准 |
| `DELEGATION_EXPIRED_OFFLINE` | HMI 本地已批准的有限委托到期及可信设备时间/单调期限证据 | 合法永久 Direct 使用权益到期 | 时钟无法证明则不能伪称已远端送达；有限远控委托保守拒绝，不借云订阅停用合法基础 Direct |

**权威不可转移**：文件/业务操作许可、最终业务状态、设备当前 Policy/Grant 仍归 HMI Host；客户端/Tool 只呈现有来源、带时间与作用域的事实；Gateway/Edge 只为自有 Route 负责，不保存/解读业务文件、不签发设备接收回执。上述全大写状态是**设计内部事实分类**，并非新 Native wire、SDK API 或已发布 UI。

**Human 已裁决分期，未来接口仍未定义（非本期实施阻塞）**：H-O01=A 将 Host 文件接收/沙箱/OperationId/业务回执延后为独立受控扩展；H-O02=A 确立离线待送达≠设备生效与有限期限的事实语义，可信签名收件/回执与持久恢复通道后续单独审签。本期应签收的是 UNSUPPORTED、OUTCOME_UNKNOWN、离线授权期限与永久 Direct 独立的 UI/FailClosed 合同；**不得**重新标为“Human 分期选择 OPEN”或将延期功能强塞现有 Native 21 消息。

**未来负向验收**：XP-22 必须将“写出/设备收到/Host 拒绝/业务结果未知”、损坏文件、路径越权、断线重试分开；XP-23/28 必须将“已签发未送达/设备明确确认/可信期限到期/旧撤销重放/永久 Direct 继续使用”分别记录。全部 `DESIGN_ONLY/NOT_RUN`；本节只明确安全事实与失败闭合，不宣称实现接口及阶段已批准。

### 5.5.1 可信时间与委托到期的追加反证判定（H-O02=A）

XP-23/28 的“受信本地期限到期”必须分别证明 **Issuer 签发/待送达、HMI 本地可信到期、设备实际可信收件与永久 Direct 权益** 四种事实；无法通过普通系统 wall clock 回拨推断 Lease 仍有效，也不能把 TIME_UNVERIFIED 的保守拒绝写为“已收到撤销/已真正到期”。记录 Host 当前 PolicyRevision/GrantEpoch、LeaseId/委托范围、可信时间来源和启动代次、可否跨重启证明期限、最终 Qt 输入撤权栅栏及出帧门。

设计反证应覆盖墙钟向后/向前跳变、在线时间源断开、掉电重启后只剩可回滚旧备份、旧续租重放、到期与 GUI 最终投递/帧明文出队并发、云端待送达但设备期限已独立届满，并确认合法永久 Direct 不被有限售后策略误关。缺少受信当前时间时，**临时委托必须 fail closed，而不是另发永久权益撤销**。这些测试尚未执行，仍为 DESIGN_ONLY/NOT_RUN，不推出必须加装安全 RTC 或新 Native 消息。

## 6. 已获 Human 批准的产品决策与仍未关闭的源审计

| ID | 问题 | 当前技术倾向 | 触发审批 |
| --- | --- | --- | --- |
| H-T01 | Tool 与 View 长期分工 | **HUMAN_APPROVED_A**：维护 Tool 已发布基础 RFB，完整 S1 #586 仍 OPEN；未来完整 Native/Managed/Edge/官方 Viewer 主线归 View，Tool 重点 SDK/Qt Kit/诊断 | 不再重开产品分工；共用 RFB 解析器/打包须技术与许可证审查 |
| H-DUAL | HMI Dual 端点启动 | **HUMAN_APPROVED_A**：声明为必需的所有协议端点原子准入/启动，失败整体拒绝；运行期局部故障隔离且不弱协议回退 | 技术回滚/真实 listener 可见性/兼容验收待证 |
| H-RFB | 标准 RFB 输入与授权 | **HUMAN_APPROVED_A**：保留 Host 已开放的基础输入能力；新架构默认只读，Scope 经 Host 栅栏与单控制者仲裁，共享口令不充当个人身份 | 旧客户端兼容/安全展示技术验收待证 |
| H-REVOKE | 在途数据撤权边界 | **HUMAN_APPROVED_A**：新 epoch 生效后不新生成/排队旧许可输出；清本地可控缓存，必要时断链，View 遮旧帧；不承诺追回已传/已见内容 | 线程线性化、内核缓冲、负例审核待证 |
| H-EDGE | 工厂后台首批范围 | **HUMAN_APPROVED_A**：仅支持登录期间 Edge 在线，退出/注销真实离线；无人值守需未来单独资格且仍同一个 View 安装 | 首批产品范围已定；Windows Service 不构成当前支持承诺 |
| H-BASELINE | V15/V8 送审稿等效完整正文与 Native 附录 A/B 已取得 | 送审稿实际 SHA 与 #827 历史记录匹配，Human 确认与已批准正式版正文相同；56/99/21 来源核对已归档，原 V8 的 WIRE-V01..V06 数值和 SOURCE_TENSION_01=A 已获 Human P-A/A 批准；WIRE-V07 真实互通及负向测试仍未运行 | Gate-D 仍未批准；不假称正式版 DOCX 二进制 SHA 已取回 |

**D4 当前结论**：Human 的 H-T01/H-DUAL/H-RFB/H-REVOKE/H-EDGE=A 均保持批准。Human 提供了与 #827 送审稿归档 SHA 一致且正文等效的 V15/V8；56/99/21 来源索引与 V15 表6 31 场景已经形成受控审计。新补的 XP-21..XP-28 是此前 XP01..20 未充分覆盖的用户验收设计项，全部 `NOT_RUN`；Native P-A 数值和 TLS-first 安全档位 A 已获 Human 批准，但 Host/Viewer 的互操作、负向向量与安全实现资格仍未运行。上述不代替 Gate-D 冻结、产品代码 Gate-I、测试资格或发布。正式 Word 仍先目录/内容/图表/版式和源条款审签后才制作。
