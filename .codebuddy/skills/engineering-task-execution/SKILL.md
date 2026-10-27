---
name: engineering-task-execution
description: Use for non-trivial engineering implementation, debugging, refactoring, review, build/test failures, or compatibility work. Establish the accepted requirement and current project facts, execute continuously within approved scope, and close the loop with exact validation evidence.
---

# Engineering task execution

## 1. Establish the task contract

- Read the project's effective instruction entry and the task's authoritative record. Do not assume a particular filename, issue tracker, branch name, or status vocabulary.
- Establish the task's accepted baseline: use the branch, revision, environment, or artifact the task authorizes, not a default branch or latest revision chosen by the Agent.
- Inspect relevant code, tests, configuration, and overlapping work before editing. Confirm the task has not been superseded or reclassified.
- For non-trivial work, state a compact contract: **user outcome, non-goals, affected layers/interfaces, acceptance criteria, risks, and required evidence**. Separate confirmed facts from hypotheses.
- When a task is sufficiently bounded, resolve ordinary implementation details from current project evidence and proceed. Ask the Human only when an unresolved choice changes product scope/semantics, public API, architecture, security promises, release scope, or requires an irreversible action.
- If a required policy or fact is missing, inspect existing project configuration and authoritative records. State what remains unknown; do not invent a convention.

## 2. Continue through implementation and validation

- Execute the whole nonblocked path: inspect, reproduce, implement, test, diagnose failures, repair, re-test, review the diff, and record results. Do not stop after planning or a status summary while useful nonblocked work remains.
- Follow the project's declared continuation, progress, and decision policies. If none are declared, continue nonblocked work and report meaningful progress; a failed experiment or pending review is an evidence/action gap, not by itself a reason to idle.
- If one route is blocked, record the precise blocker and continue with non-overlapping work or an automatable alternative. Report actual phase, evidence, blocker, and next action rather than repeated empty status messages.
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
- Add a focused regression test that fails before the fix and passes after it whenever feasible. Then run the targeted test and the applicable project verification.
- If the same class of failure has recurred, inspect prior task records, reviews, and tests to find why it escaped. Change the narrowest effective test, checker, rule, or Skill. Do not add broad generic wording without an identified failure mode.

## 5. Validate against the actual claim

- Use the project's declared build/test/verification entry and evidence policy. If no entry is declared, inspect existing project configuration and report the gap rather than inventing a command.
- Preserve the project's declared architecture, interfaces, compatibility, and safety boundaries.
- A host-only or partial check does not prove physical/native/target behavior unless the relevant evidence exists. Zero executed tests is invalid evidence.
- Review the exact diff and the accepted baseline identity. Report commands actually run, results, executed-test counts, revision/environment, unverified claims, and known risks.
- Record unavailable evidence explicitly; never present it as a pass.
