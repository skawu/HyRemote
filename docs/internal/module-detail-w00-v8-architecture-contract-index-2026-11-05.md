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
| V8 8.2 | 协商与会话建立 | D1 F-04/F-05; D2 V-01; D3 E-01/G-01 | SOURCE_TENSION_01 已获 Human=A：plain 与 tls-password 都须 HMI DeviceTLS 先行；Host/View 状态与测试仍待审核 | `INDEXED_CONTRACT_REVIEW_PENDING` |
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
| V8 A.2 | 消息类型与字段顺序 | D1 F-05; D2 V-01; D4 X-02 | 21 type/字段顺序/长度固定；原先未绑定 caps/status/format 数字已通过 Human P-A 受控补充，互操作未运行 | `INDEXED_CONTRACT_REVIEW_PENDING` |
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
| RFB/Native 二协议 | §§8.1–8.6,9.1–9.6,附录 A | V15 §§5.1–5.4,7.1–7.3 | 附录 A 21 类字段与 Human 已批准的 P-A/A 数值及 TLS-first 合同；26 字节向量、16 负例仍未运行 |
| Managed/Factory Edge | §§3.6,8.7,9.7,10.6,11.6,16.6 | V15 §§1.4,8.6–8.7,13.8–13.9 | 登录态 Edge、RoutePermit 与 HMI DeviceGrant 分离、E2E/公网 Gate |
| Tool/软件/权益 | §§12.1–12.6,14.1–14.5,15.1–15.5,16.1–16.5 | V15 §§11–12,14 | H-T01=A；Tool S1 #586 OPEN，Qt 精确平台证据、持续合法离线使用 |
| 安全资源 | §§9.1–9.7,10.1–10.6,附录 C/E | V15 §§10–11,14 | 设计额定不等于实测/SLA；中继不吸收 HMI 128MiB 预算 |

**审计结果**：`ARCH_V8_H2_INDEX=99/99`；`APPENDIX_C_D_E_F=INVENTORIED_NOT_CLAUSE_COMPLETE`；`OWNER_DESIGN_PACKAGE=ASSIGNED`；`ATOMIC_ARCH_CONTRACT_STATE_ERROR_TEST_OWNER=NOT_FULLY_REVIEWED`；`GATE_D=NO`; `GATE_I=NO`. 继续只在设计阶段审核，一旦需要改变批准的 Native A/B 字节值/正式语义，先提交受控差量给 Human，不自行赋值。

## 4. 首批架构原子义务的跨文档反证审查（16 个高风险断面，非 99 条审签）

本节依据现有 99 行索引、[D1 Framework](module-detail-d1-framework-boundaries-2026-11-05.md)、[D1 RFB/Native](module-detail-d1-device-protocols-2026-11-05.md)、[D2 View](module-detail-d2-hyremoteview-client-2026-11-05.md)、[D3 Edge/Gateway](module-detail-d3-managed-integration-2026-11-05.md)、[D4 Tool/交付](module-detail-d4-tool-delivery-quality-2026-11-05.md) 对**十六个直接影响实现安全性和可用性的架构断面**逐一查找现有原子设计依据。这里的 `LINKED_DESIGN` 只表示“确有现成内部合同及明确反例/Owner”，**不是**该节全部句子已审签或任何实测 PASS。

| V8 小节或非 H2 约束 | 权威 Owner 与对应候选合同 | 已核的状态/时序/失败闭合断言 | 未来最小反证入口、未完成项 | 本轮 |
| --- | --- | --- | --- | --- |
| §§4.4/15.2 平台插件、私有接口兼容 | F-03 `INT-04/08` + X-01 `DEP-01/08`；精确 Qt Kit/ABI 资格归各产品交付 Owner | 插件/QtPrivate 只加载在 exact Kit；QPA native delegate 错误不阻断独立 HMI 业务，禁用远程监听而非伪装成功 | FW-01/10、XP-01；不同 Qt minor/编译器/插件部署真测仍 NOT_RUN | `LINKED_DESIGN_NOT_SIGNED` |
| §§4.6/9.1 H-02a 启动与受信监听准入 | F-02 `RT-01`、F-03 `INT-07`、F-06 `POL-01/02`；最终 Host DeploymentAdmission | required RFB/Native 先 prepare/统一 commit；缺安全材料或端点失败即 abort，不出现半启动成功或明文弱回退 | FW-02/03、XP-01；端口/准备竞态尚需独立技术审查 | `LINKED_DESIGN_NOT_SIGNED` |
| §§5.3/7.3 控制独占与跨协议输入来源 | F-06 `POL-04/05/06/08` + F-04 `RLG-01..09`；HMI Scope 控制仲裁唯一权威 | Remote Ctrl 必须有当前 Scope 与 Session holder；两个 RFB/Native peer 按来源 held，不误放现场或另一合法连接 | FW-04/06、XP-04/05；按键账本与 GUI 交付顺序仍为设计候选 | `LINKED_DESIGN_NOT_SIGNED` |
| §§5.5/7.4 撤权、Qt 最终交付与保护释放 | F-06 `POL-05/06/07/08`、F-05 `AFS-06/09`；HMI 当前 PolicyRevision/GrantEpoch 权威 | 先让旧权失效，再拒旧 Qt ticket/清受控旧输入；已登记的 held 有单独幂等 UP 通道，不能因失权禁 DOWN 而残留卡键 | FW-06/09、XP-05/26；OS 内核已交字节不承诺追回 | `LINKED_DESIGN_NOT_SIGNED` |
| §§6.3/6.4 隐私派生帧和撤销缓存 | F-02 `RT-04/05/08`、F-06 `POL-03/07`、V-02 `SURF-02/05` | BaseFrame 可共享，但 Mask/Watermark/Delta 必须 per-session 授权；策略改动使旧 Full/Delta/旧敏感显示失效 | FW-07/08、XP-06/25/26；并发旧帧与慢端终止需未来验证 | `LINKED_DESIGN_NOT_SIGNED` |
| §§8.2/9.3 TLS-first、身份/会话建立 | F-05 `AFS-01/02/03/10` + V-03 `IDT-02/06`；原 V8 A/B 与 Human P-A/A 唯一 wire 依据 | 两 Native 档位均先可信 DeviceTLS；plain 无密码 AUTH 也绝非无 TLS/默认个人身份；User Auth、ViewGrant、ControlGrant 独立 | GN-01..16、XP-03/07/10；26 正向/12 负向字节规格已设计但真实 Parser/E2E NOT_RUN | `LINKED_DESIGN_NOT_SIGNED` |
| §§8.7/9.7 受管可信路由与透明数据面 | E-01/G-01 `EG-01/03/04/05/07/08` + F-05 `AFS-11` | Gateway Permit/Pair 和本地白名单是路由许可，非 DeviceTrust/Grant；Route bytes 严格有序、有界，拥塞只能背压或整路关闭 | MG-03/04/06/09、XP-09/10/27；#190 未激活，不能宣称公网安全或代理级业务结果 | `LINKED_DESIGN_NOT_SIGNED` |
| §10.4 + 附录 C 性能和受管内存 | F-02 `RT-03/05/07`、X-02 XP-17/27、D3 `EG-07`；每进程/队列所有者独立 | V8 所列 128 MiB 受管 HMI 目标、96 MiB 共享原始帧子池、8 连接、Native 每连接待发 512 KiB 等是设计预算；不能和系统 RSS/客户 SLA 混算；Edge/Gateway 队列另计 | XP-17/27、FW-12；各平台精确 Kit/P95/RSS/负载未实测，容量仍为候选资格 | `LINKED_DESIGN_NOT_SIGNED` |
| §§11.4/11.6 客户端断线与同产品 Edge 生命周期 | D2 `SURF-05/07`、`VIEW-EDGE-01..05` + D3 `EG-09`、`EDGE-LC-01..08` | 关闭单个画面仅销毁该 Session；全 View 退出/OS 注销撤当前 EdgeRunLease/Route；旧 UI 回调不能显示虚假在线或恢复 Grant | XP-12/24/26；真实 OS 注销/崩溃/锁屏与 QtQuick 生命周期测试 NOT_RUN | `LINKED_DESIGN_NOT_SIGNED` |
| §§12.3/12.5 诊断来源、导出/脱敏 | T-01 `TL-03/05/06` + V-03 `IDT-07`；Tool 只拥有用户导入证据/报告 | SDK/Qt Kit 静态与运行级观察分开；导出前由用户确认，禁止 token/私钥/画面/输入全文泄漏；来源未知保持 UNKNOWN | XP-14/16/20；Tool 新 Inspector/Report 仅设计，S1 #586 OPEN | `LINKED_DESIGN_NOT_SIGNED` |
| §13.3 文件传输与业务结果确认 | D4 XP-22、D1 F-06 与 D2 V-03 仅有边界/验收描述，**缺明确的 Host 文件接收/业务结果原子接口、权限、版本及实现分期签收** | “发送完成 / 设备接收 / Host 校验接受 / 业务已执行”不能合成同一成功；结果不明不得重放，且禁止私改 Native 1.0 增加新消息 | XP-22；[D4 §5.5](module-detail-d4-tool-delivery-quality-2026-11-05.md) 已分离写出/设备接收/Host 业务结果，Native 1.0 21 类无文件/业务回执消息；受信 Host 文件接口与实现分期仍 OPEN，改 wire 需 Human | `OPEN_INTERFACE_AND_PHASE` |
| §14.3 客户离线撤销与回执 | D4 XP-23、F-06 Device Policy 及 V-03 Identity 有原则，**尚未形成离线撤销签发、待送达、设备确认、生效与过期的完整持久状态与 Owner/回执合同** | 服务端“已下发”不能等同离线 HMI “已收讫/已撤权”；既有永久离线合法权益与有限委托期限必须分开 | XP-23/28；需按 V15 §12.1/§13.3 与 V8 §14.3 原文审查撤销生效语义及离线信任主体，无证据不补造云控制通道 | `OPEN_DELIVERY_ACK_AND_OWNER` |
| §14.4 永久离线 Direct 权益 | F-03 `INT-10`、V-03 `IDT-08`、X-01 `DEP-07`、D4 XP-19/28；本地设备安全门为最终权威 | 合法基础 Direct 不依赖 Gateway、售后账号或订阅；设备自身安全政策撤销与购买权益必须分别判断 | XP-19/23/28；期限/License/DeviceGrant 业务边界仍需原 Word 逐句核查 | `LINKED_DESIGN_NOT_SIGNED` |
| §§15.4/15.5 构建、发布与支持真相 | F-03 `INT-08`、X-01 `DEP-01/08`、T-01 `TL-03`；各产品交付和 QA Owner | 编译成功不等于 Qt Kit、平台、协议、安装器、E2E 已资格化；升级不得恢复已撤权限或旧 Site/Grant | FW-10、DEP-01/08、XP-24；正式产品支持声明全部留 Gate-R | `LINKED_DESIGN_NOT_SIGNED` |
| 附录 A.2/A.5/A.6 类型/错误/限额 | V8 A/B 原始字段与 Human [P-A/A](module-detail-w00-native-v8-delta01-approved-2026-11-05.md) 受控增补；F-05 `AFS-10` | 21 type/字段宽度和已有 ERROR/CLOSE 不修改；未知值、错误长度、消息乱序按 V8 拒绝；不能以“向量设计”冒充 Parser | 26 字节 golden + 16 负向设计、GN-01..16；真实 Host/Viewer 对拍 NOT_RUN | `LINKED_DESIGN_NOT_SIGNED` |
| 附录 B.2 事件、宿主线程与业务结果 | F-06 `POL-05/06/08`、F-02 `RT-02/06`；HMI Host/Qt GUI 最终调度 Owner | 设备策略变更与 Qt 目标销毁具有单调代际；旧 GUI ticket 在最终投递前拒绝，`QUEUED` 不等于业务执行成功 | FW-06/09、AFS-09、XP-05；Qt5/6 实际 EventLoop 竞态和 Host 结果证明未运行 | `LINKED_DESIGN_NOT_SIGNED` |

**本次实际审查结果**：上述 16 组中 **14 组仅建立合同、状态、失败与测试的明确设计级链接，2 组存在不可忽视的原子接口/责任缺口**（V8 §13.3 的文件/业务结果，§14.3 的离线撤权送达/确认）。这两项不是允许立即修改 Native wire 或新建云控制面的授权，必须从批准的原 Word 条款和现有模块设计边界中闭合。99 个 Heading2 行仍是 `INDEXED_CONTRACT_REVIEW_PENDING`，非 Heading2 的附录 C/E/F 除本表 C 的预算切片外也未逐条签收；**本表不是 14/99 条已冻结**。

**对正式 Word 编制的直接影响**：上述“信任/Route/Grant 是三种不同权威、申请控制与实际 Qt 执行是两种不同结果、内存目标不是客户 SLA、安装不是启用、离线撤销待送达不是已生效”的边界必须在 D5 已规划的组件/信任/撤权/部署图表和表格中使用同一术语；其中两条 `OPEN` 未解决前不宣称全产品 Gate-D 冻结。所有新增后续实测仍 `DESIGN_ONLY/NOT_RUN`，不是新的实施工单或设计路线重开。

## 4.1 GD-05｜V8 附录 A.3 与 Core 现行输入 ABI 的原子反证

**来源不一致不是 wire 值待选**：Human 已批准 Native P-A button 1..5、KP_0..KP_EQUAL 0x0400..0x0410。当前已发布 `src/core/include/hyremote/core/input.hpp` 的公开 `PointerButton` 只有三按钮、`KeyCode` 只有 69 个非小键盘已知值；Runtime 旧 `InputMailboxAdmission`、`qt_window_system_input.cpp` 仅围绕 69+3 类设计。不能误称 **Native 21 type 已获批准**就推断 Qt 全部输入类型已实现。

| V8/P-A 原子义务 | 当前/批准的差别 | Gate-D 内部合同 Owner | 负向反证与状态 |
| --- | --- | --- | --- |
| A.3 KEY 主键/17 小键盘 | 合法 wire 覆盖 69+17，公开 Core 仅 69 类 | F-05 内部 NativeInputFact、F-06 Qt 最终 Adapter，V-02/V-01 精确 Keypad source/encode | GN-14/XP-21；不可用普通数字/ENTER 伪映射，OPEN_PLATFORM_PROOF |
| A.3 POINTER_BUTTON 1..5 | X1/X2 对当前公开三按钮是**合法扩展类型**，不可当非法 wire | F-05 与 V-01 协议校验、F-06 Qt 末端、V-02 UI 来源 | GN-13/XP-21；保护释放/跨会话 OWNED，OPEN_PLATFORM_PROOF |
| V8 每实际目标普通 64 / 保护 136 | 旧 69+3=72 逻辑 held 类别的证明不能覆盖未来 86 键+5 按钮=91 类 | F-06/Runtime QtInputDomain 同时已接纳 held 限额 72，仍按单物理叶子 64/136 | GN-15/16/RACE-03，held 73 拒新 DOWN 但不丢旧 UP，DESIGN_CONTRACT_PENDING_INDEPENDENT_SIGNOFF |
| public Core 兼容 | 若直接扩展公开 KeyCode/PointerButton，可能破坏已发布 SDK ABI/兼容 | F-01 Core 保持既有 ABI；F-05/F-06 使用不出现在公共 API 的内部事实模型 | XP-01/21；旧 RFB 与四接入不回归，NOT_RUN |

本表不是 A/B 新协议规范、Host/Viewer 已通过测试，也不能用批准的 wire 取值推导旧 Core 已支持该输入集合。后续正式 Word 需受控更新 §4.2/4.3/§8 与附录 Native 输入字典，不能悄悄覆盖获 Human 文档评审 V5。**Gate-D 仍 NOT_FROZEN**。

## 5. GD-02｜99 项中非普通合同链接项目及附录责任（2026-11-06）

截至本轮 Human H-O01=A、H-O02=A 已批准：上文第 4 节对 V8 §13.3/§14.3 的两处“接口/分期 OPEN”为 2026-11-05 历史审核结论。现在**本期功能分期取舍已关闭，但未来文件/Host 回执与可信设备收件通道未设计/未实现**；Gate-D 本期应验证 UNSUPPORTED/OUTCOME_UNKNOWN/DEVICE_PENDING 与合法 Direct，不能要求在现有 Native 21 消息中实现未来回执。

| V8 原子来源组（对应正式 V5 的 15 项非普通合同链接） | Owner / 本期必须写清的界限 | 合同与否定验收（设计级） | 签收状态 |
| --- | --- | --- | --- |
| §§1.1–1.4、16.1（5） | X-02/W00 负责批准源与规范优先级、追溯审签/版本证据；这些是文档与变更控制义务而非 5 个 runtime API | X-02 QA、XP-01..28、V15 56 目录精准对应；不凭索引推出产品功能实现 | NORMATIVE_DOCUMENT_REVIEW_PENDING |
| §7.5（1） | F-05/F-06 与 V-02 分责 CommittedText、键鼠、触控，按已批准网络字段及 Qt 最终线性化处理 | POL-05/06/08、AFS-09、SURF-06/07、XP-21；未授权多点触控不能伪造 | INPUT_ATOMIC_REVIEW_PENDING |
| §§13.1、13.3（2） | H-O01=A：本期禁止假文件/业务回执；设备 Host 是业务结果唯一权威，扩展留未来受控版本；§13.1 的业务权限仍需 Host 拒绝合同 | POL-03/05、XP-22 本期 UNSUPPORTED；不得经 INPUT/clipboard/RFB 伪造文件接口 | HUMAN_PHASE_APPROVED / CURRENT_DENIAL_REVIEW_PENDING |
| §13.2、§13.4–13.6（4） | 文本/剪贴板、拖放/打印/通知、音频/标注/录制与可选扩展是**独立能力**；H-O01=A 不自动将其全部延期，既不得默认支持亦不得擅改上位范围 | F-06 Host 权限 + V-03 能力真相、XP-20/21/22；不得隐含文件上传或未经批准的录制 | CAPABILITY_SCOPE_REVIEW_PENDING |
| §§14.2–14.3（2） | H-O02=A：委托期限/客户接管与可信设备生效事实分离；永久合法 Direct 权益独立 | F-06 POL-01/07、V-03 IDT-08、XP-23/28；离线待送达不是已生效 | HUMAN_SEMANTICS_APPROVED / DEVICE_FACT_REVIEW_PENDING |
| §14.5（1） | OEM 表现与企业 Gateway 服务分离；G-01 #190 独立产品激活，不能推导设备 Grant | V-03 IDT-09/10、G-01 EG-04、X-01 DEP-01/08、XP-16/24 | ENTERPRISE_GATE_REVIEW_PENDING |

**非 Heading2 附录处理**：A/B 的 21 message/7 Host 事件及 Human P-A/A 仍是 Native 唯一权威；C 的资源限额必须原值绑定到 Runtime/Edge/Gateway 的**各自计量域**，不得拿 128 MiB/96 MiB 等设计预算冒充进程 RSS；D 的 56 ID 与 V15 原文标题已 56/56 校验；E/F 的具体条款、术语/示意与变更优先级仍须重新按原 Word 逐项核对，目录映射不是签收。没有已批准 Wire 变更，本轮没有任何产品测试 PASS。

**Gate-D 未闭合**：上表解决的是来源接收方、范围状态与负例入口定位，**不等于对 99 个小节逐句技术审签**。V5 仍是已批准的文档版本，后续正式受控修订才更新其 99 项附录和图表。

## 6. GD-05｜Route 重放与网关回滚的架构反链补足

正式版 V5 的 99 条追溯表中，只有 `XP-13` 与 `XP-18` 缺少**具体小节**的验收反链（排除 `XP-01..28` 纯治理范围行，其他单条缺失多由 `XP-09/10` 一类缩写造成）。V8 上位与现有技术合同的唯一接收方如下；不修改 V8 原文或延期 Gateway #190 资格。

| V8 规范条款 | 技术合同 Owner | 反证入口与必须观察的拒绝 | 冻结状态 |
| --- | --- | --- | --- |
| §§8.7、9.7（受管 Route/站点信任） | D3 `EG-04/05/08`、`MG-06/09`，G-01 的 PairLedger 与 E-01 的本地 CurrentSitePolicy；HMI 设备端最后授权独立 | `XP-13`：令牌 audience 错误、nonce/epoch 重放、双终端抢同凭据、旧 RouteClosed 回调不得恢复；不出现任意 TCP 拨号 | `DESIGN_LINKED/NOT_RUN` |
| §§11.6、15.4–15.5、14.4（工厂生命周期、升级兼容、离线 Direct） | D3 `EG-11/EDGE-LC-06` + D4 `DEP-04..08`，Gateway 安全配置/Edge 可信恢复分别独占 | `XP-18`：旧备份/旧 token、密钥轮换中断、回滚及登录态退出失败后不能复活撤销 Route/Share；合法本机 Direct 不依赖 Gateway | `DESIGN_LINKED/NOT_RUN` |

本节仅补足 *Requirement→Owner/Contract→Test* 反向索引；真正 Gateway 技术接口、平台证据与自托管发布均独立未激活，不能凭此表标 `GATE_D=PASS`。
