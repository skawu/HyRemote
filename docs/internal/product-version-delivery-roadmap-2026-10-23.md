# HyRemote 产品版本路线图：Qt 5.15 + Qt 6.8 首轮 GA，Qt 6.5 + Qt 6.12 后置

> **唯一当前跨版本产品功能排序 authority / SINGLE CURRENT PRODUCT OUTCOME ROADMAP** — #594 与 Owner #605。本文决定 **哪些用户成果在何时规划**；`docs/versioning.md` / `docs/en/versioning.md` 解释四段版本号语义；机器文件 `.github/release/release-trains.json` **独立决定已激活/退休的精确版本、mandatory_children 和候选接纳**；`docs/compatibility.md` / `docs/en/compatibility.md` 只记录已有资格的实际支持单元。已发布发行说明属于历史事实，不因新规划追改。旧 `docs/internal/development-roadmap.md` 与 `owner-execution-brief-2026-10-21.md` 为历史记录，不是并行的路线图权威。

> **检查优先级：** Owner 最新明确决定与 #594/#605 → 本文当前产品范围 → #24/#95 版本治理 → 实际机器激活/CI/精确 SHA → 实际兼容矩阵/证据。**任何 docs/Issue 的旧描述如仍称 Qt6.5 首轮 GA 必需、Linux 物理测试必需、V0.3.4 固定为整个 Tool，均为过时记录，不能执行。**

- 产品范围决定：2026-10-23。**最新 Owner 决定覆盖较早“Qt 5.15/6.5/6.8 在首轮 GA 前全部完成”的规划**：V1.0.0.0 前只要求 Qt **5.15 LTS** 与 **6.8 LTS** 的准确 Windows 合格单元；Qt **6.5 LTS** 和 **6.12 LTS** 在 **V1.0.0.0 正式 GA 后**各自推进（#605）。
- **Qt 5.15/6.8 适配优先于 HyRemoteTool 工作台完整化**。HyRemoteTool 是 D 综合工作台，A 优先的远程连接/基础诊断按小范围 PR 随多个版本逐步完善。
- 此文档是负责人**产品成果排序/版本范围决定**，不等于 V0.3.x 精确 Feature 编号已经激活。#24/#95 在实际功能 implementation-ready、独立用户价值及范围验收明确之后才分配准确编号和 release-trains.json mandatory children。**不得预占 V0.3.4～V0.3.7 的固定功能主题**。
- 历史有效发行 v0.1.0.0、v0.2.0.0、v0.2.0.1、v0.3.0.0 不追改；0.3.1/0.3.2 retired 不复活。**2026-10-23 曾公开创建 `v0.3.3.0` tag/Release，但因绕过 `release/*→main`、未完成版本更新与同一候选 SHA 的 Viewer/认证/性能验收，现已标记 RETRACTED，不能视为合格发布，也不得移动/复用该标签。** 详见 [`v0.3.3.0-release-remediation-2026-10-23.md`](v0.3.3.0-release-remediation-2026-10-23.md)。#144/#175 已关闭表示实施 Issue 状态，不能代替被撤回发行的重新准入；#370 作为长期性能治理持续开放。下一个功能版本尚未分配准确 Feature 号。**V0.4.0.0 RC 是 machine `conditional_trains` 明确 `active:false` 的未激活候选；V1.0.0.0 GA 则已作为 machine `trains` 中可选的规则配置存在，但 GA 尚未通过 #33/#95/RC 门槛，未被接受、更未发布**。`machine profile selectable` 绝不等于已获准发布或已取得 Supported 资格。
- 仅 Windows x86_64 是首轮强制真实构建/设备测试环境（#563）。Linux/Wayland/ARM64/EGLFS 为用户验证/资格未定，不能无证据宣称 Supported，也不阻断独立 Windows 成果。

## 一、交付对象与版本定义

**四种对等接入方式：** Embedded C++ API、Declarative QML API、Generic Plugin、Transparent QPA Proxy。它们共享一套 Core/Shared Runtime；Widgets 与 Qt Quick 是 UI 目标，不是新增 Runtime。Qt5.15/Qt6.8 的实际合格单元逐个按 Windows Qt Kit/CPU/编译器/Qt patch、C++/QML/Generic/QPA 路线、Widgets/Quick、安装消费者、RFB Viewer 和 QPA exact Private ABI 判断，不承诺未经实测的所有组合。

**GA 前第一轮产品交付主线：** (1) **P0 发布整改**：先为已撤回的 `v0.3.3.0` 重新建立同 SHA Windows Viewer/认证/性能/L2 证据并按 Git Flow 完成正确发行（不能复用失效标签），Raw/TRLE/Damage 实现可复用但不能直接沿用历史验收结论；(2) Qt5.15 与 Qt6.8 Windows 资格 (#603/#604)；(3) 主机配置路径与文字水印 (#591/#589)；(4) 用户应用 Docker 联合部署 (#592，实际运行才可支持)；(5) HyRemoteTool D 工作台沿多个成果逐步发布 (#581–#590)，其 SDK/进阶诊断后续增量 (#587/#588)；(6) V0.4 RC 资格冻结 → V1 GA；(7) GA 后 Qt6.5/Qt6.12 (#605) 与 Logo (#593) 等独立准入。**这里只规划功能成果，不宣布任何未来 V0.3.x Feature 已激活或其功能已完成；V1 的既存 machine train 不等于 GA 已验收或发布。**

## 二、版本与功能完整分配

| 阶段 | 用户可感知产品成果 | 必交范围与验收 | 关系/明确排除 |
| --- | --- | --- | --- |
| **V0.3.3.0 PUBLIC TAG RETRACTED / RELEASE REMEDIATION P0** | 稳定、高效的 RFB 访问与维护版 Windows GUI Viewer 互操作（候选实现存在，**正式发行未验收**） | **#144** Raw/TRLE、增量 Damage、背压/慢 Viewer、输入/画面刷新；**#175** RealVNC+TightVNC 真 GUI；**#370** 性能 P50/P95/帧年龄/CPU/RSS、光标/认证、精确同 SHA 与 L2 | **不添加** Qt LTS 多版本、Tool 重构、水印、Docker 新 mandatory |
| **下一个 V0.3.x Feature，编号待准入；P0 NEXT** | **主流 Qt 5.15 LTS + Qt 6.8 LTS** 的 Windows SDK/Runtime 实用兼容 | **#603、#57、#265、#566、#604**：Qt5.15.2 Windows C++/Widgets + Tool 已有**有限资格**，剩余 Qt5 Qt Quick/QML/Generic/QPA 按实际可适配+可验收单元决定；Qt6.8.3 作为 Windows 参考及回归。跨 C++/QML/Generic/QPA 的一个 Shared Runtime、SDK 安装、CMake/qmake 消费者、Qt Kit/version+compiler+arch 显式选择、QPA exact Qt Private ABI 及真实 Viewer 验收；未测 Unsupported/NOT_RUN 不冒称 PASS | **Qt 6.5、Qt 6.12 明确不属于本 Feature 或首轮 GA**，跟踪 #605；Tool 整套 S1 完成不阻塞 Qt 5.15/6.8 |
| **后续独立 V0.3.x Feature 候选** | **宿主配置/持久化路径 + 服务端文字水印** | **#591 + #589**：四条入口共用 config_file、可写 state_dir、未来可选 DB 路径及受保护 secret 引用；可配 UTF-8 中文/英文静态文字、透明度/字号/颜色/位置/平铺；原应用本地像素不变，标准 Viewer 看得见；Raw/TRLE/Damage/Resize/多 Viewer 性能真实验证 | 不提前实现 SQLite 引擎或 TLS/账号平台；Logo #593 在 GA 后 |
| **后续独立 V0.3.x Feature 候选** | **用户自己的 Qt App 与 HyRemote Docker 联合部署** | **#592**：依赖可运行 #591 配置/数据 API，真实 Dockerfile/Compose + Qt 插件/显示后端、RFB 连通、非 root、只读配置/密钥、可写 volume、restart/recreate 后持久化 | 没跑真实 Qt 应用容器不能宣称 Docker Supported；不因无法取得 Linux 设备阻断已合格的 Windows Features |
| **横跨多个独立 Feature 的 Tool 增量** | D 综合工作台逐步从已发布的基础 Viewer 升级到现场操作/诊断/开发自助工具 | **#581** 实用 UI、**#582** Session/默认只读/控制安全、**#590** 本机系统 Auto 语言+设置手动持久化；**#583** 保存设备、**#584** Fit/100%/全屏、**#585** 基础诊断；全部成熟后由 **#586** 对 S1 形成同 SHA Windows 总体验收；**#587** 导入宿主诊断与 **#588** SDK/Qt Kit 四路线指导都接在 S1 之后，可并行 | 不把整套 Tool S1 或高级 SDK/诊断加入首个 Qt 适配 Feature 的 mandatory；每次 Feature 发行说明按已合格 Tool 子功能陈述 |
| **V0.4.0.0 RC（未激活）** | 冻结**Qt 5.15 与 6.8** 的真实 Windows 支持矩阵，全面验证已交付产品 | #235/#343/#602：Qt5.15 与 Qt6.8 的适用 Windows exact Kit/Widgets/Quick/四路线/Private ABI、外部 GUI Viewer、性能/安全/SDK 安装/升级回滚、Tool 已发货能力与文字水印/路径等声明，同 SHA 发布闭环 | **不要求 Qt 6.5/6.12 的任何适配、兼容性或真实 Qt Kit PASS**；RC 不开发新 Feature。Docker 如未实测只能标 Unqualified |
| **V1.0.0.0 GA（机器规则已列出；NOT_READY／未发布）** | 仅提升合格 V0.4 RC 的 Windows 产品资格为首个稳定支持承诺 | #33/#95：**正式 Qt 家族只覆盖已实际资格化的 Qt 5.15/Qt 6.8 单元**，C++/QML/Generic/QPA 支持单元清单、API/包、二语说明、tag/checksums/维护声明；Tool 功能按实际验收声明 | 不暗含 Qt 6.5/6.12 支持；不发布未经实际验证的 Docker/Linux cell |
| **V1.0.0.0 之后，未来独立 Feature（均未编号）** | **Qt 6.5 LTS 及 Qt 6.12 LTS 扩展支持** | **#605**：各自选择确切 patch+compiler Kit，Windows SDK/Widgets/Quick/四路线中可支持组合与 QPA exact ABI，实际安装、连接/输入与性能回归，按真实证据声明 Supported/Limited/Unsupported | **不预留 V1.1/1.2 等编号**，6.5 与 6.12 可分成不同 Feature；在 V1 GA 前不要求真实运行/消费者/外部 Viewer 支持认证 |
| **V1.0 GA 后其它候选** | 图片 Logo 水印与更高阶安全/管理能力 | **#593** Logo 在文字水印合格且 GA 完成后独立审议；管理/RBAC/TLS/云/浏览器/多设备并发等需要单独价值、安全、Threat Model 评审 | 以上均非首轮 V1.0.0.0 GA mandatory |

## 三、Qt 家族资格矩阵：首轮强制 vs 后续

| Qt 家族 | V1 GA 目标范围 | 目前真实证据与约束 | 版本管理 |
| --- | --- | --- | --- |
| **Qt 5.15 LTS** | **必须在 V0.4 RC 前按适用声明真实合格** | Qt5.15.2 Windows C++/Widgets + Tool 有有限证据（#569/#576）；其余 route/Quick/private QPA 要逐 cell 证明，不因 Qt 大版本符合就全盘通过 | 下一个 V0.3.x 客户成果的重点 |
| **Qt 6.8 LTS** | **必须在 V0.4 RC 前按适用声明真实合格** | Qt6.8.3 Windows 参考路径，必须保持 SDK/Viewer/Raw/TRLE、各适用 route 回归与 exact Qt Private ABI | 当前参考/下一个成果回归 |
| **Qt 6.5 LTS** | **POST_GA_ONLY** | 以前 preflight/Issue 声称“必需”已被 Owner 更新的版本范围取代；不能由历史 preflight 记录推断已验证 | #605，GA 后独立资格 |
| **Qt 6.12 LTS** | **POST_GA_ONLY** | 可以记录探索性兼容性调查，但不加入首轮 GA 必需支持矩阵，也不要求当前 Windows 真实 Kit | #605，GA 后独立资格 |

**特别强调 build / Kit UX（#566/#604）：** 已合并的 PR 568 在 Qt Kit 自动发现中有过宽的 `Qt6>=6.8` 条件，错误地屏蔽已经部分资格化的 Qt5.15；下一个 Feature 必须使 **实际 Qt5 可构建的 Kit/组件**被准确提供、选择和验证。**不要求首轮让 Qt6.5 成为可选合格 Kit**；Qt6.5/6.12 检出时可注明「未来计划/尚未资格化」，不可误标 Supported。必须同时保持 Qt6.8.3 和 QPA exact ABI 的严格路径约束。Kit 发现仅表示 *candidate*，不是发布支持证据。

## 四、GA 前其它产品功能与依赖

- **RFB 核心不可回退**：#144/#175/#370 的 Raw/TRLE/Damage、输入键鼠、重连/光标、维护版 Windows GUI Viewer、慢客户端与性能、安全状态、Qt 本地/远程共存 #109。
- **四种 SDK 接入方式共用 Shared Runtime**；Windows GA 以 Qt5.15 和 Qt6.8 真正可支持的路由单元验收，QPA exact patch + private ABI，不把 Qt5 实现难度虚构为全通过。
- **配置/水印为 GA 前独立产品目标**：#591 + #589，首批**静态文字水印**；宿主与 Tool 本机设置分离；数据库仅预留可配置路径、不因为未来认证而强制引入数据库或明文密码。
- **Docker #592**：产品要有真实用户 Qt 应用容器示范；是否可以在首轮 GA 对外标 Docker Supported 由其真实容器证据决定。缺验证时不能偷给 PASS，也不阻断其它已完成 Windows 能力。
- **HyRemoteTool 持续演进**：安全只读/显式控制与本机 OS 自动/手动语言是 GA 最小产品目标；设备档案、缩放/全屏、诊断、SDK 集成按各自成熟度逐版交付，并明确标记功能完成/未完成。#586 S1 总体验收不是 Qt5/Qt6.8 版本资格的前置。
- **机器规则的第一步已经通过 [PR 606](https://github.com/skawu/HyRemote/pull/606) 合并到 develop**：`.github/release/release-trains.json` 的 #57/V1 规则现仅要求 Windows Qt5.15/Qt6.8，明确 Qt6.5/Qt6.12 为 GA 后，且 active V0.3.3 mandatory [144,175] 不变。此项是已完成的**即时 Qt 家族范围修正**，不是后续才由 #602 接手的工作。
- **RC/GA 其余机器范围整理仍由 #602 负责**：盘点历史 V1 machine 26 条 mandatory/退休 umbrella、拆成可关闭的 Windows GA 验收，再单独评审；不能拿本路线图代替机器范围权威，#57/#343/#33/#235 的 Owner 最新 Qt 家族限定已经生效。

## 五、跨版本长期责任与后续产品候选（持续维护，不预占 Feature）

**长期工作不等于“下一版本新功能”，更不能挤占当前 Windows Qt5.15/6.8 首轮 GA 已核准的发布范围。** 本章是长期技术债、稳定支持、用户反馈以及后续可选能力的统一可见性索引，**不是第二套机器 mandatory_children 或预分配 V1.1/V1.2 的新路线图**。所有具体工作依旧由各 Issue 负责；实际发布资格仍由 #24/#95、Owner 决策及 `release-trains.json` 规定。

| 类别 | 跨版本长期责任或未来用户成果 | 事实源 / 准入原则 |
| --- | --- | --- |
| **CPU / 资源效率与长期性能** | 主动构建真实 Qt Host 与 Viewer 分离的 CPU/RSS/功耗（有证据时）及 W1–W4 交互/Frame Age 基线，修正性能计时、解决测得的采集/编码/重复拷贝热点；后续每次性能敏感变更防回退 | **#370/#374/#609/#9**：CPU 是已观察问题，**现在主动测量与可移植优化**，不是等待客户投诉；GPU/低拷贝仍需 #123 有证据 GO |
| **弱网退化与长时间可靠性** | 保持 Raw/压缩、准确累积 damage、latest-frame/backpressure、慢 viewer 隔离、本地 Qt 活性、超时/重连输入清理；执行有界运行、关闭重启和异常路径回归 | **#370/#371/#372/#144/#158/#164/#109**：**基础正确性现在维护**；极端弱网 FPS/SLA/特殊 codec 按用户反馈专项，不设置新的首轮 GA 弱网门槛 |
| **安全报告、供应链与支持生命周期** | 实际版本/支持单元声明，公开安全边界的真实性，私密漏洞报告与补丁/披露流程、Qt private ABI/依赖/许可证及安装包清单维护；视具体需要决定是否加 SBOM/溯源制品 | **#611（新增持久责任设置）**，现有 **#12/#143/#158/#95/#101/#107/#176**。安全缺陷必须及时修复；TLS/RBAC/云服务不是自动首轮 GA 条件；现有 `SECURITY.md`/依赖文档的陈旧版本文字纳入 #611 修正 |
| **1.x API/Kit/升级维护** | 公共 C++/QML/CMake/Generic 稳定性、Windows Qt 补丁/编译器及 QPA 精确 private ABI，独立安装消费者、版本升级/回滚和 release/hotfix 证据 | **#101/#57/#603/#604/#209/#176/#95**；Qt6.5/Qt6.12 **严格 GA 后 #605**；每个 qualified cell 才可声明 Supported |
| **用户问题闭环与可观测性** | 错误分类、脱敏诊断、可复现步骤、精确 SHA/Qt Kit/Viewer/OS 归档、历史性能对比；避免默认收集用户屏幕/输入/秘密或打造第二套远程遥测平台 | 当前基础 **#335/#585**，进阶 **#180/#587**；真实运维阻塞或可复现缺陷驱动最小能力，持续回归 **#374** |
| **SDK 生态与真实应用覆盖** | 四条对等接入路线、文档示例、复杂第三方 Qt 应用 Widgets/Quick/弹窗/输入、clean-rootfs/安装包及真实容器的事实性兼容声明 | **#41/#134/#343/#176/#592**；Windows GA 对既定矩阵负责；Linux/Wayland/ARM64/EGLFS **#236/#346** 用户验证，不隐含预先 Supported |
| **现场 Viewer 与操作能力（候选）** | 无安装权限电脑如何访问、便携/浏览器方案的真实企业场景；许可的文本剪贴板、会话/控制权移交、多个 Viewer 的性能与准入 | **#204/#171/#170/#205/#225**：已有需求/候选，但安全隐私/维护 Viewer 验证及用户价值独立评审，不构成当前 GA 必选 |
| **网络外访问与管理能力（候选）** | 外向连接的 Edge Connector、可自托管 Gateway、必要认证审计与安全、企业运维；与 LAN 直连共存 | **#190**；须独立 Threat Model、运维/部署成本和资格矩阵才可激活；不是升级现有 RFB 自动获得的互联网安全能力 |
| **工程可持续性** | Git Flow、可重复 CI/真实 L2、独立评审、性能回归、依赖更新、旧 Issue/历史 docs 清理、适用时的 ADS 通用化而不改变产品架构 | **#95/#209/#374/#393/#602/#14**；治理工作贯穿各版本，不按技术工作项机械占用 Feature 号 |

**长期候选激活判断：** 有明确的用户场景或可复现的产品缺陷 → 确定当前 SDK/Runtime 能力是否足够 → 精确设备、Qt Kit、Viewer、SHA、网络/性能/安全证据 → 独立评估用户价值、互操作、安全与维护成本 → #24/#95 决定 GO/NO-GO 和准确 Feature 号；没有足够证据则保留条件候选而非变成 GA 阻塞。**协议正确性、内存/CPU 无界及安全缺陷是当前已有质量责任，不得借此规则放到用户投诉后再修复。**

## 六、执行次序、审查与风险约束

1. **当前最高优先级：已撤回发行整改**。V0.3.3.0 的 `v0.3.3.0` 历史标签/Release 被标记 RETRACTED，绝不是可推广的正式发行；原 `3a65842` Windows 83/83 记录与 Viewer、认证、性能并非统一已接受的同一 SHA。按整改记录重新冻结候选、统一重测、通过 `release/*→main` 与最新独立审查/L2/标记策略完成合规发行，**不得复用已撤回标签**。#144/#175 的 closed 状态不能代替发行门禁；#370 仍是跨版本性能回归总纲。
2. **当前 P0（下一独立 Feature，未编号）**：#603/#57/#265 + Kit 修复 #566/#604，只验证/承诺 **Qt5.15 与 Qt6.8**，解决 build/install/Kit/四路线适用性；不要求 Qt6.5/6.12 的真实支持。
3. **P1**：#591 配置/持久化契约 + #589 服务端文字水印；#592 Docker 依赖已实现的宿主配置路径，可按真实环境再激活。
4. **P2 iterative**：Tool #581–#590 的基础界面、设备、语言、控制、安全与基本诊断跨多个 Feature 增量交付；#587/#588 在成熟 S1 后并行。不要让完整 Tool UI 拖住 Qt5.15/Qt6.8 接入。
5. **V0.4 RC → V1 GA**：只对 frozen Windows **Qt5.15 / Qt6.8** 和已实际完成的其它产品能力资格化，不开发新功能；真实包/发行说明/标签/回滚/支持矩阵。
6. **V1.0 GA 完成后**：#605 才能正式激活 Qt6.5 和 Qt6.12 的适配 Feature；#593 Logo 和其它安全/管理扩展单独审议。

所有代码 PR 必须在 Ready PR 最新 head 的适用 Windows CI、Git Flow 和独立 P1/P2 Review **PASS 后**才能合并。没有物理 Windows/Container 实证的支持 claim 标记 NOT_RUN/UNQUALIFIED；无 Linux/ARM64 硬阻塞 #563。发布版本只能按 #24/#95 的精确 SHA、release/v、L2、tag/checksum、backmerge 过程完成。

**关联**：#1 #24 #33 #57 #95 #144 #175 #235 #260 #265 #343 #370 #563 #566 #579 #581–#594 #602 #603 #604 #605。此文件是产品规划与未来准入依据，不是功能已交付、GA 发布或新的精确 Feature 激活记录。
