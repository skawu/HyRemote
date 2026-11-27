# #962 固定 SHA RealVNC 采样

本目录独立保存采集工具。在 Windows MSVC x64 开发环境、活动桌面、Qt 6.8.3、RealVNC Viewer 7.15.1 和 Python 3.11+（含 psutil）运行。被测源码 checkout 保持干净，并固定为通过保护合并后的 develop SHA。

```powershell
./run_fixed_matrix.ps1 -Source <clean-checkout> -ExpectedSha <40-hex-develop-SHA> -Out <new-output-directory>
```

每组 120 秒，依次执行 1/0/2 Viewer 的 A1/B/A2。保存 CMakeCache、Host/Runtime/Viewer SHA-256、逐样本 QPC、Host RSS/CPU、GPU 引擎计数、Qt 本地 frameSwapped/WM_KEYDOWN 送达、Runtime 采集和 RFB trace。Viewer 采用原有合成桌面采样，物理 present 记 NOT_MEASURED。0 Viewer 的启动采集与稳态分别统计。先检查 B 确实有 GPU 读回且无公共 grab，A 确实有公共 grab，实际 RFB 活跃 Viewer 数与计划一致。合并前短跑只验证工具。

Windows 短跑已确认原生 Intel UHD 770 OpenGL、2 个同时连接的实际 RealVNC 窗口、0 Viewer 的零稳态采集，以及本地 Qt 输入、swap、CPU 和 RSS 采样。在本次环境，Windows GPU Engine 通配查询返回其他 PID 计数，未返回当前宿主 PID，Host GPU 利用率不可用。保留 CSV 供诊断，缺值不填 0。不要把短跑候选数据或 #964 归入最终 SHA。
