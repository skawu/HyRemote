# HyRemote 产品版本与用户功能对应路线图（负责人规划决定）

- 日期：2026-10-23。计划基线：develop@7e27020766aafada705e1f51d5877a08c2b078ba。
- **决策性质：产品成果、顺序和目标范围已规划。V0.3.4.0—V0.3.7.0 为有条件目标标签 TARGET_NOT_ACTIVATED，不是已获 #24/#95 准入的发布列车、已经实现的功能或不可变的 Feature 编号预约。**
- 当前唯一活跃且未发布的 Feature 是 **V0.3.3.0**，machine mandatory 为 #144/#175；#370 的 Windows 真实 Viewer 验收仍未闭环。V0.3.0.0 已发布，V0.4 RC 和 V1 GA 未激活。
- 产品决策已确认：HyRemoteTool **D 综合工作台、S1 首先选择 A**；Tool 按本机 OS 自动选择中文/英文并允许持久化手动覆盖；Shared Runtime **文字水印先、Logo 后**；用户业务 Qt 应用可选本地配置/持久化路径，需支持真实 Docker 联合部署。
- 无自动升级旧 release tag；Linux/ARM64/EGLFS 的无实机证据不能阻塞 Windows 强制资格（#563），也不能反过来宣称 Docker/Linux 已经 Supported。

## 1. 版本目标与完整交付成果

| 目标版本/状态 | 用户拿到的完整产品成果 | 必需范围（功能 + 真正的验收） | 不包含 |
| --- | --- | --- | --- |
| **V0.3.3.0 ACTIVE / NOT_READY** | 正常局域网下维护版 Windows Viewer 能高效、稳定地查看和操作 Qt 应用 | #144 Raw/TRLE、增量/Damage、慢 Viewer 限流，#175 至少两种真正 Windows 维护版 GUI Viewer；#370 真 Viewer 输入到可见/帧年龄/CPU/RSS/网络性能、Cursor/认证、同一候选 SHA 的 L2 | Tool D 工作台、水印、Docker、SDK 助手均不得增入当前必需清单 |
| **V0.3.4.0 TARGET_NOT_ACTIVATED** | **HyRemoteTool 实用远程工作台**：保存设备、连接、只读、请求控制、重连、缩放、诊断一条完整用户闭环 | S1 #581–#586 + **#590** 本机系统 Locale Auto/设置手动优先/重启保持；Windows Qt GUI 与干净安装、100/125/150% DPI、800×600、键鼠释放/只读安全、受控诊断报告 | 不依赖文字水印、Docker、Logo、完整 SDK 集成、远程管理；不能让未来模块阻塞本成果 |
| **V0.3.5.0 TARGET_NOT_ACTIVATED** | **可配置 Host + 服务端文字水印**：用户在自己的 Qt 应用中选择配置/数据位置、给远程画面打可靠文字水印 | **#591** 一个 Shared Runtime 的非秘密 config_file / 可写 state_dir / 未来可选 DB 路径 / protected secret ref，四个接入方式共用；**#589** UTF-8 静态文字/字号/颜色/透明度/位置或平铺合成；本地应用原画面不变，标准外部 RFB Viewer 可见，Raw/TRLE/Damage/多 Viewer/性能回归，Windows 真实资格 | 不包含 Logo #593、动态用户身份、已经实现的数据库引擎、加密/TLS 或主机远程策略面；Docker 可作为接入试验，但不强绑为本版交付 |
| **V0.3.6.0 TARGET_NOT_ACTIVATED** | **业务 Qt 应用可重复 Docker 部署**：配置和用户数据可被用户选择并持久化，重建后仍可用 | **#592** 使用已交付 #591 公开路径 API，真实 Qt 应用 Dockerfile/Compose、Qt 插件/运行闭包、RFB 监听/Viewer 连通、非 root、只读 policy/secrets、可写 Volume、重启/重建恢复；必须有可复现容器执行证据 | 不是第二 HyRemote daemon；不虚报所有 Linux 显示/GPU 组合 Supported。如果只有文字没有可运行容器，不能宣称 V0.3.6 用户成果已交付 |
| **V0.3.7.0 TARGET_NOT_ACTIVATED** | **开发者集成与专业支持工作台**：Tool 的 SDK 引导和故障证据闭环 | **#588** 用户自己选 SDK/Qt Kit、C++/QML/Generic/QPA 正确兼容 cell 与可复制步骤；**#587** 用户主动导入宿主诊断并与客户端实测清晰分列、脱敏；两项都以 S1 为基础，但 **S2 与 S3 彼此可并行，互不等待**。Windows 真安装消费者/CLI/Qt Kit 验收 | 不擅自修改用户项目、不自动选择多个 Qt Kit、不假装可管理远程 Runtime、无旁路身份验证 |
| **V0.4.0.0 INACTIVE / RC** | 冻结的 GA Windows 参考产品矩阵获得资格验证 | #235/#343 已宣布 Windows Qt5.15/Qt6.5/Qt6.8 的准确适用 cell、Widgets/Quick、四种对等技术及 QPA 私有 ABI、安装/升级/回滚、RealVNC/TightVNC、真实性能/安全、已交付 Tool/水印/部署能力逐项判定与同 SHA 证据 | 不在 RC 新增 Feature；Linux/ARM64/EGLFS 无物理设备不阻塞，不能凭 Windows 结果宣称非 Windows Supported |
| **V1.0.0.0 INACTIVE / GA** | 在合格 RC 线路上作长期兼容、支持、安装与安全声明 | #33/#95 接受同一 RC lineage，发布中英双语说明、校验和、标签与真支持矩阵；对 Tool/文字水印/持久化及 Docker 的支持状态按真实资格公开 | 不临时加入 Logo、管理平台、TLS/云 relay，也不把未验证 Docker 标为正式 Supported |

### 1.1 目标编号并非预占空槽

本文锁定**主题和预期归属**，但是尚未激活 V0.3.4.0—V0.3.7.0。按照 #24/#95，只有实际成果 implementation-ready、可独立交付且获得产品范围准入时，才在单独的 authority Issue 和 machine release-trains.json 中绑定**准确**的版本编号、可关闭的 mandatory children、验收清单和候选分支。若成果成熟顺序改变，后续目标标签随之重算，**不得发布空 Feature，也不得让未完工的计划编号阻塞已完成的真实用户成果**。既有 V0.3.3.0 的 scope 和 L2 流程保持原样。

## 2. GA 前应实现的产品内容：负责人的明确优先级判定

**PRE_RC_TARGET_REQUIRED（在进入 RC 前须具备实用功能与 Windows 资格，若要取消须明确审核范围与支持承诺）：**

1. **Windows 核心 GA 既有基线**：#343/#57/#265 中明确的 Windows Qt5/Qt6 reference/limited 兼容、Widgets/Quick/C++/QML/Generic/QPA 准确 cell；#144/#175/#370 的画面传输、维护版 Viewer、真实性能；基本有效可选 VNC 认证与安全事实，不把 VNC Auth 等同 TLS。
2. **第一方 HyRemoteTool 真正可用**：#581–#586 + #590（S1），用户可从连接到操作与诊断而无需打开开发调试窗口。必须是干净安装 Windows 真实可见、可操作，不把 469 行旧演示窗口冒充整体产品。
3. **配置路径与文字水印**：#591 + #589。一个 Shared Runtime + 四种对等接入，客户可以选择其业务进程需要的配置/secret/state 路径，看到真实的服务端静态文字水印；无 Logo、无动态认证身份或管理面。
4. **Tool 进阶支持与 SDK 自助闭环**：#587/#588 可在 S1 后并行完成，但要对已经声明的 Windows Qt Kit/SDK 实际消费者做验证，不能错报 Qt5 Limited、Qt6.8.3 或 QPA exact ABI。

**Docker #592：PRE_RC_PLANNED_WITH_QUALIFICATION（有明确交付意图，支持声明须独立验证）：**
- 计划作为 V0.3.6 的可独立发布成果，实际 Docker 业务应用必须连通 Viewer，配置和状态挂载、非 root/安全边界、重建可重现，才能把 Docker 写成 Supported。
- Windows-first #563 不要求 Linux/ARM64 物理主机成为其它版本或 Windows GA 的必需门槛。若容器环境拿不到且 #592 没有证据，**不发布/不声称 Docker Supported，不阻塞已经通过的 Windows 成果**；必须在 #33/#235 Scope 冻结时以明确的支持矩阵决策将该 target 延期或列为 Unqualified，而不是默默跳过。

**POST_GA_ONLY（Logo 明确只在 V1.0 GA 之后激活，不进入任何 V0.3.x/V0.4/V1.0 scope）：**
- **#593 Logo 图片水印：只有 V1.0.0.0 已完成 GA 且 #589 文字水印合格之后，才可另行提出、审批并激活 Logo Feature**。格式、存储、图文组合与性能需要独立验收；不预留固定 V1.1.x 号，也不能以另立一个早期 authority 绕过 post-GA 顺序。

**SEPARATE_AUTHORITY_NOT_GA_REQUIRED（其他增强需单独产品/安全授权，不构成当前 GA 强制范围）：**
- 每 Viewer 动态身份水印、企业账号/RBAC、远程主机启停/配置修改与独立 Management Plane、TLS/VeNCrypt/不可信公网访问、云 relay、浏览器/多设备并发、厂商硬件加速；均需新的安全/产品独立审议。不能因为旧 #378 有历史 Host 控制台文案就算已经批准。
- Linux Desktop/Wayland、ARM64/EGLFS 目前按实际 Unsupported/Experimental/User-validation pending 分类，不能用 Qt 可移植性推出经过实际资格认证的 Supported 状态。

**重要治理点：** 此处批准的是*产品目标*。现有 V1.0 release-trains.json 仍列有 26 个历史 mandatory children，其中包括可选/旧 umbrella；在真正激活 V0.4 前必须以单独 scope-authority PR 重新映射成可关闭的 GA Windows 验收单元，不能直接向那 26 项上盲加本表所有总纲 Issue，也不能任意删除既有有效 GA 义务。#33/#235 的有效版本机制仍为最终权威。

## 3. 交付顺序与并行关系

| 优先度 | 轨道 | 依赖与是否可并行 | 验收依据 |
| --- | --- | --- | --- |
| **P0** | V0.3.3 真实 Viewer + 性能/候选 | 当前发布收口，不能增加新功能 scope | #144/#175/#370，Windows 同 SHA L2 |
| **P1** | Tool S1 + OS 自动/手动语言 | #581/#582 先定义壳/状态；#583/#584/#585/#590 可在接口固定后并行；#586 集成闭环 | Windows Qt6.8.3 真实安装/GUI/安全操控 |
| **P1** | Runtime 宿主配置与文字水印 | #591 与 #589 组合器可并行开发，**持久化策略接入依赖 #591 的真实 API** | RFB 真实 Viewer 文本、Damage/编码/性能/路径权限 |
| **P2** | 真实 Qt 应用 Docker | 基于已实现的 #591，水印持久化回归在 #589 可用后进行 | 实测容器、挂载、权限、重启/重建，未测不宣称 PASS |
| **P2** | Tool SDK 集成与进阶诊断 | #587 和 #588 都等 S1 用户成果，**互不依赖，可以并行** | Windows 本地 SDK 消费者及实际导入/脱敏 |
| **P3** | Logo、安全 Management/TLS 与其他增强 | Logo 等文字水印资格完成；管理需独立 Threat Model | 不属于当前 Windows GA 首批强制范围 |

普通 feature 分支提交不引发完整 Qt CI；Ready PR 最新 head Windows CI、Git Flow、独立 P1/P2 审查**全 PASS 后才允许合并**。develop 合并不是发布。发布仍严格经 exact release/v、L2、release notes、main/tag/checksums/backmerge（#95）。

## 4. 版本激活时的动作与监督

1. #594 跟踪**这份已批准的目标归属**，不是代替真正 release authority；更新 #95/#1，定期检查每个目标 Issue/PR/真实 Windows/Container 证据。
2. 当一个用户成果符合 #24/#95 单独 Feature 准入，在独立 Issue 中明确版本唯一用户 promise、Done/Out-of-scope、可关闭 mandatory children、支持 cell 和同 SHA 测试，才审查 release-trains.json 并激活准确 Vx.y.z.w；若实际交付顺序改变，更新计划编号而非建立空槽。
3. 任何 Windows-only GA 被承诺的功能未完成时，不可静默默认进入 RC；必须在 #33/#235 中明示产品负责人批准的范围重裁定。Docker 若未实测不可假装支持，也不可因为没 Linux 板卡而阻断其它 Windows 功能。
4. 独立设计 PR #580 的 D+A 和文字水印 / 路径 RFC 在未合并前仍是 PR 设计草案，本路线图不代表那些代码已完成或已进入 develop。
5. 过往 v0.3.0.0 发布事实不追改；后续实测发现的问题使用 Issue/维护 hotfix 线修复，不能无证据声称 Windows/Linux PASS。

关联：#1、#24、#33、#57、#95、#144、#175、#235、#343、#370、#563、#573、#579、#580、#581—#593、#594。
