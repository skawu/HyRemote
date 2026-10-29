# Qt LTS 长期维护架构：WBS 执行台账与今日缺陷处置对账（2026-10-24）

> **状态：PLANNED / 审核中的规划附件，不代表功能完成、产品已经认证或具体 Feature 版本已激活。**
> 产品版本排序 authority：[#594](https://github.com/skawu/HyRemote/issues/594) 与同分支 [产品路线图](product-version-delivery-roadmap-2026-10-23.md)；正式精确版本准入只由 [#24](https://github.com/skawu/HyRemote/issues/24)、[#95](https://github.com/skawu/HyRemote/issues/95) 及 `.github/release/release-trains.json` 决定。
> 本文只映射 **执行、验收、证据和责任**，不建立第二套 mandatory release authority。当前首个 GA 仅针对 Windows x86_64 **Qt 5.15 + Qt 6.8 的实际适用且已认证单元**；Qt 6.5 与 6.12 **严格 V1 GA 后**（#605）。Linux/ARM64/Wayland/EGLFS 实机验证不阻塞 Windows GA（#563），也不能推定 Supported。

## 0. 当前真实输入与缺口

- `v0.3.3.0` 已经于 2026-10-24 **合法重新发布**（tag `a352a3e`，release code head `d6cb758`），此前错误发布的撤回事件仍保留审计历史；详见 [勘误](../releases/v0.3.3.0-errata.md) 和 [正式 Release](https://github.com/skawu/HyRemote/releases/tag/v0.3.3.0)。release-head Windows Qt6.8.3 L2 为 85/85，部分 Viewer/认证/性能事实来自先前候选 `de94a0a`，非完全同一 HEAD；Release 没有二进制附件。不可声称不存在已验收 Release，也不可外推真实 Viewer 同 SHA 全覆盖。
- Qt5.15.2 Windows C++/Widgets (+ Tool) 当前已有 bounded 62/62 CTest 和部分安装/示例证据（[最新补充](windows-qt5-15-route-2026-10-24.md)、[适配基线](qt5-adaptation-2026-10-22.md)）；**Qt5 Quick 仍为 Experimental build-only，QML/Generic/QPA 没有完成相应资格认证**。
- `src/core` Qt-free，一套 `src/runtime`，四条对等 `src/integrations/{cpp,qml,generic,qpa}`；不接受按 Qt major 复制 Session/Frame/RFB/Runtime 源码。
- 当前顶层 `CMakeLists.txt:49-83` 将 Qt5 targets 命名成 `Qt6::*` aliases，并伪造 `Qt6_VERSION` 元数据；安装 `HyRemoteConfig.cmake.in` 有类似别名；`src/runtime/CMakeLists.txt:169-185` 的 Widgets/Quick 输入适配实际依赖 GuiPrivate。更换 Qt Kit 的官方 build 工具有身份文件防缓存混用，但 source/installed 的跨-major 负例仍需补上。
- 无 Qt6.5/6.12 实际产品资格证明；`.github/workflows/qt-lts-preflight.yml` 的 standalone 公共 API/link probe 不能代替 Runtime、安装消费者、原生 Viewer 合格。

## 1. WBS 台账（22 项，不以 WBS 编号分配版本数字）

**状态规则**：下列新工作包初始全部为 `PLANNED`；发生实际 PR 后单独登记 `IN_PR`，合并且测试/独立审查后 `CODE_DONE`，所有承诺真实产品单元的证据齐全才可 `QUALIFIED`。技术代码完成不能自动升级发布状态。负责 Issue 是工作归属而非“自动关闭整个 umbrella”。

| WBS | 交付工作包与可验证验收（必须能在 PR / CI / 独立 Review 中检查） | 当前归属 | 计划阶段 | 状态 |
| --- | --- | --- | --- | --- |
| **A01** | 冻结 exact Qt patch/Kit、编译器/ABI、OS/arch、UI Widgets/Quick、C++/QML/Generic/QPA、native QPA 逐单元表；`QUALIFIED/LIMITED/EXPERIMENTAL/UNSUPPORTED/NOT_APPLICABLE/NOT_RUN` 不混淆；源码或 preflight 不能提升认证。执行载体：中英文 `docs/compatibility.md` 的当前开发线表 + [Windows Qt LTS 路由矩阵](windows-qt-lts-route-matrix-2026-10-24.md) 的 exact Kit 身份，并由 `hyremote-release-readiness-compatibility-cells` 检查（拒绝 Qt5 Supported 升级、要求同语言同单元、内部矩阵与公开文档一致） | [#57](https://github.com/skawu/HyRemote/issues/57), [#603](https://github.com/skawu/HyRemote/issues/603) | 首个未编号 V0.3.x | CODE_DONE |
| **A02** | 有界 Qt 兼容 ADR：Core 不接 Qt、Shared Runtime 一套、四前端保持对等，Qt 版本差异只在接触边界；禁止第二个 Qt-major Runtime/API | [#209](https://github.com/skawu/HyRemote/issues/209) | 首个 V0.3.x 架构基础 | PLANNED |
| **A03** | 统一内部 Kit resolver：真实 Qt 版本、prefix、arch、编译器、模块及 route eligibility，保留 CLI/build.yml/env 选择优先级和无头歧义拒绝；6.5/6.12 检出不等于已认证；已有 selftest 不回退 | [#604](https://github.com/skawu/HyRemote/issues/604), [#566](https://github.com/skawu/HyRemote/issues/566) | 首个 V0.3.x | PLANNED |
| **A04** | 消除 `Qt6::* ALIAS Qt5::*` 和伪 Qt6 版本语义：Qt5/Qt6 真实 targets，Qt5/Qt6 已安装包不能错误混用，外部消费接口保持兼容；有两方向错-major 负例 | [#209](https://github.com/skawu/HyRemote/issues/209), [#265](https://github.com/skawu/HyRemote/issues/265) | 首个 V0.3.x | PLANNED |
| **A05** | Windows Qt5/Qt6 的 source `add_subdirectory` 与 installed `find_package` 最小消费者路径明确，正确 Kit 通过、错误 major/缺依赖清晰失败，不能假阳性退回 core-only | [#265](https://github.com/skawu/HyRemote/issues/265) | 首个 V0.3.x | PLANNED |
| **B01** | Qt5/6 MouseEvent 坐标、DPR、QCursor bitmap、mapToGlobal 等仅必要的小型内部 helper；Core/Session/RFB 处理无 `QT_VERSION` 条件，行为 fixtures 等价 | [#209](https://github.com/skawu/HyRemote/issues/209), [#265](https://github.com/skawu/HyRemote/issues/265) | 首个 V0.3.x | CODE_DONE |
| **B02** | Widgets/Quick 的 `GuiPrivate` 输入桥与 QPA 私有 ABI 清晰分层，实际 Qt5/Qt6 Kit 对应头/符号/注入行为及焦点、弹窗、输入释放、DPR 回归；public SDK 无 private 泄漏 | [#209](https://github.com/skawu/HyRemote/issues/209), [#401](https://github.com/skawu/HyRemote/issues/401)（已关闭的历史输入决策，仅参考） | 首个 V0.3.x | CODE_DONE |
| **B03** | Qt5 Widgets 实际输入/采集回归；Qt5 Quick 的 DPR/Pointer/QML import 缺口逐一修复或形成明确、可审查限制；Qt6.8 Widgets/Quick 不退化 | [#265](https://github.com/skawu/HyRemote/issues/265), [#57](https://github.com/skawu/HyRemote/issues/57) | 首个 V0.3.x | CODE_DONE |
| **C01** | `HyRemote::RemoteAccess` C++ API 的 Qt5/Qt6 consumer 和安装实例，生命周期/远程输入/Viewer 路径有等价实测；不改变用户公开产品模型 | [#265](https://github.com/skawu/HyRemote/issues/265), [#101](https://github.com/skawu/HyRemote/issues/101) | 首个 V0.3.x | CODE_DONE |
| **C02** | Qt5/Qt6 QML 接口可用范围，版本对应注册/import/deploy 正例与拒绝负例；Qt5 QML 未认证时不得声明已支持或无理由 N/A | [#265](https://github.com/skawu/HyRemote/issues/265), [#603](https://github.com/skawu/HyRemote/issues/603) | 首个 V0.3.x | CODE_DONE |
| **C03** | Generic 保留原生 `qwindows` 身份，Qt5/6 按适用单元 build/install/run；不可从 Qt6 的公共 API 兼容外推出 Qt5 插件行为 | [#265](https://github.com/skawu/HyRemote/issues/265), [#603](https://github.com/skawu/HyRemote/issues/603) | 首个 V0.3.x | CODE_DONE |
| **C04** | QPA 对 exact Qt patch/private ABI 编译静态断言、native delegate 和 install/plugin loader 正反例；未完成的 Qt5 QPA 只能标未认证/明确限制，不能混 ABI | [#265](https://github.com/skawu/HyRemote/issues/265), [#57](https://github.com/skawu/HyRemote/issues/57) | 首个 V0.3.x | CODE_DONE |
| **D01** | Qt5 `windeployqt` / Qt6 deployment helper，DLL、`qt.conf`、Qt plugins、Tool 与 Host 运行时归属以及 clean deployment 的可复现证据 | [#265](https://github.com/skawu/HyRemote/issues/265) | 首个 V0.3.x | CODE_DONE |
| **D02** | Qt5 qmake-friendly legacy consumer 真正读取已安装 CMake SDK/导出或定义受支持的低侵入集成方式；可复现构建/部署；不另建一套 SDK | [#265](https://github.com/skawu/HyRemote/issues/265) | 首个 V0.3.x | CODE_DONE |
| **D03** | Windows Qt5.15.2/Qt6.8.3 真实 Kit 受影响代码 CI、非零 CTest/安装消费者；Qt6.8 的四路线按适用范围测试；预检不能代替产品资格 | [#57](https://github.com/skawu/HyRemote/issues/57), [#603](https://github.com/skawu/HyRemote/issues/603), [#393](https://github.com/skawu/HyRemote/issues/393) | 首个 V0.3.x | CODE_DONE |
| **D04** | Kit 切换旧缓存、Qt5 SDK+Qt6 consumer 及反向、错误 patch QPA、未认证路线、缺 GuiPrivate/编译器、Core Qt-free 和版本特判扫描等 fail-closed 负例 | [#209](https://github.com/skawu/HyRemote/issues/209), [#604](https://github.com/skawu/HyRemote/issues/604) | 首个 V0.3.x | CODE_DONE |
| **D05** | exact SHA/Qt Kit/编译器的 Windows 原生应用与 maintained Viewer：Widgets/Quick、适用 C++/QML/Generic/QPA，RFB Raw/TRLE、DPR、popup、输入释放/重连、Qt 本地活性；物理未运行写 NOT_RUN | [#603](https://github.com/skawu/HyRemote/issues/603), [#57](https://github.com/skawu/HyRemote/issues/57) | V0.3.x 实测、V0.4 资格确认 | CODE_DONE |
| **E01** | 中英文兼容矩阵、诊断 Kit/SDK 精确依赖、Qt5/6 Private 事实与支持状态一致；没有从 preflight 自动推断 Supported | [#57](https://github.com/skawu/HyRemote/issues/57), [#209](https://github.com/skawu/HyRemote/issues/209) | V0.3.x 文档；V0.4 冻结 | CODE_DONE |
| **E02** | #602 清理 GA inherited 26-child machine 范围（保留真实 #57 Windows 责任），真实 release gate/independent P1-P2 review/current SHA、仅资格的 V0.4 → V1 exact tag/backmerge。执行载体：[GA 范围对账台账](v1-ga-scope-reconciliation-2026-10-28.md) + 独立 machine PR（mandatory 收敛为 11 条可关闭门禁、`scope_authority` 改 #594），固定期望与负例在 release-authority policy / release-scope self-test 中；#269 的真实合并门禁证据与负例见 [develop 合并门禁证据](review-gate-enforcement-2026-10-28.md)（4 条必需检查、原生线程解决、直接推送拒绝）；#266 贡献者文档措辞仍待同步 | [#602](https://github.com/skawu/HyRemote/issues/602), [#269](https://github.com/skawu/HyRemote/issues/269), [#95](https://github.com/skawu/HyRemote/issues/95) | V0.4 RC / V1 GA | IN_PR |
| **F01** | 已安装但未认证的 synthetic Qt Kit 能被识别且正确拒绝，完整新 family/patch 的候选准入模板；不得将探针当 Supported | [#605](https://github.com/skawu/HyRemote/issues/605), [#209](https://github.com/skawu/HyRemote/issues/209) | 架构模板可先行，实际新家族 GA 后 | PLANNED |
| **F02** | Qt6.5 与 Qt6.12 **各自单独** exact Kit/Qt API/Private/四路线适用矩阵、消费者、Viewer、CI，Owner 激活后才更新产品 Supported | [#605](https://github.com/skawu/HyRemote/issues/605) | **V1 GA 后** | PLANNED |
| **F03** | 旧 LTS 退役演练：deprecation/EOL Owner 声明、拒绝旧 Kit、新/剩余 Kit 正例，删只属旧版本的 shim/CI 配置，不修改已发布 tag/制品、不动 Core | [#605](https://github.com/skawu/HyRemote/issues/605), [#611](https://github.com/skawu/HyRemote/issues/611), [#209](https://github.com/skawu/HyRemote/issues/209) | GA 后维护政策 | PLANNED |

### 合并与证据更新规则

- **每个合并候选**必须声明 `WBS-ID`、关联 Issue、精确 HEAD、改动范围、受影响 Qt Kit/产品单元、正反例命令、测试数量、安装产物、风险/回退和独立审查结论。文档 PR 不可写成 CODE_DONE 或 QUALIFIED。
- 不要求为 22 个 WBS 强行创建 22 个独立 Issue。以上真实 Owner Issue 接受各自对应的执行 checklist；小型 Ready PR 按自然的可审查改动域拆分。
- 每个 `QUALIFIED` 单元须保存 `(qt_exact_version, compiler_abi, os, arch, ui, integration, qpa_exact_private_abi_if_any, commit, tests, clean_install, actual_viewer_or_not_run)`。缺任一声明必需的实测不得关成 PASS。

## 2. 今天新增代码缺陷的独立维护/阶段裁决

**绝不能将当前版本的确定性正确性/资源缺陷留到完整 Tool S1 完成，亦不能因加到 WBS 便宣称已修复。**

| Issue | 风险分类（均未通过本轮动态验证） | 执行与版本安排 | 独立关闭证据 |
| --- | --- | --- | --- |
| [#641](https://github.com/skawu/HyRemote/issues/641) | P1，已发 Tool RFB wheel mask 值错用为 button ordinal，撤销控制先禁止再发 KeyUp/MouseUp；焦点丢失安全行为需与 #582 协调 | **立即维护/hotfix 准入评估**（#95 决定确切 Maintenance 版本或修复车道）；不依赖完整 Tool S1 或新的 Qt LTS Feature | wire fake server：0x08/0x10 + drag held state 保留；输入先 release 后 revoke；失焦转只读、重连无自动控制；适用 Qt6/Qt5 Tool 回归及 Review |
| [#642](https://github.com/skawu/HyRemote/issues/642) | P1，Tool `readAll`、过长 RFB name/reason、非法 framebuffer size/Raw update 聚合内存、pixel format | **立即独立安全/稳定性维护准入评估**，不依赖 Tool UI 新功能；与 Host #158 分开修复 | fake hostile TCP：长度/分片/累计包/长宽/格式边界，有限 RSS/缓冲、及时 fail close、合法帧/重连 PASS |
| [#643](https://github.com/skawu/HyRemote/issues/643) | P2，Core frame `stride*height` unchecked overflow，Raw/TRLE/changed tiles | **GA 前正确性候选**，先用 deterministic/sanitizer 案例界定生产入口和风险，Owner 决定 hotfix 还是开发阶段修复；不得无限期悬空 | SIZE_MAX + invalid plane metadata、mapped row/width 安全拒绝、正常 padded frame PASS；Core Qt-free、Raw/TRLE 共享合法性检查 |
| [#644](https://github.com/skawu/HyRemote/issues/644) | P2，透明 / premultiplied 像素直接转 RFB opaque RGB；真实 Windows 视觉缺陷尚未复现 | **GA 支持范围裁决先行**：如透明/叠层场景属于当前声明，RC 前使用共享 alpha-to-opaque 政策纠正并实测；如超出声明，明确限制并按独立后续 admission；不自动 GA blocker | alpha=0/128/255 straight/premultiplied/opaque fixture，Raw/TRLE 等价解码；实机 Qt Widgets/Quick（适用时）或清楚限定支持 |
| [#158](https://github.com/skawu/HyRemote/issues/158) | Host readAll 先超内存 cap 才检查；RFB3.8/Normal-phase 占用 | **既有 V1 协议/资源正确性门禁**，RC 前解决独立 Host 负例；不能用 #642 Tool 测试代替 | burst/fragment/8 stalled slots/static incremental alive/3.8 only，Host 与快 Viewer 无资源泄漏 |
| [#609](https://github.com/skawu/HyRemote/issues/609) / [#370](https://github.com/skawu/HyRemote/issues/370) / [#374](https://github.com/skawu/HyRemote/issues/374) | 编码/diff/逐 Viewer 工作可能公平性和 CPU 问题，尚未有稳定 0/1/2/8 Viewer 测量 | **长期性能 + 现有 Windows 资格要求**；先 profile 再优化，禁止虚构 CPU SLO 或预先开发硬件编码 | Host/Tool 区分，0/1/2/8 viewers，W1–W4，CPU/RSS/帧龄与本地交互，前后 exact SHA evidence |
| [#611](https://github.com/skawu/HyRemote/issues/611) / [PR #612](https://github.com/skawu/HyRemote/pull/612) | LAN 监听/认证无加密/QtPrivate 与 SDK 依赖真实性 | **现有安全文档正确性**，同步合法重发 V0.3.3.0；长期支持/补丁政策独立后续维护 | 中文英文与 release errata、当前代码事实一致，独立 Review + current-head CI |
| [#269](https://github.com/skawu/HyRemote/issues/269) / [#602](https://github.com/skawu/HyRemote/issues/602) | review prose ≠ GitHub merge enforcement，GA legacy mandatory 混杂 | **RC/GA 前真实治理门禁**，#602 单独修 machine（本路线图不改 machine） | branch protection/机器 check 负例、每个 26-child 分类、closeable acceptance/independent review |

### 缺陷关闭三态（不能虚构成功）
1. **TRIAGED**：由对应已有 Issue 确认影响、优先级、立即修复或条件延后理由；#641/#642 先进入维护准入评估。
2. **FIXED_IN_PR**：有指向真实源码差异的独立 PR、正反例和受影响 Qt Kit CI，评审就绪；不能凭文档描述结论标成修复。
3. **ACCEPTED**：最新代码 HEAD 适用门禁 PASS、审查 P1/P2 清零、实际用户场景/Kit 正式验收且版本准入合法。对 #644 可以接受一个有证据的限制/不适用裁决，而不是偷换成“已修复”。

## 3. 执行依赖和分 PR 顺序

1. **Slice-1（A01/A02/A03）**：Qt Kit/资格单元合同 + 解析器，仅一条配置真相；保留现有可工作 Kit。可与现有 #604 Kit 自测合并为一个 focused PR。
2. **Slice-2（A04/A05/D04）**：先失败案例后逐步移除 Qt5/Qt6 target 伪装；Qt5/Qt6 source+installed consumer，混合 major 应 fail-closed。
3. **Slice-3（B01/B02）**：小 shim/QtPrivate 输入 ingress，Widgets/Quick 的焦点、popups、HiDPI 和 release 负例。
4. **Slice-4（B03/C01）**：真实 Qt5 Widgets/实验 Quick 的明确结论，Qt6.8 回归；消费者 ABI 正反例。
5. **Slice-5（C02/C03/C04）**：QML、Generic、QPA **按路线拆分不同 PR**，保留 exact-ABI QPA，Qt5 路线不得凭假 target 判已支持。
6. **Slice-6（D01/D02/D03）**：安装/deploy/qmake 消费者与 Windows 双 Kit 风险选择 CI；zero/skipped tests 不算绿。
7. **Slice-7（D05/E01）**：真实 Windows/Viewer/Qt 产品证据与精准中英文兼容表，RC 不接新的未完成实现。
8. **Slice-8（E02）**：#602 / #269 的**独立治理 PR**与 V0.4/V1 权威确认；绝不在 Qt 实现 PR 中顺手改 V1 mandatory 列表。
9. **GA 后（F01–F03）**：#605 各独立 Qt LTS add/remove 演练，先真实用例后提升状态，版本号由 Owner 准入。

**依赖门 G0→G4**：G0 合法矩阵与 Kit 可获得；G1 Kit/ABI/consumer fail-closed；G2 双 LTS 已承诺功能产品证据；G3 RC/GA only qualification + machine review；G4 新/旧 Qt family 策略与不损伤既有源码。每一个 Gate 都是未来任务，当前都不标 PASS。

## 4. 审查者 / 自动验证清单

- [ ] **产品范围**：没有改变已发布 V0.3.3、没有擅自激活 V0.3.x/GA、没有让 Qt6.5/6.12 成为 V1 前阻塞。
- [ ] **架构**：Core/Session/RFB 没有 Qt LTS 分支，没有第二 Runtime/第二发布规则/第二产品 API；Private Qt 仅在内部边界。
- [ ] **逐项完整性**：22 个 A01–F03 都有唯一 WBS ID、真实 Issue 归属、拟定阶段、正反例/验收和保留 PLANNED 状态。
- [ ] **已发缺陷**：#641/#642 及时维护准入；#643 条件安全确认；#644 透明/UI 产品范围裁决；Host #158 不被 Tool #642 覆盖。
- [ ] **真实证据**：说明 `Qt5 62/62` 与 `Qt6 85/85` 的特定 SHA/Kit 局限；无 NOT_RUN->PASS。
- [ ] **独立审查**：branch 与 base 最新合并拓扑，文件兼容审查、关联 Issue、CI (含 release-authority policy) latest head PASS，P1/P2 reviewed disposition。
- [ ] **交付闭环**：所有需要合入正式路线的文档实际存在并被索引；每条执行任务可定位到一个小 PR、相应问题单和证据；无遗漏无人认领。

规划参考：[#209 WBS 原始评审](https://github.com/skawu/HyRemote/issues/209#issuecomment-6378216769)；[今日覆盖对账 #594](https://github.com/skawu/HyRemote/issues/594#issuecomment-6378264759)。本台账不得覆盖它们的已完成事实和正式 GitHub Release 权威。
