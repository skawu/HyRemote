# HyRemote 按用户成果推进的版本路线图（Qt LTS 优先版）

- 日期：2026-10-23。**当前生效的最新产品负责人优先级：多 Qt LTS 适配排在 HyRemoteTool 完善前面；HyRemoteTool 随多个 Feature 增量交付。**
- 范围状态：本文件属于正式的产品计划候选 / PR #600；**未激活未来精确版本号**，未写未来机器 release-trains.json，未建立 release/v 分支或空版本。下一 Feature 在符合 #24/#95 后由准入流程确定序号。该原则也解决旧草案提前把 Tool 锁在 V0.3.4、Qt LTS 推迟到 RC 的错误。
- 已发布：v0.1.0.0、v0.2.0.0、v0.2.0.1、v0.3.0.0。曾经的 0.3.1、0.3.2 列车已经 retired，不重新启用。
- **当前唯一已激活且 NOT_READY 的 Feature 是 V0.3.3.0**（#144/#175，#370 真实 Viewer 性能资格），不得将新的 Qt LTS、Tool、水印、Docker 需求偷塞进此版 mandatory。
- V0.4.0.0 RC 与 V1.0.0.0 GA 未激活、只做资格认证及稳定支持承诺。Windows x86_64 是唯一强制物理产品资格平台 #563；Linux/Wayland/ARM64/EGLFS 没有证据时标 User Validation Pending，不阻塞 Windows，但不宣称 Supported。

## 1. 从现在到 GA 的产品交付排序

| 顺序/状态 | 独立用户成果（未来准确 Feature 号在准入时确定） | 必交范围与真实验收 | Tool 增量（不得拖住本成果） |
| --- | --- | --- | --- |
| **当前 V0.3.3.0 ACTIVE / NOT_READY** | 稳定、低延迟远控与维护版 GUI Viewer 互操作 | #144 Raw/TRLE、Damage、背压、输入及连接稳定性；#175 RealVNC/TightVNC 两真实维护版 GUI Viewer；#370 同 SHA Windows P50/P95/帧年龄/CPU/RSS/光标/认证实际资格与 L2 | 保持 V0.3.0.0 已交付基础 RFB Tool 的兼容回归；不纳入新 UI 工程 |
| **下一 V0.3.x Feature：最高优先 Qt LTS** | **客户现有 Qt 5.15、6.5、6.8 应用能够按声明的合格单元接入、构建、安装和部署 HyRemote** | **#603、#57、#265、#566**：#260 preflight 已关闭；Qt6.8.3 参考版；Qt5.15.2 Windows C++/Widgets + Tool 已有限度测试，不推断其它 cell；Qt6.5 exact Windows Kit/构建/运行/消费者需实际验收。Widgets/Quick、C++/QML/Generic/QPA 各单元按真实适用范围写 Qualified/Limited/Unsupported/NOT_RUN；QPA 只按 exact Qt private ABI；不分裂 Shared Runtime、Kit 多候选不自动猜。必须有针对性的真实 Windows Qt5/Qt6.5 证据、Qt6.8 回归和安装消费者；**不能把尚未实现的 Qt5 QML/QPA 或 Qt6.5 当作 PASS** | Tool 只做被声明 Qt Kit 上的兼容性/启动回归；可并行改善语言和安全问题，但 **Tool 工作台 S1 完整验收不是 Qt LTS 的前置** |
| **随后独立 Feature 候选：可配置宿主 + 文字水印** | 用户自己的 Qt 应用可以指定配置/可写数据/未来可选 DB 路径，远程 Viewer 能看到服务端生成的文字水印 | **#591 + #589**：一个 Shared Runtime 配置入口覆盖四接入路线、受保护 secrets 引用、Docker 可挂载路径设计、显式无效配置 fail-closed；静态中文/英文文字、位置/平铺、透明度/字体；本地原图不受影响，Raw/TRLE/Damage/多 Viewer 真验证；不提前宣称已实现数据库引擎或 TLS | Tool 工作台 #581 壳/连接布局、#582 会话状态、#590 本机系统 Auto + 用户手动语言可作为**独立合格增量**在这条及其它实际可发布 Feature 中交付；不要求先完成 #583–#586 |
| **随后独立 Feature 候选：真实 Docker 业务应用部署** | Qt 应用与 HyRemote 同镜像/容器运行，用户自己选择持久化数据/策略及 secret 挂载 | **#592**：依赖真实 #591 API，验证 Dockerfile/Compose、Qt 插件/显示环境、RFB Viewer 连接、非 root、RO policy/secret、RW volume、restart/recreate 后恢复；没有实际容器证据不得发货/宣称 Docker Supported，不能阻塞独立 Windows GA 参考单元 | #583 已保存设备、#584 Fit/100%/全屏/输入坐标、#585 基础诊断可以在已有 UI/Session 接口成熟后逐步交付并各自验收；不与 Docker 技术验收耦合 |
| **后续客户成果：Tool 工作台可用性与进阶自助服务** | 不必等所有功能齐才让用户获得改善；最终形成真正成熟的 D 综合工作台 | **#581–#585 + #590** 的阶段性改进逐版可交付，**#586** 在 S1 功能全部达到可用状态时做 Windows 最终同 SHA 综合验收；**#587** 宿主诊断主动导入、**#588** SDK/Qt Kit 与 C++/QML/Generic/QPA 四路线引导，在 S1 后可互相独立并行。不能因为 S2/S3 未完成否定已发布 Tool 部分改进 | 以“当前版本真实交付哪些 Tool 能力”逐次列发行说明；不存在强制把整个 Tool S1 锁死在某一个更早 Feature 的规则 |
| **V0.4.0.0 INACTIVE RC** | 冻结已声明的 Windows GA 支持单元，执行完整资格认证 | #235/#343/#602：Qt 5.15/6.5/6.8 合格 Windows exact Kit + 四技术适用单元 + local/remote coexistence、RFB 真 Viewer/SLO、SDK安装/维护/回滚、已发货功能安全和性能、同 SHA 发布证据；Docker 的 Supported 必须是实际容器资格，缺证据只列 Unqualified/延期。**RC 不开发新 Feature。** | 只认证已经完成的 Tool 能力；不可在 RC 临时加完整 Tool 工作台作为无证据 GA 承诺 |
| **V1.0.0.0 INACTIVE GA** | 在已合格 RC 血统上作首个长期 API/兼容/安全/维护声明 | #33/#95：对应 Windows 真实 supported/limited matrix、release/v 精确标签/校验和、双语发布说明、升级/已知限制；未经实际容器验证不可宣传 Docker Supported | 将已完成、已验证的 Tool 能力按真实支持状态列出，不把尚未交付的高级面板表述为已交付 |

### 1.1 对先前错误排期的明确纠正

1. **Qt LTS 是首个 V0.3.3 后继完整成果**，不允许把 Qt5/Qt6.5 实现拖到 V0.4 RC 才开始。Qt5 limited cell 已有证据，但不代表 #57/#265 完结；Qt6.5 仍需要明确 Windows qualification。Qt6.12 仅保持探索预检，不属于首批 GA LTS 三元组。
2. **取消旧路线图 V0.3.4.0 = 全部 Tool S1、V0.3.5.0 = 水印、V0.3.6.0 = Docker、V0.3.7.0 = 全部 Tool SDK/诊断的提前精确编号分配**。它们仍是可发布成果，但在有真实实施/验收与顺序时才通过 #24/#95 分配精确 Feature。此决定修复 #600 关于抢先编号的审查 P2。
3. **HyRemoteTool D 综合工作台定位不变、S1 内部 A-first 先做远控/连接/基本诊断仍成立；变化只在跨产品的先后排序与交付节奏。** #581–#590 可在版本间按合格增量逐步落地。#586 只是 S1 整体验收，不是每个其它 Feature 的阻塞前置。S2 #587 和 S3 #588 在 S1 后互不依赖。
4. 宿主配置/文字水印、Docker 的产品需求不取消，只是 Qt LTS 首发优先；#589 TEXT-first，图片 Logo #593 **V1.0 GA 后方可激活**，不提前占 Feature。
5. Tool 的系统语言 Auto/手动覆盖 #590 仍属于 S1 产品功能，但可以在前期小 PR 与 Qt LTS 合格依赖协调后逐步发货；不要仅仅为了 UI 把多 Qt 兼容工作拖后。

## 2. RC/GA 核心边界（按现在的优先级裁定）

**最高优先、V0.4 前必需：**
- 多 Qt LTS Windows 合格兼容：#603/#57/#265/#566/Qt5+Qt6.5+Qt6.8、Qt Widgets/Quick、四种对等接入、真实安装使用、有限 Qt5/qmake 故事、QPA exact private ABI；仅声明已实际资格化的组合，不夸大完整笛卡尔积。
- #144/#175/#370 已发布 remote RFB 的 Windows 真实互操作/性能/光标/连接安全、#109 本地远程共存、API 和安装部署等已有 GA 核心基线。
- #591/#589 配置路径及服务端静态文字水印是已确认的 GA 前产品目标，按独立 Feature 资格推进；密码文件引用不代表 TLS/完整身份平台。

**Tool 采取分级 GA 范围，不再作为单个巨型 GA 前阻塞 Feature：**
- **GA 必要的最小 Tool 参考体验**：已发货的实际 Windows Viewer 只读连接/显式控制与错误状态准确、基本可用性和系统 Auto/手动中英语言（#590），不能因 UI 未美化而阻塞 Qt 兼容验收；对这一承诺必须有同 SHA Windows 真实 Tool 验证。
- **逐步增强且可按实际成熟度入 GA 支持表**：完整设备档案 #583、全屏/100% #584、进阶可导出诊断 #585/#587、SDK 辅助 #588。它们是 D 工作台规划产品功能，但**不再自动定义为 V0.4 RC 前所有都要 PASS 的单一强制门槛**；是否入 GA 声明由 #602/##33 的机器 scope 权威做准确分类，不得隐形延期或伪称已交付。
- **Docker #592** 是目标成果但不同于 Windows RC gate：必须有真实用户 Qt 应用容器运行证据才宣传 Docker Supported；目标环境不可得时不得阻塞 Windows Feature/GA，也不能假装成功。

**V1.0 GA 后候选、需独立产品/安全准入：**
- #593 Logo 图片水印仅在 **V1.0 GA 发布之后**，且文字水印通过后另行激活。
- 可信身份动态水印、完整账号/RBAC、远程主机管理面、TLS/VeNCrypt、公网/云 relay、浏览器访问、多 Viewer 管理面、多设备并发、硬件加速；均非默认 GA 阻塞，需明确价值及威胁模型。
- Linux/Wayland/ARM64/EGLFS 的无证据平台仍为 USER_VALIDATION_PENDING/UNQUALIFIED，不因支持 Qt 的 CMake 就变 Supported。

**受审范围门槛：** #602 在进入 V0.4 前梳理 #33/#235/#343 和现有机器 V1 26 项的历史/可选/长期 Issue，替换为真实可关闭的 Windows 资格单元。只在单独范围 PR 审查及机器检查通过后改 release-trains.json。此计划调整**不能**直接靠文字偷偷改变已激活的 V0.3.3 mandatory children。

## 3. 实施优先级及门禁

| 优先级 | 工作 | 责任与依赖 | 验收 |
| --- | --- | --- | --- |
| P0 当前 | V0.3.3 #144/#175/#370 | 优先完成当前真实 Windows Viewer 与同 SHA 发布证据 | 版本既定范围，不加新功能 |
| **P0 NEXT** | **Qt 5.15 / 6.5 / 6.8 LTS + Kit #603** | #260 已闭；#265 剩余 Qt5 细胞、#57 Qt6.5/矩阵、#566 显式选 Kit；与 Tool 增量接口隔离 | Qt5 Limited 不回退、Qt6.5 targeted Windows 真构建测试和实际应用、Qt6.8 回归 |
| P1 | Host 配置/文字水印 #591/#589 | 配置路径 API 与文字 frame compositor 可并行，持久化策略集成需要真实 #591 | 第三方 Viewer 看见文字、Raw/TRLE/damage、配置失败安全 |
| P1/P2 | Docker #592 | #591 实际入口是前置，未测 Docker 不发货 | Qt 用户应用容器+Volume 重建 |
| **P2 incremental** | Tool #581–#590 | 可跨以上多个 Feature 小步完成；#581/#582 先定基础，#583/#584/#585 后续；#587/#588 在 S1 后各自独立 | 每次已交付子集回归并记录，完成 S1 时做 #586 |
| P3 | Logo #593 + Management/TLS/cloud | Logo post-GA only；管理独立安全架构 | 不阻塞首轮 GA |

- Agent 主导开发与 CI；Human 只协助真实 Windows GUI/Viewer 或实际特殊 Docker 图形设备观察。**Ready PR** 最新 head Windows CI、Git Flow、独立 P1/P2 review 必须全部通过才能合并；普通无 PR 分支提交不启动 Qt Product CI。
- 每个 future Feature 的正式编号/mandatory children 只在 implementation-ready 后走 #24/#95 单独 activation，发布前有安装产物、精确候选 SHA 与 Windows 产品证据。开发合并不是发布。
- 版本计划里的 Qt 优先不意味着对 Linux/ARM64 移植问题撒谎；用户目标环境 bug 保持 Issue/hotfix 路径，非 Windows 物理 evidence 未跑时 NOT_RUN。

**关联：** #1 #24 #33 #57 #95 #144 #175 #235 #260 #265 #343 #370 #563 #566 #579 #581–#594 #602 #603。
