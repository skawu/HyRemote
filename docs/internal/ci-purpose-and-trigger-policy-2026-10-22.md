# HyRemote CI: purpose, trigger and protected merge gate (Owner #573, 2026-10-22)

> **This replaces Owner #571's incorrect post-merge CI policy.** Feature-branch ordinary pushes do **not** invoke Qt CI. Opening or readying a pull request to protected `develop` invokes it. **CI must pass before the PR may merge.**

## When CI runs

| Action | Automatic CI? | Merge consequence |
| --- | --- | --- |
| A commit pushed on a feature branch **without a PR** | No | Development may continue without hosted Qt waits |
| Open a ready PR to `develop` | Yes | Wait for actual affected Windows Qt product tests plus governance |
| Open a draft PR | Lightweight CI classification only; Qt deferred | GitHub draft cannot be merged |
| Mark PR ready for review | Yes | Full selected PR validation starts |
| Push another commit to a **ready** open PR | Yes, cancels superseded run | Current PR head must pass; old green cannot authorize new commits |
| Docs-only or governance-only ready PR | Yes, but Qt build skipped | Stable `ci-merge-verdict` requires classifier and relevant governance PASS; no false product-PASS claim |
| Push/merge to `develop` | **No second Qt CI** | PR head was already validated before merging |
| Manually dispatch frozen release candidate | Yes, separate `workflow_dispatch` | Admitted `FULL_GATE / CANDIDATE`, not ordinary PR approval |

**Windows x86_64 Qt 6.8.3 is the mandatory hosted platform** (Owner #563). Linux desktop/ARM64/EGLFS/Wayland are user-validation pending and do not block development or releases. The historical manually-dispatched FULL_GATE matrix remains separate until its own reviewed rebaseline and must not silently overwrite the Windows-first PR scope.

## Fail-closed merge authority

A single stable GitHub Actions job, **`ci-merge-verdict`**, is the branch-protection target on `develop`. It checks, for the current ready PR head:

1. `Classify changes` completed successfully and reported `PR_FAST / MERGE`.
2. If product/build/tests paths are affected, the selected Windows Qt matrix `product` job(s) have actually **succeeded**. `skipped`, `cancelled`, `failure` or missing checks are **not** PASS. Product jobs must actually compile and execute nonzero CTest.
3. For affected release-authority paths, the `authority-governance` job completed successfully; no skipped governance claim is mislabelled PASS.
4. If no Qt product change exists, product jobs are intentionally skipped with truthful no-product-evidence scope. This is legitimate for docs-only changes; it is **not** proof of Qt product validity.

**Repository ruleset `protect develop` (ID 25459896) must require BOTH `p1-p2-disposition` and `ci-merge-verdict`.** Editing the YAML alone does not protect merges. If GitHub account/connector permissions cannot edit the ruleset, a repository administrator must add the exact check, choosing GitHub Actions as its source. Until verified in live ruleset, the repository is **NOT FULLY ENFORCING** the Owner's fail-closed contract. Do not override/bypass this check or merge a PR with unverified CI.

Git Flow topology and independent P1/P2 review-disposition still run as their separate premerge checks. No CI runs for each standalone local commit. For a PR that is already ready and receives new commits, re-running tests is necessary: merging untested new code would violate the acceptance contract.

## What CI proves and cannot prove

CI **can** show that the exact tested PR merge ref configures, compiles, has executed the selected automated tests, meets relevant SDK installation/consumer packaging contracts and the repository governance rules within the selected Windows reference setup. Green means **only those executed checks passed**.

CI **cannot** guarantee no latent bugs, real GUI Viewer input/keyboard/resize experience, physical local+remote coexistence without actual interaction, target Linux/ARM64/EGLFS/Wayland compatibility, all Qt5/6.12 Kit ABI combinations, Internet security of plaintext RFB, or Feature/RC/GA acceptance without their independent candidate evidence.

A failing PR CI is fixed on the same PR or withdrawn; **do not merge red and repair after merging as the normal workflow**. A new commit invalidates the old CI conclusion; cancelled superseded runs are normal and do not count as accepted tests. A release candidate separately needs exact-SHA FULL_GATE.

Refs #24 #95 #343 #563 #571 (superseded) #573. Authorities: `.github/workflows/ci.yml`, `.github/workflows/git-flow-policy.yml`, `.github/workflows/review-gate.yml`, `.github/scripts/resolve-ci-scope.py` and branch ruleset `protect develop`.
