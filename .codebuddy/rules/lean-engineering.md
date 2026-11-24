---
description: 默认生效的精简研发约束：删除无必要的抽象、测试与工作流；禁止用测试数量代替产品结果。
alwaysApply: true
enabled: true
---

# Lean engineering — apply to every code task

Authority: root `AGENTS.md`, approved product design and current risk-based test strategy still win. This rule **reduces unnecessary work**; it never waives a required security, compatibility or release gate.

1. **Implement the approved behavior, not an anticipated framework.** Before writing a helper, adapter, state machine or new public API, check the existing Core/Runtime/Qt mechanism. Add an abstraction only when it removes real duplication or isolates a verified boundary; keep Qt types and platform branches in their existing layers. Do not invent reusable infrastructure for a single caller.
2. **Use bounded, meaningful failure handling.** Handle concrete API failures and lifecycle/security boundaries. Do not add chains of redundant checks, blanket catch-and-ignore, duplicated state flags or speculative future-compatibility fallbacks. Fail closed where required; never trade safety for shorter code.
3. **Tests are not deliverables by count.** First identify the existing suite that already proves the changed behavior. If it covers the risk, add **no new test**. If coverage is missing, extend that suite with the smallest deterministic case; create another executable, mock server, mirror implementation, test-only hook, workflow or test framework only when the existing seam cannot prove the named risk. A script-peer/mirror test cannot prove actual installed Runtime↔Viewer behavior.
4. **Use risk-selected local feedback.** Run the smallest relevant tests during iteration. Run existing mandatory PR, CI, security, ABI and release gates when applicable; do not build a new matrix merely to demonstrate diligence. A failing test is a reason to fix the root cause, not to endlessly grow tests or relax its oracle.
5. **Before PR, review the diff for subtraction.** Remove needless layers, extra files, duplicated helpers, diagnostic switches, stale tests and commentary that restates code. In the PR summary state only: product behavior changed; why each *new* test identity/file was indispensable (or `none`); existing tests reused; relevant gate outcome and untested boundaries. A test-only/research PR needs a distinct, current Issue rationale; do not spawn chained pilot PRs for one feature.
6. **One owner, one coherent change.** Prefer a focused implementation+needed regression in one PR. Don't create review-round PRs or tests-of-tests-of-tests. If an unrelated defect appears, log it and let its own owner decide; don't broaden the current feature.
7. **Workflows are shared CI infrastructure, not task artifacts.** Reuse `ci.yml`, `scheduled-breadth.yml`, `build.cmd` and the existing rules-first inventory before proposing any workflow add/change/retirement. A change must name its evidence consumer, preserve the four required develop checks and release protection, fail closed on missing evidence, and keep destructive scheduled jobs as check-only with an explicit manual execution entry. No temporary workflow per Issue, experiment or diagnostic. During development prefer local incremental build/tests; after PR submission continue non-dependent work and inspect gates only at merge/release milestones, never poll or rerun to hide a real failure.

Never claim performance, cross-platform support or security qualification from source review, fakes or green unit tests alone. Missing real-world evidence stays `NOT_RUN`.
