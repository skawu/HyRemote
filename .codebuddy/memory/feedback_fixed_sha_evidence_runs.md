---
name: 固定 SHA 证据/性能矩阵长任务的落地方式
description: HyRemote 里按固定 develop SHA 跑真实性能/RealVNC 矩阵时，checkout、脚本、统计与证据提交的可行路径与硬约束
type: feedback
---

按固定 SHA 跑真实性能矩阵（如 #962 的 RealVNC 0/1/2 Viewer A1/B/A2）时，按下面这套路径做，不要临场发明：

- **干净 checkout 只能建在 `tmp/` 下的 `git worktree`**（例：`tmp/win962/final`），主工作区与唯一 `build/` 不动；测完 `git worktree remove` + `prune` 收口。
- **但 hook 禁止在 `tmp/` 下新建脚本**（`PreToolUse` 直接拒绝，理由是 tmp 只放运行产物）。所以：
  - 已有采集工具用 `Start-Process -FilePath git -ArgumentList cat-file blob <sha>:<path> -RedirectStandardOutput <file>` **按字节精确恢复**（`git show >` 或 `Out-File` 会改 CRLF/BOM，导致哈希与原归档不一致）；
  - 统计/汇总用**内联 python**（PowerShell here-string 单引号块 + `python -c $code`）复算，不落自研脚本文件。
- **证据落地**：`docs/internal/evidence/<topic>-<sha8>/`（`summary.md` + `matrix-summary.csv` + `raw/*.json`）。超大 trace/日志留在 `tmp/` 不入库。
- **任何新增 `.py/.ps1/.sh`（含把归档工具恢复进仓库）必须同步登记 `tests/release-readiness/script_inventory.txt`**，否则 `check_script_discipline.cmake` 因 declared≠tracked 直接判红，`ci-merge-verdict` 会挡住合并。
- PR 分支名只允许 `feature/<issue>-<topic>` 等四个 Git Flow 家族；`evidence/*`、`docs/*` 会被 `check-pr-branch-name.sh` 拒。

**Why:** 2026-11-12 执行 #962 矩阵时实测：写 `tmp/.../summarize_*.py` 被 hook 拒绝；漏登记脚本清单会让治理 lane 变红；`git show` 重定向得到的文件比归档多出 BOM/CRLF，字节哈希不一致。

**How to apply:** 下次接同类"固定 SHA + 真实 Viewer/实机性能"任务时，先按此准备 checkout、工具与统计方式，再启动长跑；证据提交前先本地跑 `cmake -P tests/release-readiness/check_script_discipline.cmake` 自检。
