# 本地性能实验：固定命令、单次执行、有输出

此目录仅保留 **#609 调试期间**直接必要的两个工具：
- `aggregate_cpu_probe.py`：汇总 Host 插桩数据；
- `run_case.py`：运行**一个**已命名、已配置的本地场景，显示进度与终止状态。

**构建始终使用仓库现有的 `build.cmd` + `build.yml` / 已批准 Qt Kit profile。不要用本工具包装 CMake、CTest、安装或发布资格测试。** 教学 `examples/learning/01..05` 只在发布前执行产品化安装/连接验收；此处不是教学例程的测试入口。

## 只做一次本地配置

在 Windows 工作树根目录：

```powershell
Copy-Item tools/perf/cases.example.json tools/perf/cases.local.json
```

修改 `cases.local.json`，将已有的**确切**前后版本可执行程序路径、工作目录、参数替换为当地真实值。该文件是本地机配置，已加入 `.gitignore`，不得提交绝对路径、账号、密钥、真实 Viewer 地址。

`command` 必须是**前台运行、自己退出**的单个命令及参数数组；不调用 `Start-Job`、`Start-Process` 后立即退出、`&` 后台执行，也不借一条命令串联多个案例。已有的临时 `probe_run.ps1` 若仍是必需的 Viewer 编排，可在配置中作为单一前台命令的目标，但它须有明确退出/清理语义；不能把脚本已退出但外部后台进程仍存活解释成完成。

## 日常固定入口（一次只跑一个场景）

```powershell
python -u tools/perf/run_case.py --case anim-before
# 看完结果后，人或 Agent 再决定是否调用下一格；不会自动运行
python -u tools/perf/run_case.py --case anim-after
```

两次 A/B 必须使用相同场景、Qt Kit、1920×1080、已有 Harness、**恰好一个实际 Viewer**。额外的 Viewer 帧率观察者会改变 Host 编码/CPU，不能与单 Viewer CPU 基线混用。

运行时终端每约 **5 秒**明确打印：
- `[CASE START]` / `[CASE PID]`：已确认启动的是哪个案例和子进程；
- `[CASE PROGRESS]`：已运行秒数、硬期限、PID 是否仍存活、新增日志字节数；
- `[CASE LOG]`：实际子进程最近输出的最多 3 行（有新增才显示）；
- `[CASE END]`：进程退出、非零返回、超时或取消及保留证据位置。

每次在 **`build/perf-results/<UTC>-<case>/`** 增量记录 `status.json` 和 `output.log`，该目录在现有 `.gitignore` 的 `/build/` 下。局部中断仍保留已取得的信息。当前每案例默认 120 秒、允许配置 10～300 秒**总运行期限**；这是一次有界性能实验的期限，**不是**对 `build.cmd` 的“60 秒不输出就杀掉”规定。到期仅终止本案例管理的进程树；不得对全系统或其他 Agent 的进程执行广域 `taskkill`。退出 0 只证明进程正常结束，**不代表 Viewer 画面正确或性能通过**，需另附真实画面及 CPU/FPS 证据。

## Agent 操作约束

1. 开始前明确源 SHA、路线、场景、Viewer 数、预计单格期限和所使用的已固定 profile。
2. 执行期间不得静默转后台；若控制台平台强制转后台，Agent 必须主动检查其 PID、保存的状态文件和日志，按仓库 `AGENTS.md` 的 60 秒可见进展规则报告。连续无法观察真实状态时，停止继续派发，而不是当作正常执行。
3. **单格完成并解释结果之后**才决定是否启动下一格。不要恢复此前无反馈的 16 格串行矩阵。
4. 如执行异常，诚实报告 `TIMEOUT` / `CANCELLED` / `NOT_RUN`，不填估算。短时 Windows 动态试验的合格标准同时需要 Host CPU、画面实际变化/新鲜度和本地 UI 响应，不得以降低帧率换 CPU。
5. 这是可复用的**一次任务运行方式**，不引入新 CI、额外自动任务调度或产品运行依赖。

最后更新 #374 的脱敏实测和 #609 的结论；原始个人运行日志只保留在本地。
