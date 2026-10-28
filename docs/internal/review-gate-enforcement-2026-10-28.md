# develop 合并门禁的真实强制证据（#269 / WBS E02）

Status: **已执行的负例记录 + 规则快照**。记录的是仓库事实，不是流程承诺；规则文本本身在 GitHub
ruleset `25459896`，本文只是当次快照与负例输出。

- Owner 责任：#269（评审 prose ≠ 合并强制）、#209（治理父项）、#602（E02）。
- 执行日期：2026-10-28。基线：`develop@f3c9c19`。
- 关联：[GA 范围对账](v1-ga-scope-reconciliation-2026-10-28.md)、`AGENTS.md` 第 8 节。

## 1. 规则快照（`protect develop`，ruleset id 25459896）

取用命令：

```bash
gh api repos/skawu/HyRemote/rulesets/25459896
gh api repos/skawu/HyRemote/rules/branches/develop
```

2026-10-28 更新前后的差异：

| 字段 | 更新前 | 更新后（现在） |
| --- | --- | --- |
| `enforcement` | active | active |
| `conditions.ref_name.include` | `refs/heads/develop` | `refs/heads/develop` |
| `deletion` / `non_fast_forward` | 有 | 有 |
| `pull_request.required_approving_review_count` | 0 | 0（不要求虚构的自我 approval） |
| `pull_request.required_review_thread_resolution` | **false** | **true** |
| `required_status_checks` contexts | `p1-p2-disposition`, `ci-merge-verdict` | 追加 `pull-request-topology`, `release-authority-scope`（共 4 条） |
| `strict_required_status_checks_policy` | false | false（见 §4 限制） |
| `bypass_actors` | `[]` | `[]` |
| `current_user_can_bypass` | `never` | `never` |

为什么追加这两条：`#269` 要求 Git Flow 拓扑与仓库自有检查也绑定到合并，而在此之前只有
review-disposition 与 CI 汇总裁决是必需的，分支命名/拓扑即使失败也不阻断合并。这两个 check 在
#693–#699 的每个 PR（含 docs-only）都实际运行过，因此新增要求不会让某类 PR 永久卡死。

`main` / `release/*` 的规则集 `23576792` **未被修改**（仍是 deletion + non_fast_forward +
pull_request，无 required status checks），见 §4 的已知缺口。

## 2. 负例 1：直接更新 `develop` 被拒绝

在 `develop` 祖先上建临时分支并提交一个空提交，直接推送：

```text
$ git push origin HEAD:develop
remote: error: GH013: Repository rule violations found for refs/heads/develop.
remote: - Changes must be made through a pull request.
remote: - 4 of 4 required status checks are expected.
! [remote rejected] HEAD -> develop (push declined due to repository rule violations)
```

- 结果：拒绝。`develop` 未产生任何提交（临时分支与提交已删除，未推送成功）。
- 该输出同时证明两条规则同时生效：PR-only 与 4 条必需检查。

## 3. 负例 2/3/4：PR 上的真实阻断（历史事实，同一仓库）

| 场景 | 观察 | 结论 |
| --- | --- | --- |
| 未解决的评审线程 | PR #696 的 `p1-p2-disposition` 失败，日志点名 `src/runtime/tests/test_rfb_wire_robustness.cpp:1836`；`mergeStateStatus=BLOCKED` | 线程未处置时不可合并（当时由仓库检查强制；更新后 ruleset 原生也强制） |
| 重复检查被 concurrency 取消 | PR #696/#697 同一 SHA 上多个 `cancelled` 同名 check 使 rollup=`FAILURE`，`gh pr merge` 报 "base branch policy prohibits the merge" | 任一必需检查非 success（含 cancelled）即阻断；逐条重跑清除后才 `CLEAN` |
| 产品 lane 失败 | PR #696 首轮 Windows Qt6 lane 失败（latency probe 抖动）→ `ci-merge-verdict` failure → 不可合并；同 SHA 重跑通过后才合并 | CI 失败不可用旧结果放行 |

## 4. 已知限制（如实记录，不宣称更强）

1. **`strict_required_status_checks_policy=false`**：每次 push 都会产生绑定新 head 的新检查，旧 head 的
   绿色不复用；但 PR 分支不要求先追上 `develop`。因此"新 head 使旧资格失效"成立，"base 过期"不被强制。
2. **不要求人工 approval**（`required_approving_review_count=0`）：本仓库是单 Owner 仓库，要求自我
   approval 只会变成仪式；阻断依靠机器检查与线程处置，与 #269 的目标一致。
3. **`main` / `release/*` 没有 required status checks**：发布 PR 不走 `develop` 的 CI 触发路径，若直接
   照搬 4 条必需检查会永久卡死发布 PR。发布证据仍由同 SHA 的 L2 dispatch、#95 放行和 tag 审计承担；
   这是 #95 的待办，不在本记录里冒充已强制。
4. **管理员可修改规则本身**：`bypass_actors` 为空、`current_user_can_bypass=never` 只表示合并时无人可
   绕过；能改 ruleset 的账号仍可改规则。审计依赖 ruleset 的 `updated_at` 与提交历史。
5. **#266 的贡献者文档措辞**尚未按本次规则更新（不在本记录范围），当前描述以
   `AGENTS.md` 第 8 节与本文为准。

## 5. #269 验收对照

| #269 项 | 状态 | 依据 |
| --- | --- | --- |
| 1. `develop` 有生效规则 | 满足 | ruleset 25459896，`enforcement=active` |
| 2. 直接推送被拒绝而非事后审计 | 满足 | §2 的 GH013 拒绝 |
| 3. 未解决线程不能合并 | 满足 | §3 第 1 行 + 规则原生 `required_review_thread_resolution=true` |
| 4. Git Flow + CI 检查绑定到 head | 满足 | 4 条必需 checks；每次 push 重跑 |
| 5. 新 head 使旧资格失效 | 部分满足 | head 绑定成立；`strict=false` 见 §4.1 |
| 6. 小规模负例证明阻断且未削弱 main | 满足 | §2、§3；`main` 规则集未改动 |
| 7. #266 贡献者文档描述真实规则 | 未完成 | 见 §4.5 |
