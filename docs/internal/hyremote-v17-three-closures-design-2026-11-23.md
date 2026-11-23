# HyRemote V1.7｜完整产品设计三项收口（唯一候选）

> **性质：仅设计，供集中专业审查；并非已批准正式 V15/V8/ADR 原文修订，也不是 Gate-D 冻结或 Gate-I 授权。**
> **权威决定：Human D-PROC=B（Native HMI 与 Qt 应用同进程、Private 源码），D-REVOKE=A（T0/T1/T2 分阶段）；H-T01/H-DUAL/H-REVOKE/H-EDGE/H-RFB=A、H-O01/H-O02=A 与 Native P-A/A TLS-first 保持。**
> **适用目标：一套核心 Host / 四种 Qt 接入 / 公共 RFB 独立 / 两仓独立版本 / Native Host+Client Private / 一个工厂 View 内置可选 Edge / Gateway 可选。**
> 2026-11-23。此稿只整合并具体化既有 V1.7 与 D1-D5；不建立 V1.8 或第三套协议/服务。

## 1. 收口一：唯一产品架构、物理部署与源码责任

### 1.1 物理事实

```text
工控 HMI 原 Qt 业务进程（一个 OS 进程）
  ├─ HyRemote Public：Core + Shared HostRuntime + Target/Session/Policy
  │                    Qt Surface/FrameGraph/OutputGuard + Standard RFB3.8
  │                    NativeHostPort（仅可选调用端点，不含私有协议）
  └─ HyRemoteView Private 可选 HMI 模块：
                       Native TLS/Parser/Codec/PeerEgressActor
                       消费同进程 Host 已授权 FrameLease
不同 PC 的 HyRemoteView：官方 RFB + Native Client、Workspace
工厂 PC 同一个 HyRemoteView 安装：默认关闭的登录期 Edge 角色
可选 Gateway：受限站点/设备路由及 opaque TLS ByteStream
```

**撤销旧前提**：不再存在独立 Native Provider 进程、本机 named pipe/Unix socket、跨进程共享帧/BridgeClock、进程级 Native Crash 隔离、OS kill-provider 实现 T1、Public F14 Native Listener。旧方案保留为历史比较而非现行设计。Private 同进程模块是 HMI 自身可信代码；其任意内存破坏可能影响 HMI 业务和公共 RFB，也无法 OS 级阻止读取同进程原始像素。线程分离和 C ABI 均不是沙箱。**不能承诺“Native 崩溃不影响 HMI”**。

**仓库不变式**：Public 无 Private 头文件、实现、许可证、安装/云依赖可独立产出标准 RFB；Private 可依赖 `components/HyRemote` 固定 Git SHA 实现 Native HMI 模块和 View Client，但不能使 Public 仓库反向依赖 Private。禁止将“Private HMI 模块源归 View”误读成“Native 服务只运行在工厂 PC”；设备端 TLS 必须在真实 HMI 上。

### 1.2 14 个既有模块及唯一实现 Owner（不新增第15个模块）

| 原模块 | 唯一产品 Owner | 最终设计合同 | 不应跨越的边界 |
|---|---|---|---|
| F-01 Core | HyRemote Public | CT-01：Protocol/Platform-neutral Frame、Input、Session、credit 数据语义 | 不引 Qt Widgets/Quick、QPA Private、私有协议 |
| F-02 Shared Runtime | HyRemote Public | CT-04/05/06/08：唯一 Session、Policy、TargetFrame、OutputGuard | 不实现 Native Wire |
| F-03 四 Qt 接入 | HyRemote Public | CT-02/11：C++/QML/Generic/QPA peer-frontends | 不各建 Core/Capture |
| F-04 RFB | HyRemote Public | CT-07：完整标准 RFB 3.8 服务端 | 无 Private 运行依赖 |
| F-05 Native *规范和 Host 接点* | Public 仅保留非私有 HostPort、V8 合同的设备政策消费；**具体 Native HMI 归 Private F14** | CT-03/07：HostPort 与受控 21 type/P-A/A 规范关联 | 不再在 Public 实现 TLS/Parser/Codec/Native Egress |
| F-06 Host Policy/Input | HyRemote Public | CT-04/09：当前 Grant、现场优先、QtFinalGate | 不由 View/Gateway 签 Grant |
| V-01 View 协议客户端 | HyRemoteView Private | CT-07/10：官方 RFB/Native 两条独立 Direct | 不以 RFB 实现 Native |
| V-02 RemoteSurface | HyRemoteView Private | CT-05/10：DisplayEpoch、FrameSnapshot、焦点与输入映射 | 不握持 HMI QObject |
| V-03 Workspace/Trust | HyRemoteView Private | CT-10/14：OS SecretStore、DeviceTrust、会话事实/OEM | 不冒充设备业务回执 |
| E-01 工厂 Edge | 同一个 HyRemoteView 安装 | CT-12：显式站点共享、登录期 RunLease、透明转发 | 不是 HMI Native 模块 |
| G-01 Gateway | 可选独立服务（按产品激活） | CT-12：RoutePermit、Pair 一次性消费、有限 opaque relay | 不取得 DeviceGrant、不做代理整个厂区 |
| T-01 HyRemoteTool | HyRemote 独立 Tool 产品 | CT-13：已发布 Basic RFB + SDK/Kit 诊断演进 | 不与 View 重建完整 Native Client |
| X-01 交付安装 | 每个可交付产品各自 Owner | CT-02/14：签名/ABI/配置/升级/回退 | 不因一个产品升级恢复旧权 |
| X-02 质量/需求治理 | 产品级 QA/架构审查 Owner | CT-15：需求到状态/错误/反证规格追踪 | 不以设计检查宣称运行 PASS |

**WBS 兼容规则**：历史 `F14` 仅保持追溯 ID，现定位为 **HyRemoteView Private 的 Native HMI 模块实现**（非公共 Framework F14），并以 CT-03 Public HostPort 为依赖；不同时保留 Public Native TLS/Parser/Listener 的另一路实现。若历史 F-05 表述“Framework Native 协议实现”，其中 *设备端实际 Listener/TLS/Codec* 的 Owner 受本项覆盖；公开合同/授权协作仍在 Public。V-01 中 Native Client 仍在 Private。

### 1.3 条件依赖（替换旧 D5 的错误 DAG）

```text
Public F10 DeploymentAdmission + Policy/Session F11 + Frame F12
   ├─ Public F13 Standard RFB ──> RFB Direct / Tool 基础能力
   ├─ Public NativeHostPort(CT-03) ─┐
   │                               └─> Private F14 Native HMI(21 type/TLS)
   │                                         └─> Private V12 Native Client
   └─ Public F15 C++/QML/Generic/QPA (仅链接 Public)
Private V13 TrustStore基础 ───────────────┬─> V12
Private V11 protocol-neutral RemoteSurface ├─> Workspace
Private V10 StandardRfbClient ─────────────┘
Private E10/E11 工厂同 View Edge + 可选 G10/G11 Gateway
  └─> 仅 Managed 产品路径，必须以 Native DeviceTLS/HMI Grant 结束
X10/X11/X12 按本期所声明的 RFB / Native / Managed 逐路径条件准入
```

**禁止假依赖**：Public F15 不把 Private F14 作为必需编译前置；Native-only 产品不等待 RFB parser/V10；RFB-only 不等待 Native F14/V12；V12 依赖 V13 的最小 Trust 基础，而 V13 的工作区集成反向消费已可用协议，不能再制造 V12↔V13 环；Gateway #190 未激活不得阻塞合法 Direct；Tool/Gateway 都不是 HMI SDK 的强制依赖。

## 2. 收口二：开发不应再自行决定的原子技术合同

### CT-01｜Core/Runtime 只有一个设备事实 Owner
`TargetId/TargetGeneration` 由 Host TargetRegistry 创建；`SessionId/Epoch` 由 Host AccessSession 创建；`ViewGrant/ControlGrant/Scope/PolicyEpoch` 由 Host F-06 发布；`FrameId/ContentRevision/GeometryRevision/PrivacyRevision` 由 FramePipeline 创建。任何协议端或远端不得根据 IP/屏号/连接成功自行生成或恢复 Grant。RFB 和 Native 只共享设备事实、授权、基本帧、Qt 输入仲裁，**不共享解析器、报文、安全协商或编码基线**。

### CT-02｜同进程 Native 装配和可启动性
Public 以显式部署清单管理可选 Native 模块：`enabled, exact module path/identity, signature/digest policy, HostApi major/minor, host Qt Kit/OS/CPU/compiler/CRT/SSL profile, DeviceTlsCredentialRef, listener/target scope`。RFB_ONLY 绝不查找/装载 Private；Native-only/Dual 配置在模块不可信、ABI 不匹配、设备凭据缺失时拒启对应声明，而不是从网络改成明文或找未知备用插件。动态插件须有受信装载入口（可用受限 QLibrary 风格 C 工厂），Static Qt 经 HMI 厂商重新编译时可用相同 HostPort 语义静态集成；**无可信装载入口且不可改源码的静态 HMI 不承诺 Native 零改造**。签名证明来源不提供内存沙箱。模块更新默认维护窗口中重启 HMI，不能保证零停机热卸载。

### CT-03｜NativeHostPort ABI 与所有权（可指导实现；不擅自认定现成编译 API）
单个受控工厂入口返回版本化 Host/Native API table；`ABIHeader{size, major, minor, calling_convention}` 与不透明 `HostContext, ModuleHandle, SessionHandle, LeaseHandle`；接口传递定宽数据、含长度 buffer、明确错误 enum，不向模块边界暴露 `QObject*`、`QtPrivate`、`std::string/vector` 布局；模块内部使用 QtNetwork 仍须同宿主 exact Kit。最小语义：
```text
Module.Prepare(host_api, profile) -> Prepared / Failure(reason)
Module.StartAdmission(admission_epoch) -> Ready / Denied
Module.ApplyEgressFence(scope, revoke_id, policy_epoch) -> AcceptedRequest [非 T1]
Module.StopNewPeer() / Stop() / QueryHealth() / DestroyAfterQuiescence()

Host.OpenPeer(provenance, AuthMethod, peer_generation) -> SessionHandle / Denied
Host.Authorize(Session, Target/Scope, ViewOrControl) -> GrantEpoch / Denied
Host.AcquireAuthorizedFrame(Session, Target, PrivacyRevision, budget)
  -> ImmutableLease{storage, generation, length, frame_epoch, expiry, credit} / Denied
Host.ReleaseLease(LeaseHandle) -> Released [不等于撤权 T1]
Host.SubmitInputIntent(Session, Scope, ControlEpoch, GeometryEpoch, event)
  -> Accepted / Denied [不等于 Qt/业务已执行]
Host.CloseOrRevoke(Session/Grant, RevokeId) -> HostFenceT0 / Pending
```
`HostPort` **是受信 API，而非攻击隔离**。TLS 连接身份事实可由同进程受信 Private 模块向 Host 报送，但 `authenticated=true` 绝不自行签 Grant。每个进程内 handle 绑定 `HostEpoch/ModuleGeneration/SessionEpoch`；异步回调要有持有者计数且结束前可撤销，迟到回调按代际拒绝。统一预期错误为 `DENIED/UNSUPPORTED/STALE_EPOCH/WRONG_THREAD/RESOURCE_EXHAUSTED/NOT_READY/INTERNAL_FAILURE`，仅作为内部错误类，不新建 Native wire 错误码。调用约定、错误来源和结果由 **真实实现的接口版本**精确固定，禁止私有实现者随意扩展破坏 Public ABI。

### CT-04｜Host 授权 / 会话 / 控制
设备 Host 最终持有 Target、Scope 冲突图、PolicySnapshot、Session/ViewGrant/ControlGrant 与现场优先抢占；相同 Scope 默认远端单控制者。ViewGrant 撤销同 T0 级联依赖的 ControlGrant；仅撤 Control 不必终止合法查看；隐私变化只影响相关 Peer/Target。RFB legacy 共享口令不等于自然人身份；新部署默认只读，已明确启用的合法控制应经 Host 当前 ControlLease 限制，不能静默取消已批准传统权益。任何新输入在网络接纳及 Qt 最终执行点双重检查。

### CT-05｜Qt Surface → 合法 TargetFrame
唯一 CaptureCoordinator 合并多 Peer 同 Target 采集需求；`SurfaceManifest` 根据真实渲染机制声明必需来源、窗口/Popup 层级、Z/clip/alpha、DPR、几何、隐私覆盖及生存代际；普通单 Raster 可自动配置。各源只能在合法 GUI/render 阶段交付 `CompletedSurfaceFrame`。普通 QWidget Raster 可用合法绘制内容；QOpenGLWidget 必须从其真实 FBO/上下文取得帧（`grabFramebuffer` 高代价兼容）；Qt Quick 必须在真实 scenegraph 完成阶段或明确有资格的 `grabWindow()` 受限读取；`QQuickRenderControl` **不用于接管任意已有 onscreen Quick**；QQuickWidget 有独立离屏路径；native Video/QWindow 必须有实际 frame provider。Qt5.15 与 Qt6.8 不能直接共用 Qt6 QRhi/QVideoSink 类型。复杂 Surface 缺必需图层/敏感遮挡证明：拒发或完整安全遮蔽，绝不旧像素重标成新帧；区分 `NEW_VALID/UNCHANGED_VALID/UNAVAILABLE`，只有真实合法新内容提高 ContentRevision。Target Frame 基础图可共享，但授权派生 Mask/Watermark/Delta baseline **逐 Peer 单独管理**，原始帧不得交 Private 协议面。

### CT-06｜最后 TLS/RFB 发送与 T0/T1/T2 竞态
T0 是 Host 同一权威 PolicyFence 将受影响旧 Grant 置 `BLOCKING` 的线性化点；此后才开始 `AcquireAuthorizedFrame` 或 Qt `TryBeginExecution` 必须拒旧 epoch。每 Peer 的网络 QObject 所属执行域为唯一 `PeerEgressActor`，从 `EncodedChunk{Session,Grant,Privacy,Target,Lease,FrameId}` 到实际 Socket 写入前必须 **最终复核当前授权**，期间不得挂起/改线程/再入无代际队列。最终检查在 T0 后开始必须拒绝旧帧；检查在 T0 前开始、实际写与 T0 交错的旧工作属于 D-REVOKE=A 的 T0–T1 窗口。T1 以全部受影响 Actor 执行旧 epoch 持久拒写 Barrier、结束先前写调用并清其可控任务为依据；单独事件投递、队列空、`bytesWritten` 不是证据。TLS/Qt 缓冲存在不可按旧 Frame 精确撤回的明文/密文时，在网络对象所属线程 **abort** 该 Peer，不通过优雅发送旧敏感数据换连续性；已经送内核/网络/客户端的无法追回。T2 还需结算相关 Qt 同步执行、逐来源 held/Session/Lease；网络线程卡死/结果未知则保持 `REVOKE_UNCONFIRMED`，不杀 HMI 进程、不热卸载冒称安全。仅撤控制时允许 T1 输出范围为空且明确 NOT_APPLICABLE，但 T2 仍需输入结算。

### CT-07｜独立标准 RFB 与 Native1.0
Public RFB3.8 独立协商、`ServerInit` 使用合法 TargetGeometry 首先自举，首个 `FramebufferUpdateRequest` 触发合法 FULL，不等待首张像素才能知尺寸；不能插入 Native 私有报文。Native HMI 实际 TLS/网络/Parser/Codec 在 Private，同设备端先验证真实 DeviceTLS，再按 V8 A/B 原有 8-byte BE Header、**21 类消息**、已批准 P-A/A 的合法 type/字段/状态、两种模式 `plain=0` 与 `tls-password=1` 都 TLS-first；前者只是无额外密码 AUTH，不等于明文或自然人登录。Native 服务器独立 View/ControlGrant、Target、FULL/DELTA 与输入，不借 RFB；未知字段/长度/状态机非法必须按已批准错误语义拒绝。Gateway 登录/RoutePermit 不代替 DeviceTrust/HMI 授权，也不向 Native wire 追加企业字段。

### CT-08｜FrameLease、真实内存/队列资源准入
`AuthorizedFrameLease` 是 Host 保管的同进程、不可变、已脱敏逻辑帧引用，`LeaseId/StorageGeneration` 防旧地址复用；TTL 使旧帧失去未来发送资格，但**不等于引用已经归零**，物理内存只有实际 Release 后可重用；取消任务/回调不能持裸指针。既有 V8 **128MiB 受管总预算、96MiB raw、Native 每 Peer 可控待发 512KiB、连接和输入上限**不变。总预算按真实唯一 backing/Privacy copy/Codec scratch/TLS scratch/Qt socket pending/Session/FramePool 记账，必须在物理分配前预留峰值；GPU Driver/内核内存按既有定义另外如实披露。Native Pending 同时包含应用队列、QSslSocket 待加密明文、已加密待网络队列与保守加密余量，不只管 codec QByteArray；Qt 本身无固定 512KiB 输出上限。4K RGBA Full≈31.64MiB，四份≈126.56MiB，无法承诺任意 4K×多 Peer×不同 mask 组合都在预算内；先合并帧需求/限制在途副本/采用合法 tile、降频或拒准入；不得阻塞 Qt GUI/现场业务。

### CT-09｜Qt 最终输入与 held 来源结算
输入统一携 `Session/Scope/ControlEpoch/TargetGeneration/GeometryRevision/Sequence`，网络接纳不代表 Qt 已执行，GUI 合法线程的最终 Adapter 在真实开始前重新检查授权并登记已开始同步调用；T0 以后新旧 ControlEpoch 调用禁止开始。每 Peer/资源 Key/Button 的 held ledger 独立，撤掉远端绝不释放现场其它来源的按键；错误/断线保护性 UP 属设备清理而不是旧权限新增动作；无法确定物理来源时不得无依据全局 KeyUp。Native 五按钮和 17 个独立 keypad 编码遵守 P-A/A，不能丢 X1/X2 或键盘身份；CommittedText 与 Key 分离，不重复提交中文 IME。Qt Adapter 已发事件不证明 PLC/告警/文件业务状态，缺受信应用回执结果为 UNKNOWN、不自动重放有副作用操作。

### CT-10｜Viewer 与可信身份、显示
ClientEngine 以 RFB/Native 独立解析器消费统一 immutable `FrameSnapshot(TargetGeneration/ContentRevision/DisplayEpoch)`；Qt Quick RemoteSurface 只消费当前合法对象，不操作 HMI QObject，Rescale/DPR/旋转/触控坐标逆变换根据最新 TargetGeometry。DeviceTrust、当前路由、操作员认证、ViewGrant、ControlGrant 与画面是否新鲜为正交状态；连接成功不可视作可控制。失焦或失权即阻止新远端输入并清来源 held，遮蔽失权敏感旧帧；重连必须新会话和授权，不复活旧差分基线或旧按键；用户档案不是授权，SecretStore 不能明文回退。View G3 所需 QtQuick 性能资格与产品开发完成度独立。

### CT-11｜四 Qt 入口与双协议统一 Host Admission
C++、QML、Generic、QPA peer frontend 只转换目标登记、部署参数和生命周期通知；QML 在合法初始化完成后进行激活，Generic/QPA 共享同一个自动控制器而不重复监听，QPA 私有 ABI 仅精确 Qt Kit/OS/编译器档位。RFB_ONLY/Native_ONLY/DUAL_REQUIRED 按部署明确声明：`PrepareTargets+Policy+TLS+Module` → 必需端点预绑定但**不准许应用会话建立** → 全部成功后发布统一 AdmissionEpoch；任何必需端失败撤销本次对外准入、保留 HMI 现场业务（两个 OS bind 不是原子事务，**原子的是应用级准入承诺**）。运行期单 Peer/协议错误尽可能局部处理，但同进程崩溃不能声称另一协议进程级隔离。

### CT-12｜工厂 Edge/可选 Gateway 与设备最终权限
Factory PC 只安装一个 HyRemoteView；登录态 Edge 显式启用且默认 OFF，`SitePolicyRevision/EndpointBinding/EdgeRunLease` 由本地受信管理员维护。Gateway 仅为受控站点与 HMI 建立短期 RoutePermit/单次 Pair/opaque FIFO 字节路由；Edge/Cloud 既不解 DeviceTLS，也不签 Grant、不得任意 host:port 代理、不得剪切已加密协议字节当“帧丢弃”。路由许可撤销停止 Route，但不能冒充离线 HMI 已撤设备权；HMI DeviceTLS 与本地 Grant 单独决定远控。Gateway #190 可选激活，未启用时合法 Direct 完全独立；View 注销/退出使登录期 RunEpoch 失效，旧 Route 不在恢复后无批准复活。

### CT-13｜Tool 独立与来源可信诊断
HyRemoteTool 保留已发布基础 RFB 访问与其独立发布路线；完整 Tool S1 #586 未宣称完成。未来只读 SDK/Qt Kit Inspector、集成辅助与脱敏 SupportReport 属 Tool 的明确角色，不重建第二套 Native/Managed 正式 View；导入证据区分 `UNOBSERVED/NOT_RUN/PASS/FAIL` 来源，不能扫描机密或把未知环境判为健康。Tool 安装失败、升级失败不得影响 HMI Host 独立运行。

### CT-14｜独立产品打包、兼容与可信更新
Public SDK 可独立从源码及安装包发行，Private View 在子模块固定 SHA 构建但不等于 ABI/协议兼容证据；HMI Native 模块是 **Private 可选 HMI 交付组件**，设备配置必须匹配 HostPort major/struct size、Qt Kit、OS/ARCH、编译器/CRT、TLS 依赖、签名/摘要与 DeviceTLS credential reference；不支持的组合 fail closed。升级/回退在维护窗口受控重启、旧 HostEpoch/Session/Grant/Privacy/RouteRev 不复活；动态模块未达到 `callbacks=0,leases=0,QtObjects=0,workerTasks=0` 不能卸载，即便 STOPPED 也可留映射到进程结束。发布支持声明必须按真实路径/平台/Kit，不能用编译成功替代场景验收。

### CT-15｜本期能力边界、质量反证与后期扩展
`H-O01=A`：本期不以 Native 21 type 实现文件传输或可信宿主业务回执；显示 `UNSUPPORTED` / `OUTCOME_UNKNOWN` 且绝不自动重复副作用指令，未来受控扩展另行设计文件收件/宿主原子结果。`H-O02=A`：离线签发/待送达不是 HMI 已生效撤权，本期有限售后委托按 HMI 可证本地期限和保守失效策略，永久合法 Direct 权益不受云订阅到期冒充撤销；未来受信设备收件回执通道不在本期。剪贴板、打印、音频、录制、OEM 等可选能力分别声明，不擅自自动加入/延期整组。设计正负例入口沿已有 FW/AFS/RLG/VIEW/EG/XP 与 31 项表 6 场景；此处是设计审计，不跑测试。

## 3. 收口三：完整产品追踪及独立内容反证

### 3.1 审查证据等级
`SOURCE_INDEX_PRESENT`：已见经 Human 证明正文等效的 V15 56 行/V8 99 行历史归档索引；**当前没有取得正式版 DOCX/ADR 原件逐句比较**。
`CONTRACT_MAPPED`：本文件建立了该来源小节→既有设计主责/受影响合同/失败边界的**审查入口**，不意味本段每一句都已取得正式 Human 原文审签。
`CURRENT_PHASE_BOUNDARY`：本期无权支持/未来延期的部分标明明确行为，而不是无期限 OPEN。
`IMPLEMENTATION_NOT_RUN`：一切编译、测试、真实 Qt5/6/ARM64/GPU/Native/TLS/Managed E2E 仍按适用 Gate 获取证据，**不会以文档映射伪造通过**。

### 3.2 V15 56 项逐项映射（直接从已归档源码索引保留源节名/责任/失败焦点）
| 源条款 | 原索引标题 | 已有设计责任（按新 Owner 修正） | 当前目标合同 | 原索引失败闭合要求 | 设计分期判断 |
|---|---|---|---|---|---|
| V15 §1.1 | 产品所解决的问题 | D1 F-02; D2 V-01 | CT-01/15 | 越界 OS/业务操作拒绝 | 本期设计合同（实现/测试未授权） |
| V15 §1.2 | 面向的使用者 | D2 V-03; D4 X-02 | CT-01/15 | 角色可见不等于设备控制权 | 本期设计合同（实现/测试未授权） |
| V15 §1.3 | 典型使用情境与阅读顺序 | D5 W00; D4 X-02 | CT-01/15 | 不得自动复活操作 | 本期设计合同（实现/测试未授权） |
| V15 §1.4 | 设计范围与非目标 | D1 F-02; D3 E-01/G-01 | CT-01/15 | 无授权目标/任意 TCP 拒绝 | 本期设计合同（实现/测试未授权） |
| V15 §2.1 | 产品组件及其协作方式 | D1 F-02; D2 V-03; D3 E-01/G-01; D4 T-01 | CT-01/07 | 中继不授设备 Grant；Tool 不替代 View | 本期设计合同（实现/测试未授权） |
| V15 §2.2 | 共享运行时与两种远程访问协议 | D1 F-01/F-02/F-04/F-05 | CT-01/07 | 不得第三强制通道或 Native 依赖 RFB | 本期设计合同（实现/测试未授权） |
| V15 §2.3 | 宿主负责业务结果确认 | D1 F-06; D2 V-03 | CT-09/15 | 结果未知不自动重复业务命令 | 本期设计合同（实现/测试未授权） |
| V15 §3.1 | 四种接入方式的适用场景 | D1 F-03; D4 X-01 | CT-02/11 | 不得第二采集/授权系统 | 本期设计合同（实现/测试未授权） |
| V15 §3.2 | 启动、停止与异常处理 | D1 F-02/F-06; D4 X-02 | CT-02/11 | 启动失败回滚；宿主本地 HMI 不被强制终止 | 本期设计合同（实现/测试未授权） |
| V15 §3.3 | 配置及环境适配 | D1 F-03; D4 X-01 | CT-02/11 | 缺配置 FailClosed；编译不算运行资格 | 本期设计合同（实现/测试未授权） |
| V15 §3.4 | 免改源码接入的授权与交付核查 | D1 F-03/F-06; D4 X-02 | CT-02/11 | 无法区分敏感目标则停发 | 本期设计合同（实现/测试未授权） |
| V15 §4.1 | 远程访问目标的范围 | D1 F-02/F-06; D2 V-03 | CT-04/05 | 关闭/撤销目标后帧与输入同步失效 | 本期设计合同（实现/测试未授权） |
| V15 §4.2 | 多屏查看及窗口安排 | D1 F-02; D2 V-02/V-03 | CT-04/05 | 历史布局不得恢复失权目标 | 本期设计合同（实现/测试未授权） |
| V15 §4.3 | 显示准确性与画面时效 | D1 F-02/F-06; D2 V-02 | CT-04/05 | 敏感或规则不明时隐藏；停止旧输入 | 本期设计合同（实现/测试未授权） |
| V15 §5.1 | 标准 RFB 的适用范围 | D1 F-04/F-06; D2 V-01 | CT-07/10 | 共享口令不伪造个人或公网安全 | 本期设计合同（实现/测试未授权） |
| V15 §5.2 | HyRemote 私有协议的适用范围 | D1 CT-03 (HostPort)/Private F14 (Native HMI)/F-06; D2 V-01 | CT-07 | 未知 wire 值拒绝，不能猜测兼容 | 本期设计合同（实现/测试未授权） |
| V15 §5.3 | 直接连接、受管接入及恢复 | D2 V-01/V-03; D3 E-01/G-01 | CT-07/10/12 | 无静默弱路径回退 | 本期设计合同（实现/测试未授权） |
| V15 §5.4 | 兼容性说明及故障提示 | D1 F-04/Private F14; D2 V-03 | CT-07/10 | 不将能力缺失、拒绝、不可达合并成成功 | 本期设计合同（实现/测试未授权） |
| V15 §6.1 | 查看权限与控制权限 | D1 F-06; D2 V-03 | CT-04/09 | 审批前后双端最终栅栏；不能偷用旧 Grant | 本期设计合同（实现/测试未授权） |
| V15 §6.2 | 现场人员参与及授权方式 | D1 F-06; D2 V-03 | CT-04/09 | 撤权清 held，现场无需依赖 Viewer 配合 | 本期设计合同（实现/测试未授权） |
| V15 §6.3 | 断线恢复及控制权交接 | D1 F-02/F-06; D2 V-01/V-03 | CT-04/09 | 不重放迟到输入/控制申请/业务动作 | 本期设计合同（实现/测试未授权） |
| V15 §7.1 | 键盘与鼠标操作 | D1 F-06; D2 V-02 | CT-09/10 | 失焦/撤权/队列满安全释放 | 本期设计合同（实现/测试未授权） |
| V15 §7.2 | 中文及其他输入法 | D1 CT-03 (HostPort)/Private F14 (Native HMI)/F-06; D2 V-02 | CT-09/10 | 禁止重复提交/漏字/误快捷键 | 本期设计合同（实现/测试未授权） |
| V15 §7.3 | 触屏设备的输入方式 | D1 CT-03 (HostPort)/Private F14 (Native HMI)/F-06; D2 V-02 | CT-09/10 | 本地手势不能触发远端业务 | 按#172独立触控规划；当期仅已批准MouseEmulation/安全拒绝，不宣称真多点触控 |
| V15 §8.1 | 设备工作区与连接入口 | D2 V-01/V-03 | CT-10 | 存储档案不能自动授予会话 | 本期设计合同（实现/测试未授权） |
| V15 §8.2 | 以远程画面为中心的会话界面 | D2 V-02/V-03 | CT-10 | 切换/失焦停止旧输入；迟到批准不得激活 | 本期设计合同（实现/测试未授权） |
| V15 §8.3 | 多设备操作与风险防范 | D2 V-02/V-03 | CT-10/09 | 背景焦点不可转发误输入 | 本期设计合同（实现/测试未授权） |
| V15 §8.4 | 客户端更新与恢复 | D2 V-03; D4 X-01 | CT-10/14 | 升级失败不能遗失/泄漏凭据 | 本期设计合同（实现/测试未授权） |
| V15 §8.5 | 关键操作状态与异常反馈 | D2 V-03 | CT-10/13 | 不得已连接推断可控，权限失效立即禁用 | 本期设计合同（实现/测试未授权） |
| V15 §8.6 | 工厂边缘接入与设备共享 | D2 V-03; D3 E-01/G-01 | CT-12 | 仅路由许可不等于 ViewGrant/ControlGrant | Managed条件路径，不阻塞Direct |
| V15 §8.7 | 边缘接入运行与恢复 | D2 V-03; D3 E-01; D4 X-01 | CT-12 | 注销真实离线、崩溃后不假在线/复活 Grant | Managed条件路径，不阻塞Direct |
| V15 §9.1 | 文本和文件交换 | D1 F-06; D2 V-03 | CT-09/15 | 不因此获得任意目录权限 | 按能力拆分：文件/可信业务回执=H-O01=A延期；文本/剪贴板=#171独立授权与版本准入，不得整体延期或冒称已交付 |
| V15 §9.2 | 文件传输与业务应用的区别 | D1 F-06; D2 V-03 | CT-15 | 结果未知不能重复提交业务动作 | 当期按H-O01=A明确拒绝;未来接口不在本期 |
| V15 §9.3 | 拖放、打印及应用通知 | D1 F-06; D2 V-03 | CT-15 | 通知不能隐式确认告警或升级设备 | 拖放/打印/通知是独立可选能力；未经产品版本和安全准入不纳入当期实现 |
| V15 §9.4 | 音频、协作标注及录制 | D2 V-03; D4 X-02 | CT-15 | 录制默认关闭，撤权停止采集，不上传系统声音 | 音频/协作/录制独立可选能力；未经Owner明确版本及隐私准入不纳入当期实现 |
| V15 §10.1 | 设备身份与访问控制 | D1 F-06; D2 V-03; D3 E-01/G-01 | CT-04/07/14 | Gateway 会话不替代 HMI View/ControlGrant | 本期设计合同（实现/测试未授权） |
| V15 §10.2 | 可见范围及隐私遮罩 | D1 F-02/F-06; D2 V-02; D3 E-01 | CT-04/05 | 遮罩不可证则拒帧；中继不存明文 | 本期设计合同（实现/测试未授权） |
| V15 §10.3 | 审计信息与证据边界 | D4 X-02; D3 G-01 | CT-12/13/15 | 共享密码不自然人归因；元数据非业务成功 | 本期设计合同（实现/测试未授权） |
| V15 §11.1 | 以用户问题组织诊断 | D2 V-03; D4 T-01 | CT-13 | 无权信息不展示、未观测不推断 | 本期设计合同（实现/测试未授权） |
| V15 §11.2 | 轻量化诊断信息 | D1 F-02; D4 T-01/X-02 | CT-13 | 不能常驻采集密码/键盘全文 | 本期设计合同（实现/测试未授权） |
| V15 §11.3 | HyRemoteTool 的集成诊断过程 | D4 T-01 | CT-13 | 未执行不能 PASS；Tool 完整 S1 #586 仍 OPEN | 本期设计合同（实现/测试未授权） |
| V15 §11.4 | 优先保障现场应用 | D1 F-01/F-02; D3 E-01/G-01; D4 X-02 | CT-05/08/09 | 慢端不能饿死 GUI 或串漏会话数据 | 本期设计合同（实现/测试未授权） |
| V15 §12.1 | 设备交付后的治理责任 | D1 F-06; D4 X-02 | CT-04/14/15 | 未下发撤销不能虚报生效；不可续活到期委托 | 当期按H-O02=A本地事实;未来回执延期 |
| V15 §12.2 | 品牌定制与企业服务 | D2 V-03; D3 G-01; D4 X-01 | CT-14/15 | 品牌/账号不得扩权或关闭 Direct | 本期设计合同（实现/测试未授权） |
| V15 §12.3 | 更新责任及离线使用权益 | D2 V-03; D4 X-01/X-02 | CT-14/15 | 服务到期不能撤销合法基础离线访问 | 本期设计合同（实现/测试未授权） |
| V15 §13.1 | 允许修改源码的 Qt 应用接入 | D1 F-03/F-06; D4 X-02 | CT-02/11 | 配置错误不强制停宿主业务 | 本期设计合同（实现/测试未授权） |
| V15 §13.2 | 双屏工业设备的远程维护 | D1 F-02/F-06; D2 V-02 | CT-04/10 | 屏号非身份，残留输入必须清理 | 本期设计合同（实现/测试未授权） |
| V15 §13.3 | 客户授权设备厂商开展售后服务 | D1 F-06; D4 X-02 | CT-04/15 | 无期限信任/目录浏览不可默认开放 | 本期设计合同（实现/测试未授权） |
| V15 §13.4 | 网络中断后的安全恢复 | D1 F-06; D2 V-01/V-02 | CT-04/06/07/10 | 不保留敏感画面、不重放旧操作 | 本期设计合同（实现/测试未授权） |
| V15 §13.5 | 首次接入与设备身份变化 | D2 V-03; D1 CT-03 (HostPort)/Private F14 (Native HMI) | CT-07/10/14 | 同 IP 不等于同设备 | 本期设计合同（实现/测试未授权） |
| V15 §13.6 | 无法修改源码的存量 Qt 应用接入 | D1 F-03/F-06 | CT-11 | 不能区分目标则拒绝 | 本期设计合同（实现/测试未授权） |
| V15 §13.7 | 现场人员接管远程操作 | D1 F-06; D2 V-03 | CT-04/09 | 已提交业务结果查询，不能假撤回 | 本期设计合同（实现/测试未授权） |
| V15 §13.8 | 工厂内外协同的远程维护 | D2 V-03; D3 E-01/G-01 | CT-12 | Gateway/Edge 不提供通用内网 TCP 代理 | Managed条件路径，不阻塞Direct |
| V15 §13.9 | 云端不可用与受管会话安全恢复 | D2 V-01; D3 E-01/G-01 | CT-12 | 不自动降级/恢复旧授权/旧业务 | Managed条件路径，不阻塞Direct |
| V15 §14.1 | 以实际使用结果为依据 | D4 X-02; D5 W00 | CT-15 | 未执行不算通过，PoC 不外推产品 | 本期设计合同（实现/测试未授权） |
| V15 §14.2 | 全文一致遵循的设计原则 | D5 W00; D4 X-02 | CT-15 | 缺运行证据或未审协议不得冻结 | 本期设计合同（实现/测试未授权） |

### 3.3 V8 99 项逐项映射（同一设计合同，不是 99 项实测）
| 源条款 | 原索引标题 | 已有设计责任（按新 Owner 修正） | 当前目标合同 | 原索引必审失败焦点 | 设计分期判断 |
|---|---|---|---|---|---|
| V8 1.1 | 文档目的与范围 | D5 W00; D4 X-02 | CT-15 | V15/V8 权威与安全边界，不越权改上位范围 | 本期设计合同（实现/测试未授权） |
| V8 1.2 | 规范依据 | D5 W00; D4 X-02 | CT-15 | V15/V8 权威与安全边界，不越权改上位范围 | 本期设计合同（实现/测试未授权） |
| V8 1.3 | 产品边界与原则 | D5 W00; D4 X-02 | CT-15 | V15/V8 权威与安全边界，不越权改上位范围 | 本期设计合同（实现/测试未授权） |
| V8 1.4 | 能力范围 | D5 W00; D4 X-02 | CT-15 | V15/V8 权威与安全边界，不越权改上位范围 | 本期设计合同（实现/测试未授权） |
| V8 2.1 | 产品组成与用户 | D1 F-02; D2 V-03; D3 E-01/G-01; D4 T-01 | CT-01 | Framework/View/Tool/Gateway 职责隔离与宿主业务确认 | 本期设计合同（实现/测试未授权） |
| V8 2.2 | 组件职责划分 | D1 F-02; D2 V-03; D3 E-01/G-01; D4 T-01 | CT-01 | Framework/View/Tool/Gateway 职责隔离与宿主业务确认 | 本期设计合同（实现/测试未授权） |
| V8 2.3 | 宿主业务职责 | D1 F-02; D2 V-03; D3 E-01/G-01; D4 T-01 | CT-09/15 | Framework/View/Tool/Gateway 职责隔离与宿主业务确认 | 本期设计合同（实现/测试未授权） |
| V8 2.4 | 直接连接与企业服务 | D1 F-02; D2 V-03; D3 E-01/G-01; D4 T-01 | CT-07/12 | Framework/View/Tool/Gateway 职责隔离与宿主业务确认 | 本期设计合同（实现/测试未授权） |
| V8 3.1 | 总体分层与依赖 | D1 F-01/F-02/F-03; D3 E-01 | CT-01/02 | 四接入一运行时、Core 不依赖 Qt UI/具体协议、Edge 不入 HMI Core | 本期设计合同（实现/测试未授权） |
| V8 3.2 | 核心层职责 | D1 F-01/F-02/F-03; D3 E-01 | CT-01/02 | 四接入一运行时、Core 不依赖 Qt UI/具体协议、Edge 不入 HMI Core | 本期设计合同（实现/测试未授权） |
| V8 3.3 | 共享运行时职责 | D1 F-01/F-02/F-03; D3 E-01 | CT-01/02 | 四接入一运行时、Core 不依赖 Qt UI/具体协议、Edge 不入 HMI Core | 本期设计合同（实现/测试未授权） |
| V8 3.4 | 界面目标适配 | D1 F-01/F-02/F-03; D3 E-01 | CT-02/05/11 | 四接入一运行时、Core 不依赖 Qt UI/具体协议、Edge 不入 HMI Core | 本期设计合同（实现/测试未授权） |
| V8 3.5 | 私有接口依赖 | D1 F-01/F-02/F-03; D3 E-01 | CT-01/02 | QtPrivate 受控例外、public ABI 不泄漏、具体 Kit 实机证据 | 本期设计合同（实现/测试未授权） |
| V8 3.6 | 工厂边缘接入与客户端职责 | D1 F-01/F-02/F-03; D3 E-01 | CT-01/12 | 四接入一运行时、Core 不依赖 Qt UI/具体协议、Edge 不入 HMI Core | 本期设计合同（实现/测试未授权） |
| V8 4.1 | 编程式接口接入 | D1 F-03/F-06; D4 X-01 | CT-11 | 四入口同部署策略，QPA 精确 Kit 与 RFB/Native/Dual 准入 | 本期设计合同（实现/测试未授权） |
| V8 4.2 | 声明式接口接入 | D1 F-03/F-06; D4 X-01 | CT-11 | 四入口同部署策略，QPA 精确 Kit 与 RFB/Native/Dual 准入 | 本期设计合同（实现/测试未授权） |
| V8 4.3 | 通用插件接入 | D1 F-03/F-06; D4 X-01 | CT-11 | 四入口同部署策略，QPA 精确 Kit 与 RFB/Native/Dual 准入 | 本期设计合同（实现/测试未授权） |
| V8 4.4 | 平台插件接入 | D1 F-03/F-06; D4 X-01 | CT-11 | 四入口同部署策略，QPA 精确 Kit 与 RFB/Native/Dual 准入 | 本期设计合同（实现/测试未授权） |
| V8 4.5 | 协议构建与配置 | D1 F-03/F-06; D4 X-01 | CT-02/07/11 | RFB-only / Native-only / Dual 三模式；禁用全部时远控 Runtime 构建失败 | 本期设计合同（实现/测试未授权） |
| V8 4.6 | 部署配置与启动 | D1 F-03/F-06; D4 X-01 | CT-02/11/14 | 四入口同部署策略，QPA 精确 Kit 与 RFB/Native/Dual 准入 | 本期设计合同（实现/测试未授权） |
| V8 5.1 | 目标与会话模型 | D1 F-02/F-06; D2 V-03 | CT-04 | Target/Session/Scope/Grant 区分、控制独占与现场撤权 | 本期设计合同（实现/测试未授权） |
| V8 5.2 | 身份与权限关系 | D1 F-02/F-06; D2 V-03 | CT-04 | Target/Session/Scope/Grant 区分、控制独占与现场撤权 | 本期设计合同（实现/测试未授权） |
| V8 5.3 | 控制申请与独占 | D1 F-02/F-06; D2 V-03 | CT-04 | Target/Session/Scope/Grant 区分、控制独占与现场撤权 | 本期设计合同（实现/测试未授权） |
| V8 5.4 | 授权接口与策略版本 | D1 F-02/F-06; D2 V-03 | CT-04 | 不可变 PolicySnapshot 原子生效，旧 revision 不能覆盖 | 本期设计合同（实现/测试未授权） |
| V8 5.5 | 撤销与现场接管 | D1 F-02/F-06; D2 V-03 | CT-04/06/09 | 先撤权栅栏→清未执行输入/待发帧→通知，业务结果不能逆向撤销 | 本期设计合同（实现/测试未授权） |
| V8 6.1 | 远程目标与采集范围 | D1 F-02/F-06; D2 V-02 | CT-05/06 | 共享底帧、逐会话隐私衍生、撤权后可控缓冲失效 | 本期设计合同（实现/测试未授权） |
| V8 6.2 | 画面采集与更新 | D1 F-02/F-06; D2 V-02 | CT-05/06 | 共享底帧、逐会话隐私衍生、撤权后可控缓冲失效 | 本期设计合同（实现/测试未授权） |
| V8 6.3 | 会话隐私处理 | D1 F-02/F-06; D2 V-02 | CT-05/06/08 | 共享底帧、逐会话隐私衍生、撤权后可控缓冲失效 | 本期设计合同（实现/测试未授权） |
| V8 6.4 | 授权变化与缓存清理 | D1 F-02/F-06; D2 V-02 | CT-04/05/06 | 旧 PolicyRevision 待发帧失效；已入 socket 不能取消则受控断链 | 本期设计合同（实现/测试未授权） |
| V8 6.5 | 画面变换与断线显示 | D1 F-02/F-06; D2 V-02 | CT-05/10 | 共享底帧、逐会话隐私衍生、撤权后可控缓冲失效 | 本期设计合同（实现/测试未授权） |
| V8 7.1 | 输入事件归一化 | D1 F-06; D2 V-02 | CT-09 | 事件归一化、双重准入、跨来源 held 状态与 Qt 最终栅栏 | 本期设计合同（实现/测试未授权） |
| V8 7.2 | 输入接纳与提交 | D1 F-06; D2 V-02 | CT-09 | 事件归一化、双重准入、跨来源 held 状态与 Qt 最终栅栏 | 本期设计合同（实现/测试未授权） |
| V8 7.3 | 多来源输入协调 | D1 F-06; D2 V-02 | CT-09 | 事件归一化、双重准入、跨来源 held 状态与 Qt 最终栅栏 | 本期设计合同（实现/测试未授权） |
| V8 7.4 | 授权复核与释放 | D1 F-06; D2 V-02 | CT-09 | 接纳与 Qt 实际执行前双重 check；保护性释放不凭旧 Grant | 本期设计合同（实现/测试未授权） |
| V8 7.5 | 键鼠、文本与触控 | D1 F-06; D2 V-02 | CT-09 | 事件归一化、双重准入、跨来源 held 状态与 Qt 最终栅栏 | 本期设计合同（实现/测试未授权） |
| V8 7.6 | 本地输入边界 | D1 F-06; D2 V-02 | CT-09 | 事件归一化、双重准入、跨来源 held 状态与 Qt 最终栅栏 | 本期设计合同（实现/测试未授权） |
| V8 8.1 | 协议组成与能力 | D1 F-04/Private F14; D2 V-01; D3 E-01/G-01 | CT-07 | RFB/Native 双独立协议与设备身份/帧/输入/受管透明路径 | 本期设计合同（实现/测试未授权） |
| V8 8.2 | 协商与会话建立 | D1 F-04/Private F14; D2 V-01; D3 E-01/G-01 | CT-07 | SOURCE_TENSION_01 已获 Human=A：plain 与 tls-password 都须 HMI DeviceTLS 先行；Host/View 状态与测试仍待审核 | 本期设计合同（实现/测试未授权） |
| V8 8.3 | 目标发现与订阅 | D1 F-04/Private F14; D2 V-01; D3 E-01/G-01 | CT-07 | RFB/Native 双独立协议与设备身份/帧/输入/受管透明路径 | 本期设计合同（实现/测试未授权） |
| V8 8.4 | 画面更新与控制 | D1 F-04/Private F14; D2 V-01; D3 E-01/G-01 | CT-07 | RFB/Native 双独立协议与设备身份/帧/输入/受管透明路径 | 本期设计合同（实现/测试未授权） |
| V8 8.5 | 输入与扩展能力 | D1 F-04/Private F14; D2 V-01; D3 E-01/G-01 | CT-07 | RFB/Native 双独立协议与设备身份/帧/输入/受管透明路径 | 本期设计合同（实现/测试未授权） |
| V8 8.6 | 报文规范引用 | D1 F-04/Private F14; D2 V-01; D3 E-01/G-01 | CT-07 | V8 A/B 唯一权威，21 type ID 与字段不可被派生 D1/D2 另编 | 本期设计合同（实现/测试未授权） |
| V8 8.7 | 受管接入的连接与数据转发 | D1 F-04/Private F14; D2 V-01; D3 E-01/G-01 | CT-07/12 | 中继只转获准 ByteStream、有界 FIFO，禁止任意端口与降级 | Managed条件路径，不阻塞Direct |
| V8 9.1 | 部署授权与监听准入 | D1 CT-03 (HostPort)/Private F14 (Native HMI)/F-06; D2 V-03; D3 E-01/G-01 | CT-02/04/07/14 | 部署清单不可信/目标歧义/监听缺许可一律 fail closed | 本期设计合同（实现/测试未授权） |
| V8 9.2 | 标准协议安全边界 | D1 CT-03 (HostPort)/Private F14 (Native HMI)/F-06; D2 V-03; D3 E-01/G-01 | CT-07 | VNCAuth 不加密、不构成独立自然人身份 | 本期设计合同（实现/测试未授权） |
| V8 9.3 | 设备身份与安全连接 | D1 CT-03 (HostPort)/Private F14 (Native HMI)/F-06; D2 V-03; D3 E-01/G-01 | CT-07 | DeviceTLS 身份与 Gateway/Edge/用户站点身份分离 | 本期设计合同（实现/测试未授权） |
| V8 9.4 | 凭据与密码验证 | D1 CT-03 (HostPort)/Private F14 (Native HMI)/F-06; D2 V-03; D3 E-01/G-01 | CT-07 | tls-password 独立 salted verifier，PBKDF2-HMAC-SHA256 及失败限额 | 本期设计合同（实现/测试未授权） |
| V8 9.5 | 安全故障与审计 | D1 CT-03 (HostPort)/Private F14 (Native HMI)/F-06; D2 V-03; D3 E-01/G-01 | CT-07/13/15 | 监听前信任、HMI DeviceTLS、凭据/密码与站点/路由分权 | 本期设计合同（实现/测试未授权） |
| V8 9.6 | 版本迁移与兼容 | D1 CT-03 (HostPort)/Private F14 (Native HMI)/F-06; D2 V-03; D3 E-01/G-01 | CT-07/14 | 监听前信任、HMI DeviceTLS、凭据/密码与站点/路由分权 | 本期设计合同（实现/测试未授权） |
| V8 9.7 | 工厂站点信任与受控设备路由 | D1 CT-03 (HostPort)/Private F14 (Native HMI)/F-06; D2 V-03; D3 E-01/G-01 | CT-12 | RoutePermit 不代替 HMI DeviceId/Scope/Grant，边缘攻陷限制在短期路由 | Managed条件路径，不阻塞Direct |
| V8 10.1 | 运行状态与启停 | D1 F-01/F-02/F-06; D3 E-01; D4 X-02 | CT-06/08 | H-DUAL=A 初启必需协议全部成功；运行期单协议失效如实 DEGRADED | 本期设计合同（实现/测试未授权） |
| V8 10.2 | 故障隔离与恢复 | D1 F-01/F-02/F-06; D3 E-01; D4 X-02 | CT-02/08/14 | Runtime/端点故障隔离、受管资源配额和现场业务优先 | 本期设计合同（实现/测试未授权） |
| V8 10.3 | 输入与帧交付 | D1 F-01/F-02/F-06; D3 E-01; D4 X-02 | CT-06/08 | Runtime/端点故障隔离、受管资源配额和现场业务优先 | 本期设计合同（实现/测试未授权） |
| V8 10.4 | 网络与内存预算 | D1 F-01/F-02/F-06; D3 E-01; D4 X-02 | CT-08 | 8 连接/64 普通输入/136 保护性释放/512KiB Native 待发/128MiB 受管预算；非实际 RSS | 本期设计合同（实现/测试未授权） |
| V8 10.5 | 现场业务优先 | D1 F-01/F-02/F-06; D3 E-01; D4 X-02 | CT-08/09 | Runtime/端点故障隔离、受管资源配额和现场业务优先 | 本期设计合同（实现/测试未授权） |
| V8 10.6 | 工厂边缘接入的运行与资源预算 | D1 F-01/F-02/F-06; D3 E-01; D4 X-02 | CT-08/12 | Edge/Gateway 单独进程预算，不能在转发层拆删密文字节 | Managed条件路径，不阻塞Direct |
| V8 11.1 | 设备工作区 | D2 V-01/V-02/V-03; D3 E-01 | CT-10 | 设备档案/信任/多会话输入焦点/生命周期/更新 | 本期设计合同（实现/测试未授权） |
| V8 11.2 | 设备信任与权限状态 | D2 V-01/V-02/V-03; D3 E-01 | CT-10/14 | 设备档案/信任/多会话输入焦点/生命周期/更新 | 本期设计合同（实现/测试未授权） |
| V8 11.3 | 多设备与输入焦点 | D2 V-01/V-02/V-03; D3 E-01 | CT-09/10 | 设备档案/信任/多会话输入焦点/生命周期/更新 | 本期设计合同（实现/测试未授权） |
| V8 11.4 | 控制申请与断线处理 | D2 V-01/V-02/V-03; D3 E-01 | CT-04/10 | 设备档案/信任/多会话输入焦点/生命周期/更新 | 本期设计合同（实现/测试未授权） |
| V8 11.5 | 客户端更新与恢复 | D2 V-01/V-02/V-03; D3 E-01 | CT-10/14 | 设备档案/信任/多会话输入焦点/生命周期/更新 | 本期设计合同（实现/测试未授权） |
| V8 11.6 | HyRemoteView 内置边缘角色的生命周期 | D2 V-01/V-02/V-03; D3 E-01 | CT-10/12 | H-EDGE=A 登录期间在线；关闭画面窗口不等于退出站点服务；注销真离线 | Managed条件路径，不阻塞Direct |
| V8 12.1 | 诊断工具职责 | D4 T-01/X-02; D2 V-03 | CT-13 | 诊断事实出处、元数据脱敏、业务结果不得推断 | 本期设计合同（实现/测试未授权） |
| V8 12.2 | 开发与部署诊断 | D4 T-01/X-02; D2 V-03 | CT-13 | 诊断事实出处、元数据脱敏、业务结果不得推断 | 本期设计合同（实现/测试未授权） |
| V8 12.3 | 诊断证据来源 | D4 T-01/X-02; D2 V-03 | CT-13 | 诊断事实出处、元数据脱敏、业务结果不得推断 | 本期设计合同（实现/测试未授权） |
| V8 12.4 | 操作审计边界 | D4 T-01/X-02; D2 V-03 | CT-13/15 | 诊断事实出处、元数据脱敏、业务结果不得推断 | 本期设计合同（实现/测试未授权） |
| V8 12.5 | 诊断导出与脱敏 | D4 T-01/X-02; D2 V-03 | CT-13 | 诊断事实出处、元数据脱敏、业务结果不得推断 | 本期设计合同（实现/测试未授权） |
| V8 12.6 | 运行指标 | D4 T-01/X-02; D2 V-03 | CT-13 | 诊断事实出处、元数据脱敏、业务结果不得推断 | 本期设计合同（实现/测试未授权） |
| V8 13.1 | 业务接口与授权 | D1 F-06; D2 V-03; D4 X-02 | CT-09/15 | 文本与文件等分别授权，传输和业务结果分段确认 | 本期设计合同（实现/测试未授权） |
| V8 13.2 | 文本与剪贴板 | D1 F-06; D2 V-03; D4 X-02 | CT-09/15 | 文本与文件等分别授权，传输和业务结果分段确认 | 按#171独立授权剪贴板版本准入；当期仅安全边界和UNSUPPORTED行为 |
| V8 13.3 | 文件传输与业务确认 | D1 F-06; D2 V-03; D4 X-02 | CT-15 | 文本与文件等分别授权，传输和业务结果分段确认 | H-O01=A未来扩展，当前UNSUPPORTED |
| V8 13.4 | 拖放、打印与通知 | D1 F-06; D2 V-03; D4 X-02 | CT-15 | 文本与文件等分别授权，传输和业务结果分段确认 | 拖放/打印/通知属可选能力；需逐能力 Owner/版本和安全许可，未批准不得称已支持 |
| V8 13.5 | 音频、标注与录制 | D1 F-06; D2 V-03; D4 X-02 | CT-15 | 文本与文件等分别授权，传输和业务结果分段确认 | 音频/协作标注/录制属可选能力；隐私/用户同意与版本准入前不作为本期交付 |
| V8 13.6 | 可选能力扩展 | D1 F-06; D2 V-03; D4 X-02 | CT-15 | 文本与文件等分别授权，传输和业务结果分段确认 | 未来能力扩展仅保留安全与接口限制，不代表本期应实现 |
| V8 14.1 | 设备交付与接管 | D1 F-06; D2 V-03; D4 X-02 | CT-14/15 | 客户接管、离线撤销确认、有限售后委托和基础 Direct 权益 | 本期设计合同（实现/测试未授权） |
| V8 14.2 | 售后委托与期限 | D1 F-06; D2 V-03; D4 X-02 | CT-04/15 | 客户接管、离线撤销确认、有限售后委托和基础 Direct 权益 | 本期设计合同（实现/测试未授权） |
| V8 14.3 | 离线撤销与确认 | D1 F-06; D2 V-03; D4 X-02 | CT-04/15 | 离线撤销请求与 HMI 确认生效分开，不虚构已执行 | H-O02=A，离线待送达非设备生效 |
| V8 14.4 | 永久离线使用权益 | D1 F-06; D2 V-03; D4 X-02 | CT-14/15 | 合法永久基础 Direct 权益不因企业服务到期自动中止 | 本期设计合同（实现/测试未授权） |
| V8 14.5 | 品牌定制与企业服务 | D1 F-06; D2 V-03; D4 X-02 | CT-14/15 | 客户接管、离线撤销确认、有限售后委托和基础 Direct 权益 | 本期设计合同（实现/测试未授权） |
| V8 15.1 | 软件开发包与依赖 | D1 F-03; D2 V-03; D4 X-01/X-02 | CT-02/14 | Qt Kit/Private ABI、构建分发与准确支持声明 | 本期设计合同（实现/测试未授权） |
| V8 15.2 | 私有接口兼容 | D1 F-03; D2 V-03; D4 X-01/X-02 | CT-02/14 | Qt private ABI 对具体 Qt 版本/编译器/平台验证，不外推 public API 合格 | 本期设计合同（实现/测试未授权） |
| V8 15.3 | 平台与图形适配 | D1 F-03; D2 V-03; D4 X-01/X-02 | CT-02/14 | Qt Kit/Private ABI、构建分发与准确支持声明 | 本期设计合同（实现/测试未授权） |
| V8 15.4 | 构建与软件分发 | D1 F-03; D2 V-03; D4 X-01/X-02 | CT-02/14 | Qt Kit/Private ABI、构建分发与准确支持声明 | 本期设计合同（实现/测试未授权） |
| V8 15.5 | 支持声明与验证 | D1 F-03; D2 V-03; D4 X-01/X-02 | CT-14/15 | Qt Kit/Private ABI、构建分发与准确支持声明 | 本期设计合同（实现/测试未授权） |
| V8 16.1 | 产品条款追踪 | D4 X-02; D5 W00; D3 E-01/G-01 | CT-15 | 源自 V15/附录 D 的条款、主责章节与正负验证矩阵分层审签 | 本期设计合同（实现/测试未授权） |
| V8 16.2 | 构建与环境组合 | D4 X-02; D5 W00; D3 E-01/G-01 | CT-15 | 产品条款到架构/测试、变更、跨域 E2E 和证据归属 | 本期设计合同（实现/测试未授权） |
| V8 16.3 | 关键场景验证 | D4 X-02; D5 W00; D3 E-01/G-01 | CT-15 | 产品条款到架构/测试、变更、跨域 E2E 和证据归属 | 本期设计合同（实现/测试未授权） |
| V8 16.4 | 设计与实现证据 | D4 X-02; D5 W00; D3 E-01/G-01 | CT-15 | 产品条款到架构/测试、变更、跨域 E2E 和证据归属 | 本期设计合同（实现/测试未授权） |
| V8 16.5 | 演进与兼容原则 | D4 X-02; D5 W00; D3 E-01/G-01 | CT-15 | 产品条款到架构/测试、变更、跨域 E2E 和证据归属 | 本期设计合同（实现/测试未授权） |
| V8 16.6 | 跨域访问的架构验证 | D4 X-02; D5 W00; D3 E-01/G-01 | CT-12/15 | 产品条款到架构/测试、变更、跨域 E2E 和证据归属 | 本期设计合同（实现/测试未授权） |
| V8 A.1 | 通用消息格式 | D1 CT-03 (HostPort)/Private F14 (Native HMI); D2 V-01; D4 X-02 | CT-07 | 8-byte BE header、flags=0、TLS/plain 预配档位（与 §8.2 待消歧） | 本期设计合同（实现/测试未授权） |
| V8 A.2 | 消息类型与字段顺序 | D1 CT-03 (HostPort)/Private F14 (Native HMI); D2 V-01; D4 X-02 | CT-07 | 21 type/字段顺序/长度固定；原先未绑定 caps/status/format 数字已通过 Human P-A 受控补充，互操作未运行 | 本期设计合同（实现/测试未授权） |
| V8 A.3 | 输入子类型与键码 | D1 CT-03 (HostPort)/Private F14 (Native HMI); D2 V-01; D4 X-02 | CT-07/09 | INPUT subtype 1..5；modifier/button/key 映射部分未定 | Wire值域P-A已批准；5按钮/17Keypad无损 Qt 叶子适配尚待Gate-D设计签收 |
| V8 A.4 | 画面事务与重同步 | D1 CT-03 (HostPort)/Private F14 (Native HMI); D2 V-01; D4 X-02 | CT-07 | 每 Target 独立 generation/frame_id，FULL 覆盖、WAIT_FULL/REQUEST_FULL | 本期设计合同（实现/测试未授权） |
| V8 A.5 | 错误码与关闭原因 | D1 CT-03 (HostPort)/Private F14 (Native HMI); D2 V-01; D4 X-02 | CT-07 | ERROR 13 码及 CLOSE 1..5 原文已明确，TLS 失败无明文 ERROR 降级 | 本期设计合同（实现/测试未授权） |
| V8 A.6 | 长度与能力约束 | D1 CT-03 (HostPort)/Private F14 (Native HMI); D2 V-01; D4 X-02 | CT-07 | 32 Target、8192 单边、8388608 像素、64..256KiB Rect、上限验前分配 | 本期设计合同（实现/测试未授权） |
| V8 B.1 | 内部对象及宿主接口 | D1 F-03/F-06; D4 X-01 | CT-03/04/09 | 7 种 Host 接口和受控 Source/Policy Provider 生命周期 | 本期设计合同（实现/测试未授权） |
| V8 B.2 | 授权事件与线程约束 | D1 F-03/F-06; D4 X-01 | CT-03/04/09 | 7 授权事件及 InputEnvelope Qt 最终执行前 Runtime/Grant 双栅栏 | 本期设计合同（实现/测试未授权） |
| V8 B.3 | 受控部署清单示例 | D1 F-03/F-06; D4 X-01 | CT-02/03/14 | 示例键值不是审批凭据，不允许仅凭文本批准引用开放监听 | 本期设计合同（实现/测试未授权） |

### 3.4 不属于 99 个 Heading2 的约束仍适用
V8 附录 C：128MiB/96MiB/512KiB/连接与输入资源预算，归 CT-08，禁止按“每个阈值同时可满额”反推容量 SLA。
V8 附录 D：V15 56/56 映射，按上表逐项追踪、但不等于业务原文所有原子细目均 Human 审签。
V8 附录 E：交叉架构不变量归 CT-01、04、07、11、12、14，需要与原文逐项确认；本轮没取得正式 Word 不做伪审签。
V8 附录 F：术语与审计名称归 CT-15，尤其注意 HostGrant/RoutePermit、FrameId/ContentRevision、Epoch/T0/T1/T2 不混同。

### 3.5 14 模块跨界独立反证（设计逻辑核对，非运行）

| 关键反例 | 既有错误路径 | 现行限制与修订结论 | 设计审查 |
|---|---|---|---|
| Private F14 在 Public 另实现 Native | 两套 Native TLS/parser Owner 和错误 SHA 依赖 | F14 正式目标归 Private；Public F-05 仅设备政策/HostPort 与协议设计引用 | 无需并行两实现 |
| Public RFB_ONLY 无 Private 可否工作 | F15 把 F14 设必需 | F10/F11/F12/F13 + F15 不需要 F14；无私有装载要求 | 独立性条件成立 |
| Native-only 还要等 RFB | D5 旧 conditional DAG 漏改 | F14 Private + V12, HostPort, V13 Trust 基础；不依赖 F13/V10 | 旧错误依赖删除 |
| 同进程 Private 内存破坏 | 逻辑接口冒充 OS 沙箱 | 整个 HMI 是同一故障域；限制支持资格与可信升级 | 固有风险透明 |
| 撤权 T0 后编译晚到旧帧 | 编码完成被认为仍可发送 | FinalCheck 在 T0 后必须拒绝，T1 屏蔽永久旧代际 | 不跨网络线程裸写 |
| 撤权中旧 TLS 缓冲 | 优雅 disconnect 排空旧私密字节 | 真正 socket thread abort；不能追回已交 OS | T1 证据真实 |
| 只撤 Control 误断 View | 撤掉一项 Grant 就全设备断 | ControlOnly 画面继续；T1 NoAffectedOutput 明确 N/A | 影响范围精确 |
| 旧 Gtk/GPU 混合 Surface 看似正常 | QWidget render 不含 Native video/GL | 必需 source 缺失拒帧/遮蔽；不得拿旧像素伪新 FULL | Correctness 优先 |
| 4K 四全帧+多私隐 Peer | 名义零拷贝绕过 128MiB | 准入前峰值预算、限在途和 Peer，不接受不可控组合 | 有条件可行 |
| Native TLS plain 被当裸 TCP | DeviceTLS 缺失但免 AUTH | 两档都 DeviceTLS-first；Host 再 Grant | 安全合同不变 |
| Gateway 登录被当设备操作许可 | RoutePermit 冒充 ViewGrant | HMI DeviceTLS + Host 当前授权独立 | No cloud unlock |
| Edge RouteClose 当 HMI 已撤权 | 云端误报设备已处理 T2 | Route 状态只证明 Route；HMI 断链在本地另行结算 | 事实来源分离 |
| Tool 代替 View | 重复 Native/Managed 客户端 | Tool 维护基础 RFB + SDK/Kit/诊断，新完整 View 仍 Private | 按 H-T01 |
| File/HostACK 不在 Native wire | 开发临时加协议新消息 | 本期 UNSUPPORTED/UNKNOWN，不增加 21-type wire | 按 H-O01 |
| 离线设备未收撤销 | Server 签发即标 HMI Revoked | DEVICE_PENDING，不冒充设备生效；永久 Direct 另判 | 按 H-O02 |
| 设计通过被当发布通过 | 只看文档目录/PR CI | Gate-D(设计) / G3(技术证据) / Gate-I(实施许可) / Gate-R(发行) 各自独立 | 不虚假升级 Gate |

### 3.6 专业自审结论与尚未满足的正式批准事项

**本轮完成的三项设计交付**：（1）统一产品/源码 Owner 并纠正 F14/F15/V12/X10 旧依赖；（2）用 CT-01..15 把进程内装配/HostPort/Frame/Policy/FinalEgress/Qt/Resource/View/Edge/交付变成明确 Owner、时序、错误和拒绝合同；（3）把 V15 56/V8 99 原索引全量对应到当前目标合同和分期，完成 16 类关键跨模块反证，未发现需恢复独立进程或新增协议的设计必要性。

**保持不宣称完成的事项**：上表对应的是原有索引所摘要的技术义务，而非 V15/V8 正式 DOCX 每一句原文的受控逐字审签；附录 E/F 原文细节未逐句检查；`NativeHostPort` 在此是足以约束开发的接口语义/ABI 形状，非已生成可编译头文件；Qt5/Qt6/SoC/ARM64/Windows/Linux 图形支持和可靠性不能从逻辑合同推断真实合格；View G3/Native E2E/Gateway #190 仍是独立后续资格。**三项内容收口不意味着设计经过正式 Human 冻结审批，更不授权编码或测试**。

**决策与门禁**：D-PROC=B APPROVED，D-REVOKE=A APPROVED；其它 Human 批准的产品分期继续有效。TARGET_ARCH=V1.7_SINGLE_CANDIDATE；THREE_CONTENT_CLOSURES=DESIGN_COMPLETED_FOR_REVIEW；NORMATIVE_BYTE_REDLINE=NOT_DONE（原件不可访问）；Gate-D=NOT_FROZEN；Gate-I=NOT_AUTHORIZED；PRODUCT_CODE=UNCHANGED。


## 4. 可以交给各模块 Owner 的最小接口定义与错误时序（设计补充）

本节不定义可编译 C/C++ 头文件，不新造 Native 网络字段，而把需要约束实现的**数据结构、Owner、跨线程和可失败条件**确定下来。约束的优先级高于历史模块设计中含糊的“通过事件通知即可”“全部 Frame 可 zero-copy”等描述。

### 4.1 Host↔Private Native 进程内 ABI 单一调用约束

| API / 数据 | 输入与前置 | 出口事实、持有权 | 不满足时行为 |
|---|---|---|---|
| `Prepare(host_port_descriptor, module_config)` | 必须含 ABI major/minor/size、HostEpoch、可信配置 digest、QtKit profile；只在装载前准备阶段调用 | Private 只返回 `PREPARED` 或拒绝原因，不开放 Listener | 不兼容/不可信立即 `MODULE_UNAVAILABLE`，Public RFB_ONLY 路径独立 |
| `StartAdmission(admission_epoch)` | Host 完成必需 RFB/Native 预绑定、Target 与权限准入；调用所在管理线程由 Host 明确登记 | Native 可开始接受新 Peer；该状态不是 Peer 已获 Grant | 任一必需端失败本次 AdmissionEpoch 不对外发布 |
| `OpenPeer(provenance)` | 当前 TLS ConnectionInstance、认证档位、可信模块返回的设备身份/会话证据；绑定 ModuleGeneration | Host 分配 SessionId/Epoch，返回不透明 SessionHandle（Host owns） | 不在当前 ModuleGeneration 或 DeviceTLS 尚未建立则拒绝 |
| `Authorize(Session,Target,Scope,Kind)` | Host 当前 TargetGeneration、PolicyEpoch、Scope 和身份保证级别 | Host 可返回 `ViewGrantEpoch` 或 `ControlGrantEpoch`；Peer 不持“自行续期权” | POLICY_DENIED、SCOPE_UNAVAILABLE、STALE_SESSION，均不改变 Native1.0 报文字段 |
| `AcquireAuthorizedFrame(Session,Target,PrivacyRev,budget)` | 有效 ViewGrant、清洁 Surface、合格 ContentRevision、资源预留 | Host owns backing，Private 取得 LeaseHandle/len/format/epochs/expiry；只读是受信 API 约束 | RESOURCE_LIMIT、FRAME_UNAVAILABLE、PRIVACY_UNVERIFIABLE、STALE_EPOCH；不得回退 raw Frame |
| `ReleaseFrameLease(LeaseHandle)` | 只能由持有者释放一次，Host 校验 Generation | 真引用计数归零才能回收，TTL 到期不自动释放 | 重复 Release 幂等拒绝或错误记录；不能二次 free |
| `SubmitInputIntent(Session,Epochs,Input)` | 当前 ControlGrant、可信 Peer/Target/Geometry；传入有来源和顺序的事件 | 返回 HOST_ACCEPTED 或 DENIED；QtFinalGate 实际执行另行记录 | 不可把返回 HOST_ACCEPTED 写为 BUSINESS_APPLIED |
| `CloseOrRevoke(RevokeId,AffectedGrantSet)` | Host 先 T0，按 Grant/Scope/Target 确定受影响 Peer | Private 收到撤权命令只记 REQUEST_ACCEPTED；T1 须等 Peer 最后实际写入域报告 | 未执行 Barrier 不得提前报告 STOPPED；T2 必须另等 Qt Input/held 结算 |
| `StopNewPeer/Quiesce/Destroy` | 模块状态 ACTIVE→QUIESCING，先禁接入、逐 Peer 停发、清输入与真实引用 | STOPPED 不等于 UNLOAD_ALLOWED；Owner 在合法网络线程释放 QSslSocket | 未实际归零的回调/QtObject/Worker/Lease 存在时禁止卸载，必要时保留映射至 HMI 进程退出 |

**调用/线程规则**：Public HostPort 接口函数不能假定来自 Qt GUI；与 Target / Qt 输入有关的调用应由 Host 以受控异步命令转移到合法线程，在最终执行时重新校验 Epoch。阻止同步网络线程等待 GUI 再反向等待网络线程的循环等待。所有跨线程任务在调度/取消/结束时保留 ModuleGeneration 与 SessionEpoch 及引用计数；异步函数的 Accepted 返回值不是业务成功或撤权完成。C ABI 边界的“含长度 buffer”只约束正常调用；Private 同进程仍有能力通过内存破坏越界访问，不宣称安全沙箱。

### 4.2 最终发送串行化证明条件

`EncodedChunk` 在编码完成时始终带 `HostEpoch/SessionEpoch/ViewGrantEpoch/PrivacyRevision/TargetGeneration/FrameId/LeaseId`。**网络线程**处理的最后数据写入序列必须满足：

1. 取得 Peer EgressActor 当前状态（`ACTIVE/BLOCKING/ABORTED`），以及适用 Grant 的最新 Host PolicyFence；
2. 读取所有 Epoch 与 Lease Deadline；只有逐项仍有效且网络预算已预留时接受本次 FinalSend；
3. 在同一所属网络执行域内、不发生线程迁移/异步挂起地提交 `QSslSocket::write`，保留实际 accepted bytes 和保守 TLS 队列信用；
4. T0 使 Host 当前 Grant 进入 BLOCKING；T0 后才首次执行最终检查的旧包必须拒绝；
5. T1 要求与仍在执行的旧 FinalSend 交错操作全部结束、对应 Peer 的禁止代际对未来迟到 chunk 持久生效、旧可控应用候选清理，并完成必要 TLS abort；聚合的 T1 只等待**受影响** Peer；
6. T2 除 T1 外还要求 Qt 已开始的同步 Adapter 调用结束及按来源 held 释放。`QtFinalGate` 的输入临界区与发送临界区独立，不进行跨线程长锁。

这种合同容许 D-REVOKE=A 的 T0–T1 有界过程，但不允许旧权限的帧在 T0 后新做最终检查还能继续提交。Socket 已接收字节与已送远端的事实须区分。RFB 与 Native 共享 Host 的撤权事件，不共享写入网络 Actor；Private 模块不能因为“网络事件已排队”报告 T1。

### 4.3 Qt Surface 的“支持”不是“Qt 库存在”

| 来源 | 可作为正确性候选的前置 | 高性能路径的附加资格 | 不合格时声明 |
|---|---|---|---|
| Raster Widgets | 可在合法 GUI 绘制上下文获取完整窗口绘制内容，Popup 与敏感 overlay 事实可见 | 证明无额外强制整屏重绘且 FrameSource 可按真实 Damage 驱动 | LIMITED 或 FRAME_UNAVAILABLE，不虚构原生视频图层 |
| QOpenGLWidget | 当前 GL context/FBO 有合法读取权，Resize 与 context loss 导致资源代际变更 | 可持有同步合格的 GL 渲染完成资源，避免长期同步 glReadPixels | 只有低频读回时标 LIMITED，不宣称高 FPS |
| QQuickWindow/QuickView | 当前 RenderLoop 可取得完成的 Frame 或低频合法 grabWindow，渲染所有权明确 | 真实 QSG/RHI 完成点、资源 lifetime/fence、GPU 读回/共享路径经特定 Qt Kit 约束 | 无可呈现帧时 UNAVAILABLE，不以 sceneChanged 替代 RenderCompleted |
| QQuickWidget / Qt6 RHI Widget | 实际离屏渲染结果、接口与 Qt Kit 一致 | 充分处理额外 pass 和重建资源，防 GUI 读回拖慢 HMI | 禁用高性能宣传，保持受控兼容条件 |
| Native/video/mixed | 真实 video/native frame provider 与 Z/Alpha/Clip/DPR/遮挡信息齐备 | 完整已合成来源与资源同步已知 | 缺必需敏感内容安全覆盖则不发布远端 TargetFrame |

`NoNewFrame`（内容可能静止）、`UnchangedContent`（已有合法画面仍当前）与 `Unavailable`（无法保证画面）必须有不同 Host 状态。允许复用当前且未失效的合法内容作为首次 FULL，但不允许把旧内容标为新 ContentRevision。Qt5.15 与 Qt6.8 的实际类/后端须按 Kit/Window/Surface Profile 分别记录，没有“任何 Qt 图形后端通用高性能捕获”这种支持级别。

### 4.4 设计审查后仍然真实存在的四项客观限制

1. **同进程可信代码风险**不可由 HostPort 或设计审查消除。要求 OS 强制隔离 Native 的产品场景与 D-PROC=B 固有不相容，不能改用原来独立进程绕开 Human 裁决。
2. **任意 Qt GUI/Native Surface 全自动完整高 FPS**不存在无需条件的公共 API。对未知原生/混合画面只能限定资格或 fail closed；不能通过“后续测试”设计一个不存在的读图能力。
3. **128MiB/96MiB/512KiB 与任意多 Peer 大尺寸组合**可能有物理资源矛盾；必须在支持配置中限制在途副本、隐私派生和最大可声明并发，不能静默扩大已批准预算。
4. **源文完整性与实现真实性**未自动取得：本稿基于此前经 Human 确认正文等效的源索引和已有技术设计；正式 V15/V8/ADR 字节原件未在本轮逐字打开，故不冒称法律式原文 redline；无产品运行或技术验证证据，Gate-D 仅可提交设计评审申请。

**此处即本次三项收口的终点**：技术合同与影响范围已经写出，新增问题必须证明违反既定不变量才会改变架构，不再按功能名称无止境增加平行专项。后续是 Human 对正式内容的集中审批，以及独立授权的真实实施/支持资格，不是继续自造 V1.8。
