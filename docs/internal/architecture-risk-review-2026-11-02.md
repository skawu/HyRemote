# HyRemote 架构与风险审查报告

| 项 | 值 |
| --- | --- |
| 审查日期 | 2026-11-02 |
| 审查基线 | `develop` = `5f64e540`；候选释放 SHA `79f71b5c`（V0.3.5.0，未发布） |
| 审查环境 | Windows x86_64 / Qt 6.8.3 `msvc2022_64` / MSVC 14.44.35207 |
| 审查执行 | LOCAL OWNER（AGENT_PRIMARY_HUMAN_ASSISTED） |
| 方法 | 仓库度量（git 清单 + 行数 + 模式检索）、结构化抽样、以及本工作流内已完成的实证验收 |
| 结论 | 架构 A- / 选型 A- / 实现 B+ / 工程治理 A；**1 项 P1 未修（非 ASCII 参数）**，3 项 P2，5 项 P3 |

本报告只写**能被复现或已被实测确认**的内容。凡属推断，均在条目内标注"推断"。

---

## 1. 摘要

HyRemote 是一个把远程访问能力**零源码改动**注入既有 Qt 应用的产品：四个接入面（C++ 单头 API、QML 模块、Generic 插件、QPA 平台插件）共享同一套配置策略与宿主路径契约，失败一律 fail-closed 且**依赖范围化**（一个能力不可用不拖垮宿主进程）。代码量不大（C++ 33k 行），但工程治理密度极高（文档 20k 行、CMake 11.6k 行、79 条 CTest 身份、catalog 漂移守卫、G0–G6 执行手册、按影响面的 CI 选择）。

强项是**架构约束与测试治理**；弱项是**没有任何编译期/UB 工具门禁**，以及一条真实存在的 P1 编码缺陷：非 ASCII 水印文本在零代码路线上被静默损坏并照常渲染。

---

## 2. 仓库画像（实测）

| 维度 | 数值 |
| --- | --- |
| C++ 实现 | 130 个 `.cpp` / 33,010 行；47 个 `.hpp` / 3,998 行；4 个 `.h` / 476 行 |
| 构建 | 43 个 CMake 文件 / 11,643 行 |
| 文档 | 178 个 Markdown / 19,995 行 |
| CI 与工具 | 16 个 Python / 3,488 行；10 个工作流 |
| QML | 4 个文件 / 274 行 |
| 公共头文件 | **1 个**（`src/integrations/cpp/include/HyRemote/RemoteAccess.h`） |
| 测试身份 | Qt 6.8.3 四前端 **79** 条 CTest；Qt 5.15.2 C++/Widgets **56** 条；测试可执行程序 43 个 |
| 最大文件 | `test_rfb_wire_robustness.cpp` 2582、`rfb_transport.cpp` 1780、`access_instance.cpp` 1592、`session.cpp` 1099 |
| 风险标记 | `reinterpret_cast` 34、`memcpy` 7、`strcpy` 0、`TODO/FIXME` 0、`qFatal` 0、`goto` 0 |
| 错误处理风格 | `ErrorCode::` 148 处 vs `throw` 33 处 |

分布特征：`src/runtime` 86 个文件、`src/integrations` 53、`src/core` 33；`docs/internal` 66。文档与代码比约 0.6:1。

---

## 3. 架构评价

### 3.1 分层与依赖方向

```
core/          会话生命周期、状态机、mailbox（不含 RFB 协议细节）
runtime/       共享策略、宿主路径、RFB 传输实现、自动控制器、诊断快照
integrations/  cpp（唯一公共头）/ qml / generic（QPluginLoader）/ qpa（平台插件）
examples, tests
```

实测**零层间 include 逃逸**（`#include "../"` 在 core 与 runtime 中 0 命中），依赖方向单向。这是可维护性的第一道保障。

### 3.2 核心设计决定（优点）

1. **单一共享策略**：四条路线只提交"请求"，默认值/边界/文案集中在 `detail::watermark_policy` 与 `detail::host_paths`。四条路线在结构上不可能漂移出不同语义。
2. **零代码注入**：QPA 平台插件与 Generic `-plugin` 让客户应用不改一行代码即可被远程访问；这是产品差异点，也是最大的耦合风险点（见 R-07）。
3. **fail-closed 且依赖范围化**：状态目录不可用只拒绝出流、宿主继续运行；诊断区分 `InvalidConfiguration` 与 `StartFailed`，并给出可操作处置。
4. **唯一公共头 + pimpl**：ABI 面极小，升级风险可控。
5. **不可变快照 + 原子发布**：诊断读路径不加锁；写路径集中。

### 3.3 架构层面的结构性问题

- **同一事实三处表述**：CTest 注册、`tests/TEST_CATALOG.md`、分组运行器脚本各自描述"哪些用例存在"。已发生两起实证事故（R-06、R-08）。
- **传输层单文件 1780 行**同时承担协议编解码、像素格式协商、TRLE 编码、输入映射、多客户端状态。内聚但审计粒度差。
- **构建产物运行时定位靠"同目录有 DLL"**：示例与插件各自 copy 依赖（R-07）。

---

## 4. 技术选型评价

| 选择 | 评价 | 代价 |
| --- | --- | --- |
| Qt 5.15 + Qt 6.8 双轨 | 覆盖面正确，5.15 是大量存量项目的地基 | 所有共享代码双编译；两条 lane 的工具链差异会制造真实事故（R-05） |
| C++17 | 保守合理，`optional`/`atomic` 已够用 | 无 |
| CMake + Ninja + CTest | 正确；`build.cmd` 封装 G0–G6 分层，配合 label 与 catalog 守卫 | 治理脚本本身成为需要维护的资产 |
| 唯一公共头 + pimpl | 优秀，ABI 面最小 | 内部重构不受约束，外部无中途版本兼容承诺 |
| 零第三方依赖 | 优秀且罕见；RFB 全自研含 TRLE | 协议正确性完全自担，缺少 fuzz/静态分析时风险集中（R-02） |
| QPA 插件作为产品级注入点 | 聪明的零代码手段 | 插件加载失败会表现为无关的启动错误（R-07 实证） |
| 错误码优先（148 vs 33） | 适合库 | 混用边界未文档化（R-09） |
| Python 编排 CI 决策 | 务实，`resolve-ci-scope.py` 带自测 | 门禁决策是单点 |
| 无 `/W4`、无 `/WX`、无 sanitizer、无 clang-tidy | — | **本仓库最大的系统性薄弱**（R-02） |

---

## 5. 实现方式评价

**做得好的**

- 状态机显式（`AccessState` + projected state + revision），通知顺序有契约并被测试断言。
- 并发以"邮箱 + 单 worker 线程"为主，锁只保护队列本身（`core/src/detail/mailbox.cpp`）。
- 失败路径覆盖罕见地完整：越界策略、损坏策略文件、不可用状态目录、无合成器构建、目标缺失，全部有对应断言。
- 默认路径代价被实测：1920×1080 合成 **角标 0.66 ms/帧**、平铺 **14.1 ms/帧**（合成器单独值，不含采集/编码/投递）。

**做得不足的**：见第 6 节风险登记册。

---

## 6. 风险登记册

**状态图例**：`FIXED` 已修并合并；`OPEN` 未修；`MITIGATED` 有临时措施；`INFO` 记录性。

### P1

**R-01 非 ASCII 参数静默损坏（OPEN，已定位边界）**
命令行中非 ASCII 的水印文本被按 Latin-1 逐字节解释，GBK 中文变成 `Ë®Ó¡ºòÑ¡` 一类字符；共享策略视为合法可打印文本 → 落盘 → 合成器照常渲染。**无报错、无诊断**，`WATERMARK_TEXT_CODE_UNITS` 看起来仍然合理。
证据（候选 `79f71b5c` 部署副本实测）：

| 用例 | 落盘长度 | 与输入一致 | 实际码点 |
| --- | --- | --- | --- |
| Generic ASCII | 7 | 是 | `U+0057 U+0031 …` |
| Generic 中文 | **8** | 否 | `U+00CB U+00AE U+00D3 U+00A1 U+00BA U+00F2 U+00D1 U+00A1` |
| QPA 中文 | **8** | 否 | 与 Generic **逐字节相同** |

边界结论：**参数摄入阶段**，早于 Generic 的 `key=value` 解析与 Qt 的 `-platform` 令牌化——依据是 ASCII 在两路线均逐字符正确、损坏序列在两路线完全一致、且形态是"ANSI 字节逐字节放宽"而非 UTF-8 替换符。
影响：操作者以为设置了中文水印，实际发出乱码水印；危害高于"拒绝启动"。
证据落点：`docs/evidence/2026-11-02-761-cjk-argv/`（证据分支 `2fbe33f0`）。

### P2

**R-02 无编译器警告门禁 / 无 sanitizer / 无静态分析（OPEN）**
`/W4`、`/WX`、`-fsanitize`、`clang-tidy`、`cppcheck` 在 `cmake/`、`CMakeLists.txt`、`build.cmd` 中 **0 命中**。而代码含 34 处 `reinterpret_cast` 与手写 RFB 解码（`readU16/readU32/parsePixelSpec`）。当前仅靠功能测试兜底。

**R-03 关闭与清除只能靠"给一个无关参数"这种未文档化的副作用（OPEN）**
策略写入 `<状态目录>/watermark.policy` 后：省略全部水印参数 → 视为"本次未选择"→ **恢复**该策略（正确）；而只给 `watermark-position=…`（Generic）或 `hyremote-watermark-position=…`（QPA）、不给文字 → 控制器视为"提交了一个未启用的请求"→ 在 `start()` 中**清除**存储的策略。
实测（候选 `79f71b5c` 部署副本，同一状态目录）：先用 `watermark=SEED-MARK` 写入 → `stored=True`；再用仅 `watermark-position=bottom-right` 启动 → `stored=False`。
风险方向与最初判断相反：**不是"无法关闭"，而是"任何只调整位置/不透明度/字号/颜色的启动，都会静默关掉一个正在生效的水印"**，且没有任何诊断提示或文档记载。既没有显式 disable 令牌，也没有"部分参数=保持现状"的语义。

**R-04 传输层巨型 TU（OPEN）**
`rfb_transport.cpp` 1780 行同时承担 codec、像素格式、TRLE、输入映射、多客户端状态；`access_instance.cpp` 1592 行混合状态机、策略门禁、诊断装配。review 与定位成本高。

### P3

**R-05 Qt5 lane 的源文件编码假设（MITIGATED）**
Qt5 lane 不传 `/utf-8`，测试源中的 CJK 字面量变 cp1252 乱码 → 策略（正确地）拒绝 → 表现为"持久化不生效"。已改用码点构造规避（#747），但根因（构建配置未显式指定编码）仍在。

**R-06 分组运行器与注册不一致导致静默跳过（FIXED，`6cc0d922`）**
`run_qpa_runtime.cmake` 把未定义的可执行变量当 `STATUS` 跳过，`surfaces` 组跑 2 条却声称 3 条且长期绿。已改为缺可执行即 `FATAL`，仅条件夹具可跳过并计数上报。

**R-07 构建树产物依赖定位靠"同目录有 DLL"（FIXED，`fdf87dbf`；结构仍脆弱）**
Windows 从**应用目录**解析插件依赖，`hyremote-qmlplugin.dll` 找不到同目录的 `hyremote-qml.dll` → 构建树直跑 `exit 65` 无窗口。已用依赖跟踪的复制修复；但这是布局补丁，不是加载策略修复。

**R-08 文档与代码漂移（MITIGATED）**
catalog 头部数字陈旧（83/113/112）、交付账中两个合并 SHA 写错（#750 已修正并全表复核）。根因：关键事实被多处手写。

**R-09 错误处理边界未文档化（OPEN，低危）**
既有 `ErrorCode` + bool 返回，也有 33 处 `throw`；哪些 API 会抛、调用方是否必须捕获，没有契约文档。

**R-10 "减进程"不等于"减时间"（INFO）**
身份数 94→79 真实下降，但同一棵树三次 canonical 墙钟 **421.94 / 436.37 / 453.93 s**（±7%），长尾（example smoke ≈86 s、四个 installed consumer 40–55 s）未动。任何性能声明必须注明这一点。

---

## 7. 建议（按投入产出排序）

| # | 建议 | 对应风险 | 验收标准 | 预估 |
| --- | --- | --- | --- | --- |
| 1 | 入口处一次性从宽字符命令行构造 `QString`，两条零代码路线共用 | R-01 | 非 ASCII 值经 Generic/QPA 往返后与输入逐码点一致；新增回归用例 | 0.5 天 |
| 2 | Linux lane 加 `-fsanitize=address,undefined`；MSVC 侧启 `/W4`（先报告不阻断），逐步对核心目录 `/WX` | R-02 | CI 出现 sanitizer job；核心目录零警告 | 1 天 |
| 3 | 为 RFB 解析加 fuzz 目标（`parsePixelSpec`、消息头、TRLE 解码） | R-02/R-04 | 语料来自现有 wire 测试；固定时长 fuzz 无 crash 入 CI | 2–3 天 |
| 4 | 拆分 `rfb_transport.cpp` 为 codec / session / encoder 三层 | R-04 | 纯重构，行为不变，wire 测试全绿 | 2 天 |
| 5 | 显式 `watermark=off`（或 `disable`）令牌；"只给位置"不得再清空策略，并在诊断里报告"策略被本次启动清除" | R-03 | 三种启动（省略 / 仅位置 / 显式 off）各自行为有测试；清除事件有诊断行 | 1 天 |
| 6 | catalog 计数改由 `ctest --show-only=json-v1` 生成 | R-08 | 文档数字与注册清单不可能不一致（CI 生成 + 校验） | 1 天 |
| 7 | 显式给所有测试目标加 `/utf-8`（或等价 `-finput-charset`） | R-05 | Qt5 与 Qt6 lane 对同一源文件行为一致 | 0.5 天 |
| 8 | 文档化"哪些 API 抛异常"的契约 | R-09 | 公共头注释 + 一条契约测试 | 0.5 天 |
| 9 | 示例/插件的依赖定位改为 manifest 或显式 DLL 目录，而非逐目标复制 | R-07 | 构建树与部署布局共用同一加载策略 | 1 天 |

建议 1、2 应在本候选之后的**下一个补丁版本**前完成；3、4 属结构性投入，可与 V1 冻结前的稳定性窗口合并；5–9 可随时按切片推进。

---

## 8. 未覆盖与后续

- 未逐行审查 33k 行；核心协议实现的逐路径正确性只做了结构抽样。
- Qt5 分支的完整行为差异未复测（仅覆盖构建与测试通过）。
- 安装/打包脚本、`build.cmd` 全部分支未逐条审计。
- 建议下一轮以"R-02 工具链门禁"为切入点，因为它同时提高 34 处 `reinterpret_cast` 与手写解码的可信度，且不需要改动产品行为。
