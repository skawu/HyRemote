# W00｜V8 99 个二级架构小节 → D1–D5 合同审签责任索引（设计候选）

> 来源：Human 确认正文与正式批准 V8 一样的完整 `HYR-ARCH-001/V8` 送审稿，实际 SHA 及附录核验见 [W00 源审计](module-detail-source-audit-v15-v8-2026-11-05.md)。本索引**不声称替代 V8 原文**，仅将 99 个 Word Heading2 小节全部登记到已存在的 D1–D5 模块责任范围。**不是** 99 项接口/代码/证据 PASS。

## 1. 实物来源完整性与受控边界

- 主体 V8 第 1–16 章共 **90** 个二级节；附录 A 六节、附录 B 三节，共 **99** 个 Heading2。附录 C–F 无二级 Heading2，但其中 C 资源/故障表、D 的 V15 56 条对应、E 基本约束和 F 术语同样是**完整原文约束**，不能因没有 Heading2 而漏审。
- 99 行在下表按 `章号.节号` 唯一索引，标题是原文，责任模块取自已有 D1/D2/D3/D4/D5 候选结构，不新建 Core/Runtime/Gateway 平行体系。列中“需审签行为与失败焦点”是**设计审查摘要**，不同于正式源文全部原子义务。
- 状态统一 `INDEXED_CONTRACT_REVIEW_PENDING`；不将源结构索引等同逐项消息/状态/测试/Owner 完整检查。主文档 [V15 的 56 项追踪](module-detail-w00-v15-behavior-trace-2026-11-05.md) 与 Native [21 类消息差距/测试向量计划](module-detail-w00-native-wire-contract-gaps-2026-11-05.md) 单独交叉引用。
- **非 Heading2 附录必审**：C（内存预算/故障矩阵）、D（56/56 主责章核对通过）、E（架构约束总表）、F（核心术语）。本轮索引覆盖其文档存在性，不把它们错误计为“已经逐条验证”。

## 2. V8 99 个二级条款逐一归属

| V8 规范小节 | 原文标题 | 负责详细设计包 | 必审合同/失败焦点（审核摘要） | 状态 |
| --- | --- | --- | --- | --- |
| V8 1.1 | 文档目的与范围 | D5 W00; D4 X-02 | V15/V8 权威与安全边界，不越权改上位范围 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 1.2 | 规范依据 | D5 W00; D4 X-02 | V15/V8 权威与安全边界，不越权改上位范围 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 1.3 | 产品边界与原则 | D5 W00; D4 X-02 | V15/V8 权威与安全边界，不越权改上位范围 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 1.4 | 能力范围 | D5 W00; D4 X-02 | V15/V8 权威与安全边界，不越权改上位范围 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 2.1 | 产品组成与用户 | D1 F-02; D2 V-03; D3 E-01/G-01; D4 T-01 | Framework/View/Tool/Gateway 职责隔离与宿主业务确认 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 2.2 | 组件职责划分 | D1 F-02; D2 V-03; D3 E-01/G-01; D4 T-01 | Framework/View/Tool/Gateway 职责隔离与宿主业务确认 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 2.3 | 宿主业务职责 | D1 F-02; D2 V-03; D3 E-01/G-01; D4 T-01 | Framework/View/Tool/Gateway 职责隔离与宿主业务确认 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 2.4 | 直接连接与企业服务 | D1 F-02; D2 V-03; D3 E-01/G-01; D4 T-01 | Framework/View/Tool/Gateway 职责隔离与宿主业务确认 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 3.1 | 总体分层与依赖 | D1 F-01/F-02/F-03; D3 E-01 | 四接入一运行时、Core 不依赖 Qt UI/具体协议、Edge 不入 HMI Core | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 3.2 | 核心层职责 | D1 F-01/F-02/F-03; D3 E-01 | 四接入一运行时、Core 不依赖 Qt UI/具体协议、Edge 不入 HMI Core | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 3.3 | 共享运行时职责 | D1 F-01/F-02/F-03; D3 E-01 | 四接入一运行时、Core 不依赖 Qt UI/具体协议、Edge 不入 HMI Core | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 3.4 | 界面目标适配 | D1 F-01/F-02/F-03; D3 E-01 | 四接入一运行时、Core 不依赖 Qt UI/具体协议、Edge 不入 HMI Core | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 3.5 | 私有接口依赖 | D1 F-01/F-02/F-03; D3 E-01 | QtPrivate 受控例外、public ABI 不泄漏、具体 Kit 实机证据 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 3.6 | 工厂边缘接入与客户端职责 | D1 F-01/F-02/F-03; D3 E-01 | 四接入一运行时、Core 不依赖 Qt UI/具体协议、Edge 不入 HMI Core | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 4.1 | 编程式接口接入 | D1 F-03/F-06; D4 X-01 | 四入口同部署策略，QPA 精确 Kit 与 RFB/Native/Dual 准入 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 4.2 | 声明式接口接入 | D1 F-03/F-06; D4 X-01 | 四入口同部署策略，QPA 精确 Kit 与 RFB/Native/Dual 准入 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 4.3 | 通用插件接入 | D1 F-03/F-06; D4 X-01 | 四入口同部署策略，QPA 精确 Kit 与 RFB/Native/Dual 准入 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 4.4 | 平台插件接入 | D1 F-03/F-06; D4 X-01 | 四入口同部署策略，QPA 精确 Kit 与 RFB/Native/Dual 准入 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 4.5 | 协议构建与配置 | D1 F-03/F-06; D4 X-01 | RFB-only / Native-only / Dual 三模式；禁用全部时远控 Runtime 构建失败 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 4.6 | 部署配置与启动 | D1 F-03/F-06; D4 X-01 | 四入口同部署策略，QPA 精确 Kit 与 RFB/Native/Dual 准入 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 5.1 | 目标与会话模型 | D1 F-02/F-06; D2 V-03 | Target/Session/Scope/Grant 区分、控制独占与现场撤权 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 5.2 | 身份与权限关系 | D1 F-02/F-06; D2 V-03 | Target/Session/Scope/Grant 区分、控制独占与现场撤权 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 5.3 | 控制申请与独占 | D1 F-02/F-06; D2 V-03 | Target/Session/Scope/Grant 区分、控制独占与现场撤权 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 5.4 | 授权接口与策略版本 | D1 F-02/F-06; D2 V-03 | 不可变 PolicySnapshot 原子生效，旧 revision 不能覆盖 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 5.5 | 撤销与现场接管 | D1 F-02/F-06; D2 V-03 | 先撤权栅栏→清未执行输入/待发帧→通知，业务结果不能逆向撤销 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 6.1 | 远程目标与采集范围 | D1 F-02/F-06; D2 V-02 | 共享底帧、逐会话隐私衍生、撤权后可控缓冲失效 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 6.2 | 画面采集与更新 | D1 F-02/F-06; D2 V-02 | 共享底帧、逐会话隐私衍生、撤权后可控缓冲失效 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 6.3 | 会话隐私处理 | D1 F-02/F-06; D2 V-02 | 共享底帧、逐会话隐私衍生、撤权后可控缓冲失效 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 6.4 | 授权变化与缓存清理 | D1 F-02/F-06; D2 V-02 | 旧 PolicyRevision 待发帧失效；已入 socket 不能取消则受控断链 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 6.5 | 画面变换与断线显示 | D1 F-02/F-06; D2 V-02 | 共享底帧、逐会话隐私衍生、撤权后可控缓冲失效 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 7.1 | 输入事件归一化 | D1 F-06; D2 V-02 | 事件归一化、双重准入、跨来源 held 状态与 Qt 最终栅栏 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 7.2 | 输入接纳与提交 | D1 F-06; D2 V-02 | 事件归一化、双重准入、跨来源 held 状态与 Qt 最终栅栏 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 7.3 | 多来源输入协调 | D1 F-06; D2 V-02 | 事件归一化、双重准入、跨来源 held 状态与 Qt 最终栅栏 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 7.4 | 授权复核与释放 | D1 F-06; D2 V-02 | 接纳与 Qt 实际执行前双重 check；保护性释放不凭旧 Grant | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 7.5 | 键鼠、文本与触控 | D1 F-06; D2 V-02 | 事件归一化、双重准入、跨来源 held 状态与 Qt 最终栅栏 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 7.6 | 本地输入边界 | D1 F-06; D2 V-02 | 事件归一化、双重准入、跨来源 held 状态与 Qt 最终栅栏 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 8.1 | 协议组成与能力 | D1 F-04/F-05; D2 V-01; D3 E-01/G-01 | RFB/Native 双独立协议与设备身份/帧/输入/受管透明路径 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 8.2 | 协商与会话建立 | D1 F-04/F-05; D2 V-01; D3 E-01/G-01 | Native §8.2 TLS-first 与 A.1 plain 档位存在 SOURCE_TENSION_01，尚待受控消歧 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 8.3 | 目标发现与订阅 | D1 F-04/F-05; D2 V-01; D3 E-01/G-01 | RFB/Native 双独立协议与设备身份/帧/输入/受管透明路径 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 8.4 | 画面更新与控制 | D1 F-04/F-05; D2 V-01; D3 E-01/G-01 | RFB/Native 双独立协议与设备身份/帧/输入/受管透明路径 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 8.5 | 输入与扩展能力 | D1 F-04/F-05; D2 V-01; D3 E-01/G-01 | RFB/Native 双独立协议与设备身份/帧/输入/受管透明路径 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 8.6 | 报文规范引用 | D1 F-04/F-05; D2 V-01; D3 E-01/G-01 | V8 A/B 唯一权威，21 type ID 与字段不可被派生 D1/D2 另编 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 8.7 | 受管接入的连接与数据转发 | D1 F-04/F-05; D2 V-01; D3 E-01/G-01 | 中继只转获准 ByteStream、有界 FIFO，禁止任意端口与降级 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 9.1 | 部署授权与监听准入 | D1 F-05/F-06; D2 V-03; D3 E-01/G-01 | 部署清单不可信/目标歧义/监听缺许可一律 fail closed | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 9.2 | 标准协议安全边界 | D1 F-05/F-06; D2 V-03; D3 E-01/G-01 | VNCAuth 不加密、不构成独立自然人身份 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 9.3 | 设备身份与安全连接 | D1 F-05/F-06; D2 V-03; D3 E-01/G-01 | DeviceTLS 身份与 Gateway/Edge/用户站点身份分离 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 9.4 | 凭据与密码验证 | D1 F-05/F-06; D2 V-03; D3 E-01/G-01 | tls-password 独立 salted verifier，PBKDF2-HMAC-SHA256 及失败限额 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 9.5 | 安全故障与审计 | D1 F-05/F-06; D2 V-03; D3 E-01/G-01 | 监听前信任、HMI DeviceTLS、凭据/密码与站点/路由分权 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 9.6 | 版本迁移与兼容 | D1 F-05/F-06; D2 V-03; D3 E-01/G-01 | 监听前信任、HMI DeviceTLS、凭据/密码与站点/路由分权 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 9.7 | 工厂站点信任与受控设备路由 | D1 F-05/F-06; D2 V-03; D3 E-01/G-01 | RoutePermit 不代替 HMI DeviceId/Scope/Grant，边缘攻陷限制在短期路由 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 10.1 | 运行状态与启停 | D1 F-01/F-02/F-06; D3 E-01; D4 X-02 | H-DUAL=A 初启必需协议全部成功；运行期单协议失效如实 DEGRADED | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 10.2 | 故障隔离与恢复 | D1 F-01/F-02/F-06; D3 E-01; D4 X-02 | Runtime/端点故障隔离、受管资源配额和现场业务优先 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 10.3 | 输入与帧交付 | D1 F-01/F-02/F-06; D3 E-01; D4 X-02 | Runtime/端点故障隔离、受管资源配额和现场业务优先 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 10.4 | 网络与内存预算 | D1 F-01/F-02/F-06; D3 E-01; D4 X-02 | 8 连接/64 普通输入/136 保护性释放/512KiB Native 待发/128MiB 受管预算；非实际 RSS | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 10.5 | 现场业务优先 | D1 F-01/F-02/F-06; D3 E-01; D4 X-02 | Runtime/端点故障隔离、受管资源配额和现场业务优先 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 10.6 | 工厂边缘接入的运行与资源预算 | D1 F-01/F-02/F-06; D3 E-01; D4 X-02 | Edge/Gateway 单独进程预算，不能在转发层拆删密文字节 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 11.1 | 设备工作区 | D2 V-01/V-02/V-03; D3 E-01 | 设备档案/信任/多会话输入焦点/生命周期/更新 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 11.2 | 设备信任与权限状态 | D2 V-01/V-02/V-03; D3 E-01 | 设备档案/信任/多会话输入焦点/生命周期/更新 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 11.3 | 多设备与输入焦点 | D2 V-01/V-02/V-03; D3 E-01 | 设备档案/信任/多会话输入焦点/生命周期/更新 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 11.4 | 控制申请与断线处理 | D2 V-01/V-02/V-03; D3 E-01 | 设备档案/信任/多会话输入焦点/生命周期/更新 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 11.5 | 客户端更新与恢复 | D2 V-01/V-02/V-03; D3 E-01 | 设备档案/信任/多会话输入焦点/生命周期/更新 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 11.6 | HyRemoteView 内置边缘角色的生命周期 | D2 V-01/V-02/V-03; D3 E-01 | H-EDGE=A 登录期间在线；关闭画面窗口不等于退出站点服务；注销真离线 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 12.1 | 诊断工具职责 | D4 T-01/X-02; D2 V-03 | 诊断事实出处、元数据脱敏、业务结果不得推断 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 12.2 | 开发与部署诊断 | D4 T-01/X-02; D2 V-03 | 诊断事实出处、元数据脱敏、业务结果不得推断 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 12.3 | 诊断证据来源 | D4 T-01/X-02; D2 V-03 | 诊断事实出处、元数据脱敏、业务结果不得推断 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 12.4 | 操作审计边界 | D4 T-01/X-02; D2 V-03 | 诊断事实出处、元数据脱敏、业务结果不得推断 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 12.5 | 诊断导出与脱敏 | D4 T-01/X-02; D2 V-03 | 诊断事实出处、元数据脱敏、业务结果不得推断 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 12.6 | 运行指标 | D4 T-01/X-02; D2 V-03 | 诊断事实出处、元数据脱敏、业务结果不得推断 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 13.1 | 业务接口与授权 | D1 F-06; D2 V-03; D4 X-02 | 文本与文件等分别授权，传输和业务结果分段确认 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 13.2 | 文本与剪贴板 | D1 F-06; D2 V-03; D4 X-02 | 文本与文件等分别授权，传输和业务结果分段确认 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 13.3 | 文件传输与业务确认 | D1 F-06; D2 V-03; D4 X-02 | 文本与文件等分别授权，传输和业务结果分段确认 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 13.4 | 拖放、打印与通知 | D1 F-06; D2 V-03; D4 X-02 | 文本与文件等分别授权，传输和业务结果分段确认 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 13.5 | 音频、标注与录制 | D1 F-06; D2 V-03; D4 X-02 | 文本与文件等分别授权，传输和业务结果分段确认 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 13.6 | 可选能力扩展 | D1 F-06; D2 V-03; D4 X-02 | 文本与文件等分别授权，传输和业务结果分段确认 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 14.1 | 设备交付与接管 | D1 F-06; D2 V-03; D4 X-02 | 客户接管、离线撤销确认、有限售后委托和基础 Direct 权益 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 14.2 | 售后委托与期限 | D1 F-06; D2 V-03; D4 X-02 | 客户接管、离线撤销确认、有限售后委托和基础 Direct 权益 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 14.3 | 离线撤销与确认 | D1 F-06; D2 V-03; D4 X-02 | 离线撤销请求与 HMI 确认生效分开，不虚构已执行 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 14.4 | 永久离线使用权益 | D1 F-06; D2 V-03; D4 X-02 | 合法永久基础 Direct 权益不因企业服务到期自动中止 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 14.5 | 品牌定制与企业服务 | D1 F-06; D2 V-03; D4 X-02 | 客户接管、离线撤销确认、有限售后委托和基础 Direct 权益 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 15.1 | 软件开发包与依赖 | D1 F-03; D2 V-03; D4 X-01/X-02 | Qt Kit/Private ABI、构建分发与准确支持声明 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 15.2 | 私有接口兼容 | D1 F-03; D2 V-03; D4 X-01/X-02 | Qt private ABI 对具体 Qt 版本/编译器/平台验证，不外推 public API 合格 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 15.3 | 平台与图形适配 | D1 F-03; D2 V-03; D4 X-01/X-02 | Qt Kit/Private ABI、构建分发与准确支持声明 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 15.4 | 构建与软件分发 | D1 F-03; D2 V-03; D4 X-01/X-02 | Qt Kit/Private ABI、构建分发与准确支持声明 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 15.5 | 支持声明与验证 | D1 F-03; D2 V-03; D4 X-01/X-02 | Qt Kit/Private ABI、构建分发与准确支持声明 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 16.1 | 产品条款追踪 | D4 X-02; D5 W00; D3 E-01/G-01 | 源自 V15/附录 D 的条款、主责章节与正负验证矩阵分层审签 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 16.2 | 构建与环境组合 | D4 X-02; D5 W00; D3 E-01/G-01 | 产品条款到架构/测试、变更、跨域 E2E 和证据归属 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 16.3 | 关键场景验证 | D4 X-02; D5 W00; D3 E-01/G-01 | 产品条款到架构/测试、变更、跨域 E2E 和证据归属 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 16.4 | 设计与实现证据 | D4 X-02; D5 W00; D3 E-01/G-01 | 产品条款到架构/测试、变更、跨域 E2E 和证据归属 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 16.5 | 演进与兼容原则 | D4 X-02; D5 W00; D3 E-01/G-01 | 产品条款到架构/测试、变更、跨域 E2E 和证据归属 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 16.6 | 跨域访问的架构验证 | D4 X-02; D5 W00; D3 E-01/G-01 | 产品条款到架构/测试、变更、跨域 E2E 和证据归属 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 A.1 | 通用消息格式 | D1 F-05; D2 V-01; D4 X-02 | 8-byte BE header、flags=0、TLS/plain 预配档位（与 §8.2 待消歧） | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 A.2 | 消息类型与字段顺序 | D1 F-05; D2 V-01; D4 X-02 | 21 type/字段顺序/长度固定；caps/status/format 等具体数值域 OPEN | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 A.3 | 输入子类型与键码 | D1 F-05; D2 V-01; D4 X-02 | INPUT subtype 1..5；modifier/button/key 映射部分未定 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 A.4 | 画面事务与重同步 | D1 F-05; D2 V-01; D4 X-02 | 每 Target 独立 generation/frame_id，FULL 覆盖、WAIT_FULL/REQUEST_FULL | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 A.5 | 错误码与关闭原因 | D1 F-05; D2 V-01; D4 X-02 | ERROR 13 码及 CLOSE 1..5 原文已明确，TLS 失败无明文 ERROR 降级 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 A.6 | 长度与能力约束 | D1 F-05; D2 V-01; D4 X-02 | 32 Target、8192 单边、8388608 像素、64..256KiB Rect、上限验前分配 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 B.1 | 内部对象及宿主接口 | D1 F-03/F-06; D4 X-01 | 7 种 Host 接口和受控 Source/Policy Provider 生命周期 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 B.2 | 授权事件与线程约束 | D1 F-03/F-06; D4 X-01 | 7 授权事件及 InputEnvelope Qt 最终执行前 Runtime/Grant 双栅栏 | `INDEXED_CONTRACT_REVIEW_PENDING` |
| V8 B.3 | 受控部署清单示例 | D1 F-03/F-06; D4 X-01 | 示例键值不是审批凭据，不允许仅凭文本批准引用开放监听 | `INDEXED_CONTRACT_REVIEW_PENDING` |

## 3. 强制交叉复核与冻结前缺口

| 审查簇 | V8 小节/附录 | 衔接的 V15 小节 | 不可越过的技术差距 |
| --- | --- | --- | --- |
| 一 Runtime 四入口 | §§3.1–3.5,4.1–4.6,15.1–15.5,附录 B | V15 §§2.2,3.1–3.4,13.1,13.6 | Qt Private 精确 Kit/ABI、受控准入与统一 Target/Policy |
| 授权/撤权双栅栏 | §§5.1–5.5,6.1–6.5,7.1–7.6,附录 B | V15 §§4.1–4.3,6.1–6.3,10.1–10.2 | Host 唯一 PolicyEpoch、per-session 缓存与 Qt delivered-before-action 竞态 |
| RFB/Native 二协议 | §§8.1–8.6,9.1–9.6,附录 A | V15 §§5.1–5.4,7.1–7.3 | 附录 A 21 类定长/变长与 SOURCE_TENSION_01、WIRE-V01..V07 未定数值域 |
| Managed/Factory Edge | §§3.6,8.7,9.7,10.6,11.6,16.6 | V15 §§1.4,8.6–8.7,13.8–13.9 | 登录态 Edge、RoutePermit 与 HMI DeviceGrant 分离、E2E/公网 Gate |
| Tool/软件/权益 | §§12.1–12.6,14.1–14.5,15.1–15.5,16.1–16.5 | V15 §§11–12,14 | H-T01=A；Tool S1 #586 OPEN，Qt 精确平台证据、持续合法离线使用 |
| 安全资源 | §§9.1–9.7,10.1–10.6,附录 C/E | V15 §§10–11,14 | 设计额定不等于实测/SLA；中继不吸收 HMI 128MiB 预算 |

**审计结果**：`ARCH_V8_H2_INDEX=99/99`；`APPENDIX_C_D_E_F=INVENTORIED_NOT_CLAUSE_COMPLETE`；`OWNER_DESIGN_PACKAGE=ASSIGNED`；`ATOMIC_ARCH_CONTRACT_STATE_ERROR_TEST_OWNER=NOT_FULLY_REVIEWED`；`GATE_D=NO`; `GATE_I=NO`. 继续只在设计阶段审核，一旦需要改变批准的 Native A/B 字节值/正式语义，先提交受控差量给 Human，不自行赋值。
