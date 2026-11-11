---
description: 测试事前治理与实机/GUI 约束。在新增、修改或执行任何测试、实机场景、压力扫描，或准备启动可见窗口、等待 CI 时加载。
alwaysApply: false
enabled: true
paths:
  - "**/tests/**"
  - "**/*test*"
  - "tools/perf/**"
  - ".github/workflows/**"
---

# Test governance and physical-measurement discipline

Root `AGENTS.md` remains the single execution entry.

- Before adding, modifying or executing any test (CTest, planned registered
  script, physical scenario, stress/scan), justify it item by item and obtain
  Human confirmation. A throwaway scratch script is not an admissible test
  type; see `script-planning/SKILL.md`. Each item must state: (1) name and concrete goal; (2) the real
  risk or requirement; (3) why existing evidence, code review or current
  tests cannot replace it; (4) exact timing, environment, scope and maximum
  number of runs; (5) PASS/FAIL criteria, cost of not running, and expected
  cost. Run only the confirmed scope. Do not widen a matrix, repeat a full
  suite, or keep re-testing until a result looks good. Code review, QtPrivate/
  QRhi source analysis and non-test engineering fixes remain autonomous.
- Do not launch visible GUI applications on the Human's desktop without asking
  first, and never repeatedly restart the same window. Prefer offscreen, CLI
  diagnostics or CI evidence. Keep a single instance, close it when done, and
  report it. Do not steal focus (`SetForegroundWindow`/`AttachThreadInput`).
- Do not use fixed long sleeps to wait for CI; use
  `gh pr checks --watch --interval 10` / `gh run watch` and exit on completion.
- Windows physical work: `.ps1` files with ASCII content invoked via
  `-File`; cross-call long-lived processes via
  `Invoke-CimMethod -ClassName Win32_Process -MethodName Create`;
  `PrintWindow` needs `System.Drawing` and a try/finally resume. Never call
  `PrintWindow` on a suspended process.
- On this machine, static CPU A1/A2 drift above 15% means UNSTABLE: stop
  performance claims instead of resampling until it passes. Do not change the
  250ms probe, performance thresholds or product runtime to make a test pass.
