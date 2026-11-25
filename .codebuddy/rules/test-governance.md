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

- **Agent owns test selection.** Before changing or running tests, inspect the affected behavior and existing tests once per task. Reuse the lowest-cost deterministic case that proves the risk, then run only the affected scope during iteration. Adding a new test case requires a named uncovered defect/contract and a failing oracle; prefer extending an existing suite. A new CTest executable, mock protocol server, matrix, workflow or test helper requires a distinct technical necessity, documented briefly in its PR. Do not seek Human approval for routine test design/runs or ask Human to relay commands; Human participates only in exceptional security/product decisions or unavoidable physical GUI work. Mandatory release/CI/ABI/security gates remain in force. Never rerun until green or add tests solely to increase the count.
- Do not launch visible GUI applications on the Human's desktop without asking
  first, and never repeatedly restart the same window. Prefer offscreen, CLI
  diagnostics or CI evidence. Keep a single instance, close it when done, and
  report it. Do not steal focus (`SetForegroundWindow`/`AttachThreadInput`).
- Do not poll or block on CI while independent authorized work exists. Inspect required checks at merge/release milestones; only when no nondependent work remains may a bounded existing watch be used. Do not rerun passing checks to satisfy a ritual.
- Windows physical work: `.ps1` files with ASCII content invoked via
  `-File`; cross-call long-lived processes via
  `Invoke-CimMethod -ClassName Win32_Process -MethodName Create`;
  `PrintWindow` needs `System.Drawing` and a try/finally resume. Never call
  `PrintWindow` on a suspended process.
- On this machine, static CPU A1/A2 drift above 15% means UNSTABLE: stop
  performance claims instead of resampling until it passes. Do not change the
  250ms probe, performance thresholds or product runtime to make a test pass.
