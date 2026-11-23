# HyRemote V1.7｜版本路线 WBS 与交付验收（候选）

> 对应 [受控版本路线图](hyremote-v17-implementation-version-roadmap-2026-11-23.md) 和 [统一三项设计收口](hyremote-v17-three-closures-design-2026-11-23.md)。本 WBS 是**设计/执行权责矩阵**，不是已派发任务、GitHub Issue 自动创建记录或产品源码实施许可。Human D-PROC=B、D-REVOKE=A 已批准；Gate-D 尚未冻结、View G3 未通过、Gate-I 未授权。2026-11-23。

## 一、执行规则：防止 AI 无限制增加工作和偏离目标

1. **条件依赖防止错误全局阻塞**：表中 `A,B` 是同一声明路径的 AND；`A 或 B` 和 `Public=X；View=Y` 是条件 OR，绝不把一张跨仓验收台账写成必须等全部产品完成。View G3 只制约适用 View 路径，不制约已有 Public SDK 的维护或已批准的 Public 专项。独立产品各自 Gate-R 单独核验。
2. **固定 WBS 边界**：只使用当前 36 项已识别用户成果和必要支持/验证工作，不自动拆成无限子 Issue，不因“持续推进”新增新架构/通道/云控制层。正式施工时每一条任务只允许一个实现 Owner、一个代码仓和可复现证据出口；跨仓并行用依赖 Issue/PR 合同协作。
2. **实际已授权的 V0.3.5.0 与维护车道独立进行**；P-EXT 与 View Native 新增代码只在 Gate-D/所需 G3/正式 Gate-I 后实施。View G3 PoC 本身有独立明确授权规则，不把先前 ADS 治理恢复。**当前 View #5 仍要求 A–H 全套 PoC 才整体 PASS：A03R/A03N 只是本 WBS 的合理条件分流候选；在 #5 Owner 正式修订单路径准入规则前，不能用单独 A03R PASS 冒称 View G3 或 View Gate-I 已通过。**
3. **每个代码任务至多一个清晰产品变化/接口**，避免把构建、协议、GUI、性能和发布汇成巨 PR；必要的独立审查/CI/安全用例均在同任务验收下进行，而不是新造统一质量服务。
4. **禁止错误状态提升**：`PLANNED → DESIGN_REVIEWED → IMPLEMENTATION_AUTHORIZED → SOURCE_COMPLETE → REVIEWED/TESTED → PRODUCT_QUALIFIED → RELEASED`；实际应以 owning issue、同 SHA CI 和运行/发行证据独立标注。任何状态不确定标 `NOT_RUN/UNKNOWN`。
5. **交互式 Human 集中点**只在：Gate-D 正式审签、Public P-EXT 正式 Feature 版次分配／View 自有编号、Gate-I 工作包激活、真机/账号不可替代的必要操作、正式发行 Gate-R。其余 Agent/Owner 按授权范围自行执行，不让 Human 充当日常验收操作员。
6. 已发生的 #565/#567/#362 现场问题、V0.3.5 性能/水印 P2 按原有 Issue/维护车道做，不因为本 WBS 又开一个同样的重构专题；优先真实用户结果。
7. **无工期虚估**：在 G3 QtQuick/DeviceTLS 与具体 Qt Kit/Profile 尚无实证前，不伪造“几日完成/某天发布”，用前置完成条件与用户结果定义阶段。

## 二、可执行 WBS（36 项、五个责任域；按产品声明选择条件前置）

### 设计批准与既有产品维护

| WBS | 唯一 Owner（类型/仓库） | 前置 | 实际交付结果 | 最小验收/可证伪失败 | 版本/CT | 当前状态 |
|---|---|---|---|---|---|---|
| A01 | 公共架构/QA | — | D1–D5、V1.7 受控差异和 V15/V8 源追踪审查；逐条标记当期/延期/可选 | 记录 source→CT→状态/错误/测试/Owner；旧 Native Public Owner 全部纠偏；提交 Gate-D 合同 | Gate-D | 候选待审 |
| A02 | Human/独立审查 | A01 | 集中审查重大设计冲突与当前产品支持范围；正式决定 Gate-D 或带条件批准 | 书面限定已批准/未批准范围；任何关键 OPEN 不得假装 FROZEN | Gate-D | 未批准 |
| A03R | View G3 Owner（RFB/Quick） | A01 | 仅 View RFB/Qt Quick Direct 的有界技术验证；RFB parser / Qt Quick 渲染/真实 Viewer | RFB 错误包/Direct 画面输入、Qt Kit、FPS/CPU/RSS/安全/许可证真实证据；不等待 Native G3 | View RFB G3 | NOT_PASSED；#5/#6/#9；**拆分放行尚待#5 Owner正式修订规则** |
| A03N | View G3 Owner（Native） | A01 | Native DeviceTLS/受信 HMI 同进程模块/Grant/最终撤权，**并覆盖 Native-only View 所必需的协议中立 Qt Quick 共享显示能力** | 真 DeviceTLS/HostPort/Qt Kit/21-type/安全/线程/资源；Native-only 共享 Qt Quick 用真实 FrameSnapshot/RenderSurface/尺寸失效/CPU、GPU、RSS 和资源负例（不要求 RFB parser）；明确 GO/NO-GO，不阻断 RFB Direct | View Native G3 | NOT_PASSED；#5/#14/#30；**拆分放行尚待#5 Owner正式修订规则** |
| A04 | Human/产品 Owner | A02 | 按 Public Extension、View RFB、Native、Managed 分路径确认 Gate-I 实施权限 | 每条获准工作包列仓库、范围、基线、审核 Owner；未获批准路径不实施；各路径适用的 G3 证据在该路径 Gate-I 中独立核验；Public Extension 不等待 View G3 | Gate-I | NOT_AUTHORIZED |
| A05 | HyRemote Owner #594 | A01 | 为 Public P-EXT 选择单独可交付 Feature 目标/既有安排影响；与 View 候选版号独立审定 | 旧→新编号与原因、Tool/Qt/0.3.5影响、评审 PR；Machine train 仅激活时修改 | 版本决策 | 待批准 |
| H01 | HyRemote #95 / #783 | — | V0.3.5.0 原已批准范围收口：路径/文字水印、P2 fail-closed、RealVNC A/B runner 观测可信、CPU/画面质量和真实 Viewer | #1048 仅修 sample 被遮挡假阴性，motion 1V 仍须真实裁定；最终候选同 SHA L2/Review/Viewer；目前 HOLD | V0.3.5 | ACTIVE/HOLD |
| H02 | HyRemoteTool #579/#586 | — | 依既有 V0.3.6.0 规划实现 Tool S1 /设备档案/缩放/语言/安全键鼠/基础诊断（只在原 train 准入后） | Win 实机 800×600、真实维护 Viewer、断线/held、状态真实、独立 Tool 安装；不承接 Native；Tool 可与 V0.3.5 收口隔离并行；正式发行遵循 Public 既有 lineage | Public 0.3.6 planned | PLANNED |
| H03 | HyRemote #592 | — | 依 V0.3.7.0 条件 Docker 实际 Qt App + SDK + config/state/secret | 实际容器真实 Viewer/非 root/restart/recreate/权限，缺环境保持 NOT_RUN 且不阻 RC；Docker 可在 #591 配置合同可用后独立设计/实测，正式发行仍受既定版本 lineage 约束 | Public 0.3.7 conditional | PLANNED |
| H04 | HyRemote #235/#95 | H01 | 按现有 V0.4 RC → V1.0 GA 不开发新功能，只资格化 Windows Qt 5.15/6.8 已承诺单元 | 在 Gate 满足情况下冻结真实支持矩阵、API/Kit/安装/安全/Viewer/性能/发布；0.4 active:false 不抢跑 | Public RC/GA | CONDITIONAL |

### Public Extension & Qt Host

| WBS | 唯一 Owner（类型/仓库） | 前置 | 实际交付结果 | 最小验收/可证伪失败 | 版本/CT | 当前状态 |
|---|---|---|---|---|---|---|
| P01 | HyRemote Public SDK | A04,A05 | NativeHostPort 单一 C ABI header/API table；size/major/minor、调用约定、handle、错误类、线程/取消/持有权 | 独立源码和 CMake consumer 可用；无 Private headers/QObject/QtPrivate/STL 暴露到 ABI；未知 major 拒绝 | P-EXT, CT-03 | 设计完成、未编码 |
| P02 | HyRemote Public SDK | P01 | OptionalModuleSlot 受信声明加载/静态整机等价入口、部署 Profile 和 Admission 准入 | 无 Private 时 RFB-only build/install/run 不查找私有模块；错误 Kit/摘要/证书 fail-closed；Dual 必需端失败应用级原子拒绝 | P-EXT, CT-02/11 | 设计完成、未编码 |
| P03 | HyRemote Runtime | P01 | 唯一 Session/Target/Scope/Grant/PolicyEpoch 和现场优先 ControlArbiter；HostPort 仅消费身份事实 | RFB/Native 同 Scope 不串权；View 被撤时 Control 级联，Control-only 不误停 View；未知 Owner 拒绝 | P-EXT, CT-01/04 | 设计完成、未编码 |
| P04 | HyRemote FramePipeline | P02,P03 | Qt Raster/Widgets/Quick/OpenGL/native/video 源登记与 Scene 生命周期，TargetFrame 完整合成；不新增通用截图引擎 | Completed/Unchanged/Unavailable 区分；混合敏感覆盖未知拒帧；无旧 FrameId 假新画面；HMI GUI 不被同步读回长期占用 | P-EXT, CT-05 | 设计完成、未编码 |
| P05 | HyRemote FramePipeline | P03,P04 | OutputGuard 授权派生 Mask/Watermark、FrameLease Generation/credit、慢 peer 限额与共用底帧 | 128MiB/96MiB/每 Native peer pending 512KiB 预准入；Qt TLS 明文/密文真实计入；错 Grant 数据不出帧 | P-EXT, CT-08 | 设计完成、未编码 |
| P06 | HyRemote Runtime + RFB Owner | P03,P05 | Host T0、RFB 受影响 Peer 最后出队栅栏、QtFinalGate 与来源 held 输入结算 | T0 后 final check 不发送旧帧，T1 由真正 final writer 证实；T2 不冒充业务结果；旧 RFB 控制与现场安全无退化 | P-EXT, CT-06/09 | 设计完成、未编码 |
| P07 | HyRemote QA | P02,P06 | 无私有仓库、证书、license 情况下 Public RFB 标准第三方客户端完整访问 | 四 Qt 入口适用 Kit/Installed SDK consumer/Raw/TRLE/本地 UI/安全错包；不要求 View CI/Version/G3 PASS | P-EXT | NOT_RUN |

### HyRemoteView RFB / Native Direct

| WBS | 唯一 Owner（类型/仓库） | 前置 | 实际交付结果 | 最小验收/可证伪失败 | 版本/CT | 当前状态 |
|---|---|---|---|---|---|---|
| V01 | HyRemoteView Client | A03R,A04 | 有限状态/长度 RFB3.8 parser、Raw/TRLE/尺寸变化/必要输入释放与旧主机兼容 | 有界流量/恶意包/断线/重连/合法 old RFB Host 与真实 Viewer 正反例；View RFB 正式实现须其自身 G3/安全资格；不以 Public 已发布 RFB 代替 | View 0.1 proposed, CT-07/10 | 未编码 |
| V02 | HyRemoteView Surface | A04 + 条件显示资格：RFB=A03R；Native-only=A03N中的共享 Qt Quick 证据（须先获 View #5 范围化 Gate 正式批准，否则执行 A–H 全量） | 协议中立 immutable FrameSnapshot、Qt Quick RenderSurface、DisplayEpoch、DPR/座标逆变换 | 真帧/旧帧/失权遮帧与焦点输入独立；适用 Windows/Linux 真 Kit 性能、GPU/CPU/RSS 出口；RFB 与 Native 的共享 Qt Quick 渲染证据可以复用，但绝不要求 Native-only 通过 RFB parser/G3，也不以 Mock PASS | View 0.1 proposed, CT-10 | 未编码 |
| V03 | HyRemoteView Identity | A04 | DeviceProfile/DeviceCatalog、OS SecretStore、证书变化与最小 DeviceTrust（第一切片） | 同 IP 不同设备拒旧 trust；明文存储不可用时安全失败；RFB 无自然人身份显示真实 | View 0.1 proposed, CT-10/14 | 未编码 |
| V04 | HyRemoteView E2E QA | V01,V02,V03 | 独立 View RFB Direct 预览候选/用户工作流 | 精确 Public release SHA × View SHA，键鼠/帧/Resize/丢网/旧版本/隐私/软件包，合格才可发布 0.1 | View 0.1 proposed | NOT_RUN |
| N01 | HyRemoteView Private Host | P01,P02,A03N,A04 | Private HMI 同 Qt OS 进程 Native 模块构建/装配、签名/运行时/Qt Kit compatibility、生命周期 | Public 不含 Native parser/TLS，Static Qt 仅可受控整机编译；停用后回调/Lease/Qt QObject 归零再允许 unload；Private Native HMI 正式实现须 Native G3 适用证据；不因 Public 延后而私自复制 HostPort | View 0.2 proposed, CT-02/03/14 | 未编码 |
| N02 | HyRemoteView Private Host | N01,P03 | Native 21 type 完整协议 parser/Encoder、TLS-first 双档、DeviceTLS 私钥/Peer 身份/错误语义 | 26 正向 byte vectors、12 负向规格、消息长度/时序/密文错误、授权前不泄 Target；plain 绝非明文 | View 0.2 proposed, CT-07 | 未编码 |
| N03 | HyRemoteView Private Host | N02,P05,P06 | Native per-Peer codec/Full-Delta 基线、最终 EgressActor、T1 fence 与 Qt TLS pending 预算 | 末端 final check 与 write 不再异步跳域；旧 TLS 无法清理时合法网络线程 abort，T1/T2 真实结算；资源严格限额 | View 0.2 proposed, CT-06/08 | 未编码 |
| N04 | HyRemoteView Private Host | N02,P06 | Native 五按钮/17 keypad/中文 committed text/input Grant 进入同一 QtFinalGate | 跨 RFB 同 Scope 单控制者/held 来源隔离；网络接纳不冒充 Qt/业务已执行 | View 0.2 proposed, CT-09 | 未编码 |
| V05 | HyRemoteView Native Client | N02,V02,V03 | 真正 Native TLS Trust/User/Target/ViewGrant/ControlGrant/帧和输入解析，不以 RFB 代实现 | 拒假 HMI/错误证书/旧 Grant/错报文/迟到回调；有真实 ControlGrant 才允许输入 | View 0.2 proposed, CT-07/10 | 未编码 |
| V06 | 跨仓 QA | N03,N04,V05,P07 | 真实 Host↔View Native Direct 双 SHA/ABI/Kit/权限/安全/性能闭环 | DeviceTLS/HostGrant/Frame/输入/T0-T1-T2 两端 + Qt GUI 真实业务；两协议同时启用/单独启用；负例、回滚 | View 0.2 proposed | NOT_RUN |

### Workspace、Factory Edge 与 Gateway

| WBS | 唯一 Owner（类型/仓库） | 前置 | 实际交付结果 | 最小验收/可证伪失败 | 版本/CT | 当前状态 |
|---|---|---|---|---|---|---|
| W01 | HyRemoteView Workspace | V04 或 V06（按所声明协议能力；Native 增强还须 V06） | 协议中立 DeviceWorkspace/多 Target/多窗口焦点/旧画面与错误事实呈现 | 客户可识别设备/权限/画面时效；跨设备操作不误投递、证书轮换及离线 Direct 不需云；工作区共享能力可先消费已合格 RFB/Native 任一分支，不形成双协议强制前置 | View 0.3 proposed | 未编码 |
| W02 | HyRemoteView Workspace | W01 | 诊断来源等级、最小 OEM branding/权限显示/支持资料出口 | 不把 Gateway 登录称设备授权，不承诺没有 Host 业务回执的成功；密钥及报告脱敏 | View 0.3 proposed | 未编码 |
| E01 | HyRemoteView Edge | W01,A04 | 同一 View 安装默认 OFF 的登录态站点共享/EndpointBinding、RunLease/RunEpoch/可信策略持久化 | 显式管理员与设备白名单，View 退出/OS 注销/崩溃撤 Route；升级/恢复不复活未许可旧共享 | View 0.4 conditional, CT-12 | 未编码 |
| E02 | Gateway 独立 Owner #190 | E01,V06,A04 | 受独立 #190 激活后 Stage05 唯一控制 IDL/RoutePermit/PairLedger/opaque FIFO | 租户与 HMI 白名单/一次配对/网络双向保序/期限和操作审计；不解 DeviceTLS，不发 Host Grant | Managed 条件版本 | 未激活 |
| E03 | HyRemoteView + Gateway QA | E02,V06 | 经 Gateway/Edge 到 HMI DeviceTLS/Grant 的安全端到端完整会话 | 对抗路由/证书/Pair 重放、Relay断链/站点撤权/恢复；无任意内网代理、无弱 RFB 公网回退 | Managed 条件版本 | NOT_RUN |

### 跨仓资格与独立发行

| WBS | 唯一 Owner（类型/仓库） | 前置 | 实际交付结果 | 最小验收/可证伪失败 | 版本/CT | 当前状态 |
|---|---|---|---|---|---|---|
| Q01 | 跨仓 Release QA | 条件前置：Public=P07；View RFB=V04；View Native=V06 | Source SHA 与 View gitlink/lock、HostPort ABI、Native Wire、QtKit/OS 的配对支持矩阵 | 不把 gitlink 固定视为互通 PASS，变更仅在对应产品支持矩阵中声明已通过单元；每个已声明产品/协议单独固定兼容矩阵，不能因 View Native 未完成阻断 Public 或 View RFB | 各 View Release | NOT_RUN |
| Q02 | 跨仓 QA / 用户环境仅必要时 | Q01 | Windows x86_64 Qt5/6.8 真实本地 HMI+Viewer；Linux/ARM64/EGLFS/Wayland 依适用产品档位另证 | 0/1/2 Viewer 资源/帧龄/现场输入与真实采集后端；用户现场环境未到不要求重复人工验证、不宣称 Supported | 各平台声明 | NOT_RUN |
| Q03 | 跨仓 QA | Q01,Q02（分别仅取本次产品/平台声明的实例） | Public SDK 独立安装；View 单包 Edge；Private HMI 模块签名、升级、回滚、失权高水位 | P1/P2、恶意包、TLS 失败、错误 Qt Kit、实际重启/旧授权不复活；没有真实证据不给出 release PASS；安装验收按此产品的真实构件进行，不能要求所有组件（含 Gateway）预先实现 | 各产品 Gate-R | NOT_RUN |
| Q04 | 各仓产品 Owner / Human 发布授权 | Q03（仅本次产品对应实例） | Public/View/Tool/Gateway 分别接受适用版本/客户结果和独立发行资格 | Human 只审批重大正式节点；不强制同号发布，不用 View 未完阻断 Public；必须是被批准 train/tag/SHA；每个产品独立发行，不能以其他产品的未完成项阻断 | Gate-R | NOT_AUTHORIZED |

## 三、里程碑与唯一关键路径

| 里程碑 | 严格的完成定义 | 不可冒充通过的结果 |
|---|---|---|
| **M-D：设计准入** | A01,A02，V1.7 与源文关系受控，适用 CT Owner/失败/接口契约完成，Human Gate-D 通过或带条件批准 | 只修改 Markdown / 56与99行有映射 |
| **M-G3：技术风险已取得真实证据（分支资格仍待 View #5 权威确认）** | A03R/A03N：A03R、A03N **按各自产品路径**取得 QtQuick 或 Native TLS/Grant、适用 Kit/License 的精确测量/负例；不合格给出 NO-GO | Qt 文档可行性、Mock 单元或 Public 已发布 Viewer |
| **M-I：按仓/路径实施授权** | A04 对选定代码范围有效；Public P-EXT 另外要求 A05 公共版本归属签收，机器/版本权威未越权 | Human 只批准架构方向、Draft PR 已有编译代码 |
| **M-R：Public 可选 HostPort 合格** | P01~P07，Public RFB-only 不加载 Private/独立构建/真实第三方 Viewer E2E，版本由 #594 单独批准 | 私有整机套包能跑，但 Public 不能单独安装 |
| **M-V01：View RFB Direct 用户可用** | V01~V04，工作区最小入口和断线/输入/显示及真实安装材料已验收 | 测试 Parser 返回帧但 UI 不正确或在线状态虚假 |
| **M-V02：Native Direct 端到端可信** | N01~N04 + V05/V06 + Public HostPort；两端真实 TLS/21-type/Target/Grant/Frame/Qt Input/撤权/2 Peer | 单端 PoC、相同 mock parser、双 SHA 只锁未测 |
| **M-V03：工业设备工作区** | W01~W02，真多目标/多设备/信任与诊断无权限/身份混淆 | 单桌面 Demo 或仅堆 UI |
| **M-V04：工厂 Edge 阶段** | E01 满足默认 OFF/登录/注销/授权；正式对外客户场景需要对应真实环境资格 | 不经 Gateway 却宣称跨公网 Managed 已可用 |
| **M-M：Managed（独立可选）** | E02/E03，#190 授权、唯一 IDL、Route Permit/PairLedger/真正 DeviceTLS/Grant 端到端 | Gateway online 或 Edge 上报在线 |
| **M-RL：每产品独立发行** | Q01~Q04 按实际声明分支；Public/View/Tool/Gateway 分别采用各自受控 release/tag | 一项 PASS 自动带动其它产品发布 |

**最短有价值路径**：保留已发布 Public RFB → A03R 有界 RFB/QtQuick G3 → View RFB Direct（V01–V04）；与此并行（均待 Gate-I）实现 Public P-EXT（P01–P07）与 Private Native HMI+View Native（N01–N04,V05–V06）。Native 不强制拖慢独立 Public RFB/Tool；Workspace 和 Managed 绝不抢占前两个 Direct 最终安全资格。若 G3 或 Gate-D 发现当前 Native 方案物理不合格，N 路径停止并提交**明确问题/有界备选/是否重开重大 Human 决策**，不是自行转为新独立进程或造新协议。

## 四、跨仓交接点与版本身份

| 提供方 → 消费方 | 唯一交付物与不可变边界 | 实际适用版本/可追溯条件 |
|---|---|---|
| HyRemote Public → HyRemoteView RFB | 标准 RFB3.8、现有 Host 的真实 framebuffer/输入安全/Viewer compat | View 固定 Public 已发布 SDK/Runtime SHA；View 0.1 可先于 P-EXT |
| HyRemote Public P-EXT → View Private HMI | NativeHostPort ABI major/minor/HostEpoch、Grant/FrameLease/T0、Qt Kit exact Profile | View HMI 模块只装/链受控 Public；没有 Private 时 Public standalone 可用 |
| View Private HMI → View Native Client | V8 21 type、Native P-A/A、DeviceTLS、Target/Grant/Frame/Input、错误状态 | 双仓各自 SHA、Private HMI module build hash、View hash 与设备真实 Kit/P95/RSS |
| View Workspace → Factory Edge | OS SecretStore、站点白名单及登陆期 RunLease、Edge UI/共享许可 | 工厂同一个 View 安装包；不生成另一个独立 Edge 软件 |
| Factory Edge → Optional Gateway → Native HMI | RoutePermit、Pair once、opaque FIFO、DeviceTLS E2E 仍终止在真实 HMI | #190 独立激活后 Managed 组合实际取得安全 Gate；Direct 完全可脱离 Gateway |
| Public/Private/Tool → QA/Gate-R | SHA/Kit/平台/协议/权限/安全/性能/升级/真实 Viewer 证据 | 各产品版本独立 Gate；**未证实=NOT_RUN** |

## 五、风险和执行收敛线

- 最优先制约 **V0.3.5 HOLD** 并不是 P-EXT：#1048 Runner 采样遮挡确实造成伪阴性，但 Motion CPU 没有改善；Release 在 #95 正式签收以前不能 Promote/Tag。
- 同进程 Native 的故障/内存破坏可影响 HMI 现场业务，不能通过 C ABI、线程或代码 Review 许诺 OS 崩溃隔离；确认可信集成软件和严格现场故障优先级。
- Qt Quick/GPU/native 画面只有真实来源/合成/渲染同步与隐私正确性成立，才能声明相应 high FPS Profile；无证据时降级为受限可用或 fail-closed，绝不冒称 Qt 全 backend 一致。
- 多 Peer/4K 隐私派生可能超过 128MiB/96MiB/512KiB 控制预算；实际 Host QoS 先降远程并发/速率、限制复制，现场 Qt UI 优先。
- Native/Managed 的未来发布不构成用户现场 ARM64 阶段重复操作指令；待 Human 提供已协调环境后按最短设备反馈闭环测一次，不让 Human 成为持续测试工人。
- 已批准 H-O01/H-O02 当前分期只需要 `UNSUPPORTED/UNKNOWN/DEVICE_PENDING` 正确性，本期不实现真实文件传输/业务回执/离线可靠接收扩展，不为凑“全功能”擅改 Native wire。
- 当前 **WBS=DESIGNED_FOR_REVIEW**，并无 Agent/Local Owner 任务 ACK、已提交产品代码、已执行 Gate-G3、已通过 Gate-I 的新事实。用户未来批准特定路径后，Owner 只从本 WBS 派发有界任务而不是重新设计系统。
