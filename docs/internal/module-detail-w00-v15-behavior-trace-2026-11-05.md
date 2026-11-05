# W00｜V15 56 条产品要求→模块行为与失败验收追溯（设计草案）

> 来源：Human 本轮上传并声明与正式版**正文相同**的 `HYR-PD-001/V15` 送审稿，以及 `HYR-ARCH-001/V8` 送审稿。两份 SHA 和 56/56 V8 附录 D 章目核验见 [上位正文审计](module-detail-source-audit-v15-v8-2026-11-05.md)。本表**不是**源文逐字转录，也不是最终技术设计验收：其“行为合同/失败闭合”是根据原文归纳、映射到既有 D1–D5 设计候选的 **待审实现合同**。原 Word 保留完整约束，本文不取代原 Word。

## 1. 审核规约

- **ID 不造来源**：`V15 §X.Y` 是 Word 原有 Heading2 章节 ID。标题按原稿保留。最后一列 `S#` 是 **V15 表 6 第 N 条场景的引用序号（本文派生索引）**，不是原文独立发布的测试 ID，也不证明已执行。对应 31 条完整业务场景仍必须以 V15 表 6 原文为准。
- **模块责任**：`D1 F-01..F-06`、`D2 V-01..V-03`、`D3 E-01/G-01`、`D4 T-01/X-01/X-02`、`D5 W00` 是已合并的设计模块/计划域；多模块以分号表示原文义务需在设备、客户端与质量多边各自满足，**并非多个独立授权源**。
- **状态不造 PASS**：本轮全部行只完成需求→责任→候选行为→禁止/故障→原 Word 验收场景初步映射，统一 `MAPPED_NOT_TESTED`。行级 `CONTRACT_REVIEWED`、`WIRE_READY`、`VALIDATED` 需有可查回证据，不能因为 Markdown 合并变 PASS。
- **验证级别**：首轮单独检查来源 56/56、标题对应、主模块归属、是否写出可反证失败边界。后续对每行追加精确 `ContractID/State/Error/TestID/Owner/EvidenceSHA` 并分解成可执行原子要求；多项“及”不应在一条测试覆盖下自动全绿。
- **五项批准**：`H-T01=A; H-DUAL=A; H-REVOKE=A; H-EDGE=A; H-RFB=A` 不再开放。Native V8 附录 A/B 的原有消息编号/字段不变；缺失数值与两档 TLS-first 语义已有 Human 批准的 P-A/A 受控补充，任何新增协议语义仍不得由本表改写。

## 2. V15 56 项来源逐行行为追溯

| 源小节 | 原文标题 | 设计责任（多方） | 需实现的候选行为合同（归纳） | 必须阻断/失败闭合 | V15 表6场景序号 S# | 状态 |
| --- | --- | --- | --- | --- | --- | --- |
| V15 §1.1 | 产品所解决的问题 | D1 F-02; D2 V-01 | 应用 Qt 目标而非桌面；业务由宿主确认 | 越界 OS/业务操作拒绝 | 1、3、7 | `MAPPED_NOT_TESTED` |
| V15 §1.2 | 面向的使用者 | D2 V-03; D4 X-02 | 开发/厂商/客户/现场/售后角色权限分别表达 | 角色可见不等于设备控制权 | 8、16、26 | `MAPPED_NOT_TESTED` |
| V15 §1.3 | 典型使用情境与阅读顺序 | D5 W00; D4 X-02 | 完整接入→核验→查看→控制→诊断→断线安全 | 不得自动复活操作 | 5、11、18 | `MAPPED_NOT_TESTED` |
| V15 §1.4 | 设计范围与非目标 | D1 F-02; D3 E-01/G-01 | 应用级授权目标、工厂单 View、可选网关；非通用代理 | 无授权目标/任意 TCP 拒绝 | 21、23 | `MAPPED_NOT_TESTED` |
| V15 §2.1 | 产品组件及其协作方式 | D1 F-02; D2 V-03; D3 E-01/G-01; D4 T-01 | Framework/Viewer/Tool/Gateway 责任独立 | 中继不授设备 Grant；Tool 不替代 View | 21、25 | `MAPPED_NOT_TESTED` |
| V15 §2.2 | 共享运行时与两种远程访问协议 | D1 F-01/F-02/F-04/F-05 | 四入口一 Runtime；RFB3.8 与独立完整 Native1.0 | 不得第三强制通道或 Native 依赖 RFB | 3、4、12 | `MAPPED_NOT_TESTED` |
| V15 §2.3 | 宿主负责业务结果确认 | D1 F-06; D2 V-03 | 发送/传输/宿主已执行分别给出处置证据 | 结果未知不自动重复业务命令 | 7 | `MAPPED_NOT_TESTED` |
| V15 §3.1 | 四种接入方式的适用场景 | D1 F-03; D4 X-01 | C++/QML/Generic/QPA 对接同一 Runtime | 不得第二采集/授权系统 | 15、16 | `MAPPED_NOT_TESTED` |
| V15 §3.2 | 启动、停止与异常处理 | D1 F-02/F-06; D4 X-02 | 受控部署后才监听；H-DUAL=A 必需端原子对外可用 | 启动失败回滚；宿主本地 HMI 不被强制终止 | 10、15 | `MAPPED_NOT_TESTED` |
| V15 §3.3 | 配置及环境适配 | D1 F-03; D4 X-01 | 监听/凭据/目标/Kit/QtPrivate 确切资格 | 缺配置 FailClosed；编译不算运行资格 | 15、16 | `MAPPED_NOT_TESTED` |
| V15 §3.4 | 免改源码接入的授权与交付核查 | D1 F-03/F-06; D4 X-02 | Generic/QPA 按可信选择器准入 Target | 无法区分敏感目标则停发 | 15 | `MAPPED_NOT_TESTED` |
| V15 §4.1 | 远程访问目标的范围 | D1 F-02/F-06; D2 V-03 | TargetKey/TargetId 与物理屏号分离；每会话授权 | 关闭/撤销目标后帧与输入同步失效 | 2、13 | `MAPPED_NOT_TESTED` |
| V15 §4.2 | 多屏查看及窗口安排 | D1 F-02; D2 V-02/V-03 | 一会话多个获准目标/本地窗口不增加 Scope | 历史布局不得恢复失权目标 | 2 | `MAPPED_NOT_TESTED` |
| V15 §4.3 | 显示准确性与画面时效 | D1 F-02/F-06; D2 V-02 | 像素/DPR/旋转换算、断线旧帧分级保护 | 敏感或规则不明时隐藏；停止旧输入 | 19 | `MAPPED_NOT_TESTED` |
| V15 §5.1 | 标准 RFB 的适用范围 | D1 F-04/F-06; D2 V-01 | 标准第三方 VNC 互通、Host 明确允许的 legacy 输入 | 共享口令不伪造个人或公网安全 | 3、14 | `MAPPED_NOT_TESTED` |
| V15 §5.2 | HyRemote 私有协议的适用范围 | D1 F-05/F-06; D2 V-01 | Native 全独立帧/输入/Target/Grant/TLS；A/B 原文权威 | 未知 wire 值拒绝，不能猜测兼容 | 4 | `MAPPED_NOT_TESTED` |
| V15 §5.3 | 直接连接、受管接入及恢复 | D2 V-01/V-03; D3 E-01/G-01 | Direct 独立离线；Managed 站点/设备/Grant 三层 | 无静默弱路径回退 | 22、25、28 | `MAPPED_NOT_TESTED` |
| V15 §5.4 | 兼容性说明及故障提示 | D1 F-04/F-05; D2 V-03 | 按协议版本、身份与权限区分失败原因 | 不将能力缺失、拒绝、不可达合并成成功 | 11、28 | `MAPPED_NOT_TESTED` |
| V15 §6.1 | 查看权限与控制权限 | D1 F-06; D2 V-03 | ViewGrant/ControlGrant/Scope 分离，默认只读、单控制者 | 审批前后双端最终栅栏；不能偷用旧 Grant | 1、12、26 | `MAPPED_NOT_TESTED` |
| V15 §6.2 | 现场人员参与及授权方式 | D1 F-06; D2 V-03 | 现场拒绝/抢占优先，不自动锁物理输入 | 撤权清 held，现场无需依赖 Viewer 配合 | 18、26 | `MAPPED_NOT_TESTED` |
| V15 §6.3 | 断线恢复及控制权交接 | D1 F-02/F-06; D2 V-01/V-03 | 断开/重连新身份与授权、held 清理 | 不重放迟到输入/控制申请/业务动作 | 5、19 | `MAPPED_NOT_TESTED` |
| V15 §7.1 | 键盘与鼠标操作 | D1 F-06; D2 V-02 | 组合键/小键盘/滚轮/拖拽及释放语义 | 失焦/撤权/队列满安全释放 | 6、18 | `MAPPED_NOT_TESTED` |
| V15 §7.2 | 中文及其他输入法 | D1 F-05/F-06; D2 V-02 | CommittedText 与 keysym 分离；不隐式同步剪贴板 | 禁止重复提交/漏字/误快捷键 | 6 | `MAPPED_NOT_TESTED` |
| V15 §7.3 | 触屏设备的输入方式 | D1 F-05/F-06; D2 V-02 | 鼠标仿真/直接点击/未来 Native 多点触控资格区分 | 本地手势不能触发远端业务 | 6 | `MAPPED_NOT_TESTED` |
| V15 §8.1 | 设备工作区与连接入口 | D2 V-01/V-03 | 无需云账号 Direct；档案、设备身份、授权三层分离 | 存储档案不能自动授予会话 | 11、31 | `MAPPED_NOT_TESTED` |
| V15 §8.2 | 以远程画面为中心的会话界面 | D2 V-02/V-03 | 清晰分设备、目标、当前权限及焦点 | 切换/失焦停止旧输入；迟到批准不得激活 | 1、26 | `MAPPED_NOT_TESTED` |
| V15 §8.3 | 多设备操作与风险防范 | D2 V-02/V-03 | 多个设备窗口清晰归属，输入仅向明确前台 Target | 背景焦点不可转发误输入 | 2、26 | `MAPPED_NOT_TESTED` |
| V15 §8.4 | 客户端更新与恢复 | D2 V-03; D4 X-01 | 签名可信更新、离线升级、档案迁移与回退 | 升级失败不能遗失/泄漏凭据 | 9 | `MAPPED_NOT_TESTED` |
| V15 §8.5 | 关键操作状态与异常反馈 | D2 V-03 | 身份可信/链路可用/权限有效三轴提示 | 不得已连接推断可控，权限失效立即禁用 | 11、28 | `MAPPED_NOT_TESTED` |
| V15 §8.6 | 工厂边缘接入与设备共享 | D2 V-03; D3 E-01/G-01 | Edge 默认 OFF，站点和单 HMI 白名单/期限/路由许可 | 仅路由许可不等于 ViewGrant/ControlGrant | 23、24、25 | `MAPPED_NOT_TESTED` |
| V15 §8.7 | 边缘接入运行与恢复 | D2 V-03; D3 E-01; D4 X-01 | 同 View 安装统一升级；H-EDGE=A 登录期间服务 | 注销真实离线、崩溃后不假在线/复活 Grant | 21、29 | `MAPPED_NOT_TESTED` |
| V15 §9.1 | 文本和文件交换 | D1 F-06; D2 V-03 | 文本/剪贴板/文件分别授权；宿主显式开放资源 | 不因此获得任意目录权限 | 7 | `MAPPED_NOT_TESTED` |
| V15 §9.2 | 文件传输与业务应用的区别 | D1 F-06; D2 V-03 | 上传/校验/接收/应用结果分段可信确认 | 结果未知不能重复提交业务动作 | 7 | `MAPPED_NOT_TESTED` |
| V15 §9.3 | 拖放、打印及应用通知 | D1 F-06; D2 V-03 | 拖动输入与本地文件导入不同授权；打印经宿主受控导出 | 通知不能隐式确认告警或升级设备 | 7 | `MAPPED_NOT_TESTED` |
| V15 §9.4 | 音频、协作标注及录制 | D2 V-03; D4 X-02 | 音频/标注/录制均为单独能力和许可 | 录制默认关闭，撤权停止采集，不上传系统声音 | 13、17 | `MAPPED_NOT_TESTED` |
| V15 §10.1 | 设备身份与访问控制 | D1 F-06; D2 V-03; D3 E-01/G-01 | HMI DeviceId/站点/Edge/用户各自核验 | Gateway 会话不替代 HMI View/ControlGrant | 11、25 | `MAPPED_NOT_TESTED` |
| V15 §10.2 | 可见范围及隐私遮罩 | D1 F-02/F-06; D2 V-02; D3 E-01 | 每会话出帧前遮罩/水印/授权，epoch 失效清缓存 | 遮罩不可证则拒帧；中继不存明文 | 13、17、27 | `MAPPED_NOT_TESTED` |
| V15 §10.3 | 审计信息与证据边界 | D4 X-02; D3 G-01 | 审计只记录可证身份、会话、传输和宿主结果 | 共享密码不自然人归因；元数据非业务成功 | 14、16 | `MAPPED_NOT_TESTED` |
| V15 §11.1 | 以用户问题组织诊断 | D2 V-03; D4 T-01 | 网络/身份/权限/画面静止/业务告警分来源提示 | 无权信息不展示、未观测不推断 | 16、28 | `MAPPED_NOT_TESTED` |
| V15 §11.2 | 轻量化诊断信息 | D1 F-02; D4 T-01/X-02 | 资源/会话/Qt/状态数据脱敏、按需开启 | 不能常驻采集密码/键盘全文 | 16 | `MAPPED_NOT_TESTED` |
| V15 §11.3 | HyRemoteTool 的集成诊断过程 | D4 T-01 | Qt Kit/部署/Target/RFB 基础检查与来源清晰报告 | 未执行不能 PASS；Tool 完整 S1 #586 仍 OPEN | 16 | `MAPPED_NOT_TESTED` |
| V15 §11.4 | 优先保障现场应用 | D1 F-01/F-02; D3 E-01/G-01; D4 X-02 | 本地 HMI/输入/撤权优先，帧/网络有界背压 | 慢端不能饿死 GUI 或串漏会话数据 | 10、30 | `MAPPED_NOT_TESTED` |
| V15 §12.1 | 设备交付后的治理责任 | D1 F-06; D4 X-02 | 售后委托期限与设备收件确认、客户接管 | 未下发撤销不能虚报生效；不可续活到期委托 | 8、20 | `MAPPED_NOT_TESTED` |
| V15 §12.2 | 品牌定制与企业服务 | D2 V-03; D3 G-01; D4 X-01 | OEM 仅呈现差异，企业服务为可选 | 品牌/账号不得扩权或关闭 Direct | 31 | `MAPPED_NOT_TESTED` |
| V15 §12.3 | 更新责任及离线使用权益 | D2 V-03; D4 X-01/X-02 | Framework/宿主升级分离，永久基础 Direct 离线权益 | 服务到期不能撤销合法基础离线访问 | 9、20、31 | `MAPPED_NOT_TESTED` |
| V15 §13.1 | 允许修改源码的 Qt 应用接入 | D1 F-03/F-06; D4 X-02 | C++ 明确登记目标/只读准入及宿主继续运行 | 配置错误不强制停宿主业务 | 1、15 | `MAPPED_NOT_TESTED` |
| V15 §13.2 | 双屏工业设备的远程维护 | D1 F-02/F-06; D2 V-02 | 两 Target 不同 Grant，撤销只关闭失权 Scope | 屏号非身份，残留输入必须清理 | 2、18 | `MAPPED_NOT_TESTED` |
| V15 §13.3 | 客户授权设备厂商开展售后服务 | D1 F-06; D4 X-02 | 有限期售后授权、客户撤销、业务结果分别归因 | 无期限信任/目录浏览不可默认开放 | 8、20 | `MAPPED_NOT_TESTED` |
| V15 §13.4 | 网络中断后的安全恢复 | D1 F-06; D2 V-01/V-02 | 旧画面时效规则、重新鉴权、重连新会话 | 不保留敏感画面、不重放旧操作 | 5、19、28 | `MAPPED_NOT_TESTED` |
| V15 §13.5 | 首次接入与设备身份变化 | D2 V-03; D1 F-05 | 设备信任建立与地址/证书变化独立核验 | 同 IP 不等于同设备 | 11 | `MAPPED_NOT_TESTED` |
| V15 §13.6 | 无法修改源码的存量 Qt 应用接入 | D1 F-03/F-06 | Generic/QPA 受控选择器与隐私界限 | 不能区分目标则拒绝 | 15 | `MAPPED_NOT_TESTED` |
| V15 §13.7 | 现场人员接管远程操作 | D1 F-06; D2 V-03 | 两协议共 Scope 撤权与 held 释放 | 已提交业务结果查询，不能假撤回 | 18、26 | `MAPPED_NOT_TESTED` |
| V15 §13.8 | 工厂内外协同的远程维护 | D2 V-03; D3 E-01/G-01 | 工厂仅 View、HMI 白名单、HMI DeviceTLS 最终 Grant | Gateway/Edge 不提供通用内网 TCP 代理 | 21、23、25、27 | `MAPPED_NOT_TESTED` |
| V15 §13.9 | 云端不可用与受管会话安全恢复 | D2 V-01; D3 E-01/G-01 | 云故障 Direct 继续、路由撤销/重连再鉴权 | 不自动降级/恢复旧授权/旧业务 | 22、28 | `MAPPED_NOT_TESTED` |
| V15 §14.1 | 以实际使用结果为依据 | D4 X-02; D5 W00 | 按 V15 表6 实际场景验收并标明证据 Kit/设备 | 未执行不算通过，PoC 不外推产品 | 1-31 | `MAPPED_NOT_TESTED` |
| V15 §14.2 | 全文一致遵循的设计原则 | D5 W00; D4 X-02 | 依赖/授权/失败/边界跨全部产品一致 | 缺运行证据或未审协议不得冻结 | 1-31 | `MAPPED_NOT_TESTED` |

## 3. 源文验收场景引用说明

V15 表6 共 31 条，引用序号范围 **1–31**，文档原文没有这些行的 `S#` 编号。第 21–30 条特别覆盖工厂 View 内置 Edge/Gateway 的端到端场景；表6中的控制撤权、多屏、隐私、断线、免改源码、离线售后和 Tool 诊断可以交叉用于多模块。**不得拿“56 行”作为 56 条独立自动化测试已经通过的证明。**

## 4. 三类高风险追踪冲突与剩余工作

| 追踪组 | 牵涉条款 | 必须保持跨模块同一事实 | 首轮结论 |
| --- | --- | --- | --- |
| 权限撤销/旧帧/held | §4.1/4.3/6.1–6.3/10.2/13.2/13.4/13.7 | HMI 唯一 Policy/Grant 线性化，网络可控旧缓存失效；View 遮旧帧、禁输入；旧已达网络/已见像素不承诺追溯删除 | `POLICY_APPROVED / RACE_AND_TEST_OPEN` |
| Direct/Managed/Tool | §1.4/2.1–2.2/5.1–5.3/8.6–8.7/11.3/13.8–13.9 | Direct 离线独立；Factory PC 只装 View；Edge/Gateway 仅 opaque 路由；Tool 仍维护已发布 RFB、S1 #586 OPEN | `PRODUCT_DECISION_APPROVED / E2E_OPEN` |
| Native 字节规范/身份 | §5.2/5.4/6.1/7.2/10.1 | 独立 Native 21 类与 HMI DeviceTLS、会话 Target/Grant；不凭 RoutePermit 或用户 UI 自行授权，按已批准 P-A/A 数值和 TLS-first 安全语义；未知值必须 FailClosed | `SOURCE_A_B_AUDITED / P_A_A_APPROVED / PARSER_E2E_NOT_RUN` |

### 4.1 首批高风险原子合同的实际交叉核查（设计级，不冒充 56 项签收）

本批从已有 56 行中选出**十组影响授权、失败关闭、业务事实和交付资格的高风险跨模块断面**，实地对照 D1/D2/D3/D4 已存在的合同 ID 与测试入口。下列 `CONTRACT_LINKED` 只表示**本轮已验证对应的设计文本存在且责任一致**；不代表所有源句子已经被原子化，更不是独立 Gate-D 审签或测试 PASS。尚无可核签的实现合同者明确 `OPEN`，不补造接口。

| V15 原文义务/消费方 | 已存在的最小 ContractID 与唯一事实 Owner | 正常状态→拒绝/异常时序（要审签的原子断言） | 负例证据入口及未关闭事项 | 本轮状态 |
| --- | --- | --- | --- | --- |
| §3.2 启动/停止；F-02/F-06 + F-03 | D1 `RT-01`、`INT-07`，Host DeploymentAdmission | 必需 RFB/Native 先 prepare → 完整可信 commit 后公开；任一缺项 abort → 未对外可用，Qt 本地业务仍正常 | FW-02/03、XP-01；准备/端口绑定/故障竞态尚待真实资格 | `CONTRACT_LINKED_NOT_SIGNED` |
| §§6.1/6.2/13.7 控制许可/现场优先；F-06 | D1 `POL-04/05/06`、F-05 `AFS-05/06/09`；HMI 当前 Scope Arbiter | 当前同 Scope holder 有效才可接纳 DOWN；现场抢占/GrantEpoch 变更 → 新 DOWN 禁入，GUI 旧票据投递前再拒、按来源保护释放 | FW-04/06、XP-04/05；Qt 最终交付线性化需独立审查 | `CONTRACT_LINKED_NOT_SIGNED` |
| §§4.3/10.2/13.4 显示隐私和撤权；F-02→V-02 | D1 `RT-04/05/08`、`POL-03/07`；D2 `SURF-02/05`；HMI 负责输出策略，View 负责本地过期图 | PolicyEpoch 变化 → 未发会话队列和 DELTA 基线失效；View 失权时清敏感旧屏与禁输入；不声称追回已到达 OS/TLS 或已见像素 | FW-06/07/08、XP-06/25/26；逐 Session Filter 与网络在途界限需独立审签 | `CONTRACT_LINKED_NOT_SIGNED` |
| §§5.1/5.2/5.4 两独立协议；F-04/F-05/V-01 | D1 `RLG-01..09` 与 `AFS-01/02/10`、Native P-A/A；HMI 设备权威 | Legacy RFB 固定 Target/Scope、明确允许输入且不伪造个人身份；Native 两档均先 DeviceTLS，非法握手/顺序/枚举拒绝 | DP-07、GN-01..12、XP-02/03/07；标准 Viewer + Native parser 双端均未做本轮实测 | `CONTRACT_LINKED_NOT_SIGNED` |
| §§8.6/13.8 工厂共享/受管访问；E-01/G-01/F-05 | D3 `EG-01/03/04/08`、D1 `AFS-11`；工厂管理员许可 Route，HMI 独立准许像素/控制 | Gateway Permit + 本地精确白名单双通过后只能建立 Route；HMI DeviceTLS/Grant 若拒 → 零获准像素和输入；不许任意 host:port | MG-03/06/07、XP-08/09/10；Gateway #190 NOT_ACTIVATED，产品级真实路由未测 | `CONTRACT_LINKED_NOT_SIGNED` |
| §§8.7/13.9 登录态 Edge 断线/恢复；E-01/V-03/X-01 | D3 `EDGE-LC-01..08`、`EG-09`，D2 `VIEW-EDGE-01..05`；当前 `EdgeRunLease` 唯一有效运行许可 | 单画面关闭不自动取消获准共享；整个 View 退出/OS 注销/崩溃 → 旧 RunEpoch/Route 失效，登录/升级不能无条件复活 | XP-12/24；需真实 OS 登录/注销/升级与持久撤权复验 | `CONTRACT_LINKED_NOT_SIGNED` |
| §§8.4/12.3 客户端/安装升级及持续 Direct；V-03/X-01 | D4 `DEP-01/04/05/07/08`、D2 `IDT-03/05/08`；各产品独立 Install/Trust Owner | 包签名/Kit/策略 schema 预检 → 可恢复激活；旧撤销或不同设备身份在失败/回滚后不得复活；合法 Direct 不依赖 Gateway | XP-19/24/28；持久高水位掉电一致性尚无实测 | `CONTRACT_LINKED_NOT_SIGNED` |
| §11.3 Tool SDK 集成与诊断；T-01 | D4 `TL-01/03/04/05/06/08`；Tool 是实际观察/报告 Owner，不是 HMI Grant 权威 | 用户明确 SDK/Kit/日志来源 → 只读检查/人工确认脱敏报告；来源未知/未运行 → `UNKNOWN_SOURCE/NOT_RUN`，不能变成 PASS | XP-14/15/16/20；仅完成设计，Tool S1 #586 OPEN，新 Inspector/Report 未实现 | `CONTRACT_LINKED_NOT_SIGNED` |
| §§7.1/7.2 用户输入与中文 IME；F-05/F-06/V-02 | D1 `POL-05/06/08`、D2 `SURF-04/06/07`；设备最终 Qt 投递为业务输入界限 | 有效焦点/Scope/代次才发新 DOWN/CommittedText；失焦/几何重绑清旧 held；禁止把提交文本与按键重复执行 | VIEW-06/07、XP-05/21；真实中文 IME 与多 Qt Kit 测试未运行 | `CONTRACT_LINKED_NOT_SIGNED` |
| §9.2 文件传输与业务应用结果；F-06/V-03 | 当前 D4 仅 `XP-22` 的阶段性验收语义；**没有在本轮查到可签收的文件传输实现接口/Host 业务结果回执合同** | 必须区分客户端已发、设备收到、宿主接受、业务生效/未知；结果未知不允许自动重放。**不得为填表私加 Native 1.0 字节消息或未经批准的管理端口** | XP-22；需回到 V15/V8 完整原文确定规划阶段、真实权限与可实现的受控接口，再技术审查 | `OPEN_ATOMIC_INTERFACE_AND_PHASE` |

**本批发现与下轮入口**：九组有可定位的候选合同链，但仍缺独立合同审签/验证；**§9.2 文件/业务结果链缺可签收的设备端原子接口及具体分期证据**，不得以 XP-22 的文字覆盖假装该功能已确定实现。W00 的其他 46 个 V15 行及 V8 99 小节/非 Heading2 附录 C/E/F 仍按原始索引审查，必须继续按既定 `Requirement→Contract→State/Failure→Test→Owner` 逐项补齐；**这十组不是“完成 10/56 条冻结”**。真实实现和测试一概 `NOT_RUN/NOT_AUTHORIZED`。

## 5. 与 V8 99 小节与 D1–D5 的下一道复核门禁

V8 附录 D 提供 56 项对应的主责章节/补充章节，已 56/56 精确匹配，故本表按 **V15 原章目**逐行建立责任映射。V8 全部 99 个 Heading2 是架构接口/状态/故障的下一层审签单位；**本轮未声称对 99 节的所有句子逐条映射到可编译 API**。正式 W00 在冻结申请前应补 `V8 § → D1–D5 ContractID → State/Failure/Test/Owner`，尤其 QtPrivate 版本单元、Runtime 资源限额、Grant/隐私线性化、Managed RoutePermit，以及**已经 Human 批准 P-A/A 的 Native 字节值域与仍未验证的状态/错误/负向对拍**。

**本轮输出**：`SOURCE_REQUIREMENTS_56=EXACT_INDEXED`; `PD_56_BEHAVIOR_ROWS=56_MAPPED_NOT_TESTED`; `HIGH_RISK_ATOMIC_CHAINS=9_LINKED_1_OPEN_NOT_SIGNED`; `V8_APPENDIX_D_56_TITLE_MATCH=PASS`; `V8_ARCH99_BEHAVIOR_TRACE=OPEN`; `GATE_D=NO`; `GATE_I=NO`. 不生成正式 Word、不改产品代码。
