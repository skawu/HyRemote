---
name: engineering-task-execution
description: Use for non-trivial HyRemote implementation, debugging, refactoring, review, build/test failures, or compatibility work. Establish the actual user requirement and current repository facts, execute continuously within approved scope, and close the loop with exact validation evidence.
---

# HyRemote engineering task execution

## 1. Establish the task contract

- Read root `AGENTS.md` and `CONTRIBUTING.md`; read the target Issue and latest authoritative comments before editing.
- Refresh current `develop`, inspect relevant code/tests/docs, and search for overlapping open Issue/PR ownership. Confirm the task has not been superseded or reclassified.
- For non-trivial work, state a compact contract in the working plan/PR: **user outcome, non-goals, affected routes/layers, API/Qt/platform implications, acceptance criteria, risks, and required evidence**. Separate confirmed facts from hypotheses.
- When a task is sufficiently bounded, resolve ordinary implementation details from current code and contracts and proceed. Ask Human only when an unresolved choice changes product scope/semantics, public API, architecture, security promises, release scope, or requires an irreversible action.

## 2. Continue through implementation and validation

- Execute the whole nonblocked path: inspect, reproduce, implement, test, diagnose failures, repair, re-test, review the diff, and record results. Do not stop after planning or a status summary while useful nonblocked work remains.
- Follow the existing root `AGENTS.md` Stop / Continuation Contract. `NOT_RUN`, a red test, a failed experiment, a pending review gate, or an OBSERVE CI run is an evidence/action gap, not by itself a reason to idle.
- If one route is blocked, record the precise blocker and continue with non-overlapping work or an automatable alternative. Preserve the existing useful progress-update rule for long-running local work; report actual phase, evidence, blocker, and next action rather than repeated empty status messages.
- Classify failures using evidence: task regression, pre-existing product defect, build-system defect, or environment/infrastructure. Fix issues within scope; otherwise create or update the appropriate defect follow-up and continue what is not blocked.
- Request Human action only after automatable alternatives are exhausted, and state the exact action/checklist, decision options, consequences, and recommended option.

## 3. Debug before fixing

- When a bug, test failure, or environment discrepancy appears, establish the failure first: read the exact error, reproduce it, identify the affected layer, and separate product, build-system, and environment causes before changing code.
- State one concrete hypothesis and run the smallest experiment that can disprove it. Do not stack unrelated fixes or treat a green test as proof of a root cause it did not exercise.
- After a failed experiment, record what changed in the evidence and update the hypothesis. Do not keep the same explanation after its prediction failed.
- These debugging-discipline concepts are adapted from `obra/superpowers`
  `systematic-debugging` v7.0.1, commit `2ce8aa15d05bd57213e6f746f953aafb7ded7a8b`,
  licensed under MIT. No upstream Skill dependency, installer, or worktree workflow is imported.

## 4. Prevent recurring defects

- Reproduce the reported defect before editing when feasible; preserve a failing case or diagnostic evidence.
- Add a focused regression test that fails before the fix and passes after it whenever feasible. Then run the targeted test and the required canonical verification.
- If the same class of failure has recurred, inspect prior Issues, review comments, and tests to find why it escaped. Change the narrowest effective test, checker, rule, or Skill. Do not add broad generic wording without an identified failure mode.

## 5. Validate against the actual product claim

- On Windows, use the canonical `build.cmd test` path from the configured repository root. On POSIX, use `sh ./build.cmd test`. Follow the task's applicable focused build/test and install/deploy requirements; do not create a parallel full-suite acceptance command.
- Preserve the four peer integration routes (C++, QML, Generic, QPA), the single Shared Runtime, public/private Qt API boundaries, and exact per-cell compatibility claims.
- A build/CTest pass does not prove physical/native/viewer/installed-consumer behavior unless the relevant evidence exists. Zero discovered/executed tests is invalid evidence.
- Review the exact diff and base/head identity. Report commands actually run, results, discovered/executed test counts, exact SHA where available, platform/Qt cells tested, unverified claims, known risks, and follow-up ownership. Label unavailable evidence `NOT_RUN` or `NOT VERIFIED`, never PASS.
