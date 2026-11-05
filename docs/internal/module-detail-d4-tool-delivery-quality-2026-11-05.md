# D4｜HyRemoteTool、安装交付与全产品安全/质量合同（候选）

> **任务**：[全产品设计总控 #843](https://github.com/skawu/HyRemote/issues/843)，范围 T-01、X-01、X-02；对齐 D1 Framework、D2 HyRemoteView、D3 Edge/Gateway 设计候选。  
> **正式基线**：Human 批准 `HYR-PD-001/V15`、`HYR-ARCH-001/V8`，但尚未取得可逐条回读的完整 DOCX/Native A/B。现有 `hyremote-tool-workbench-prd-2026-10-22.md` 的 **D 综合工作台与 S1 先交付**曾经另获 Product Owner 认可；不能静默把其已获批/已交付远程访问删除。  
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
- 两套描述对“未来谁负责官方远程会话 UI、是否继续投资维护 Tool 的独立 RFB 客户端”存在**实质产品路线重叠**，不能仅凭“架构更优”直接覆盖旧审批或撤销客户权益。

### 2.2 推荐的低风险候选 A（Human 审批前不生效）

**保留既有 Tool S1 的真实客户可用访问功能及维护责任**；将未来的高阶完整 View/Native/Managed/身份/共享界面作为 **HyRemoteView** 的单一产品演进主线；Tool 新增开发主线集中在本机 SDK 安装检查、四接入指导、真实/脱敏诊断及工单证据，不在 Tool 重新造一套 Native/Edge/企业工作区。对 RFB 客户端核心的复用，只在许可证、单一所有权、二进制 ABI 与依赖独立性通过设计评审后进行，不能借临时共享源码让两产品版本互相锁定。

### 2.3 Human 必须对比的替代方案

| 选项 | 功能归属与迁移 | 风险/成本 | 建议 |
| --- | --- | --- | --- |
| **A：存量保留，新增主线分工** | Tool 已交付 Remote S1 继续维护；新高级访问/Native/Edge 进入 View；Tool 聚焦 SDK/诊断 | 一段时间双 RFB UI/安全补丁路径并存；须治理共用 parser 或替代计划 | **推荐**：不追溯剥夺现有功能，避免未来无限双轨 |
| B：Tool 逐步转纯 SDK/诊断 | View 接管所有 Remote UI，Tool 提供迁移/入口与兼容窗口，约定停止支持日期 | 变更已批准 D/S1 和用户安装/工作流，版本与迁移成本高 | 需要 Human 明确退役和已交付权益过渡 |
| C：Tool 和 View 永久各自全面 Remote | Tool 与 View 都持续实现 RFB/Native/权限/更新 | 双安全面、双测试矩阵、产品定位重叠且长期成本最高 | 不建议，除非明确不同客户群与商业理由 |

**禁止默认决定**：不因为选 A 就把旧 Tool 发布为“已经支持 Native TLS”，也不因为设计 V15 就从工具已发布安装包中删除有效连接入口。无 Human 决策时 D4 仅记录候选并继续不依赖它的交付技术审查。

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
| XP-12 | GUI 退出、用户注销、SCM 可用/不可用、升级回退时站点在线状态准确 | V03/E01/X01 |
| XP-13 | 工厂 Edge 复制/重放 ticket 与旧 SitePolicyRevision 无权路由 | E01/G01 |
| XP-14 | Tool 选择真实 SDK/Qt Kit 生成四路线准确步骤和脱敏证据，无伪远程诊断 | T01 |
| XP-15 | Tool 旧 S1 兼容与 View 新官方访问产品各自独立安装/升级且不倒退 | T01/V01/X01，**Human 产品权责决策未定** |
| XP-16 | OEM 多品牌不绕 TLS/设备许可，不改变 Native wire、不串持久资料 | V03/T01/X01 |
| XP-17 | View/HMI/Tool/Edge/Gateway 逐进程 1080p CPU/帧龄/RSS、慢端和队列均可复现 | X02 |
| XP-18 | 自托管 Gateway 证书轮换、备份/迁移、升级/回滚不会复活已撤 Route/共享 | G01/X01 |
| XP-19 | 无公网的工厂可独立使用 RFB/Native Direct，未安装 Gateway 时 UI 不报错 | V01/X01 |
| XP-20 | 权限/凭据/界面功能尚未实现时统一显示不支持/未知，禁止假按钮和假资格 | 全域 |

上述是**设计验收合同**而非目前的测试结论。完整 V15 原文条款追踪仍要逐条建立，不能用 20 个场景直接宣称覆盖全部产品要求。

## 6. 需集中 Human 审核的实质决策与 D5 输入

| ID | 问题 | 当前技术倾向 | 触发审批 |
| --- | --- | --- | --- |
| H-T01 | Tool 长期仍为“远程综合工作台”还是改向 SDK/诊断？ | 推荐 A：已交付 S1 维护；新完整官方 Viewer/Native/Edge 归 View；不把旧功能直接删除 | **产品路线与已批准历史冲突，必须 Human** |
| H-DUAL | HMI Dual 预期协议启动是整体原子失败还是明确局部退化？ | 推荐 requiredProfiles 原子 Admission；运行中单协议故障只影响授权的对应 Session | 若影响客户可用性/安全承诺 |
| H-RFB | 标准 RFB 匿名/共享密码连接能承诺怎样的细粒度 Grant/Scope？ | 推荐标准 Direct 的显式受限 Host 策略、默认只读；高保证身份/动态 Target 使用 Native | 任何削减已发布控制功能或更改安全承诺需 Human |
| H-REVOKE | 撤权后已进入 TLS/socket/内核的 in-flight bytes 安全承诺 | 推荐权威新 epoch 之后不再产生新的旧权限输出；清设备侧可清队列、关闭必要连接，View 遮旧帧，不声称物理撤回已传内容 | 若 V8 安全文字要求更强绝对保证，Human 审批 |
| H-EDGE | 是否将“注销后仍有 Edge 在线”作为首批必须成果？ | 首批只承诺已登录可用；无人值守独立实机资格后交付 | 产品级别/版本范围变化由 Human |
| H-BASELINE | V15/V8 正式 Word 源文件及 Native 附录 A/B 核对 | 审批冻结前核对真实授权原文，按 SHA 归档索引；发现矛盾提出旧/新合同、影响和回退 | 真实文件输入/正式冻结需要 Human |

**D4 结论：** 可以继续 D5 规划依赖和技术工作包，但以上真实决策不能由“文档合并成功”替代审批。下一步为**全产品统一实施 WBS/DAG + 设计完整性/冲突清单 + Human 决策卡**。正式 Word 编制须在完整方案/目录/内容/图表/版式先行规划和 Human 同意后独立展开，避免反复排版。
