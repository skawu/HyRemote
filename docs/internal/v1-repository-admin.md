# V1 Repository Administration Gate

Status: **repository preparation / administration gate; not product acceptance evidence**

This document records the repository-level actions that must be complete before `v1.0.0.0` release authorization. These actions keep the GitHub repository governable and auditable; they do **not** replace #104 Windows/Linux product execution or #109 physical/native coexistence evidence.

## 1. Drain superseded V1 Actions runs

The 2026-09-17 V1 convergence work accumulated a large queue before per-workflow concurrency cancellation existed. The current workflows now cancel superseded runs on the same ref, but runs created from older workflow definitions can remain in the Actions queue.

First inspect the exact stale-run set without changing anything:

```powershell
pwsh .github/scripts/drain-superseded-v1-runs.ps1
```

The helper reads the current `feature/104-v1-ga-acceptance-matrix` HEAD and only marks queued/in-progress `pull_request` runs from older SHAs as stale. Current-HEAD runs are always retained. To cancel the reviewed stale set:

```powershell
pwsh .github/scripts/drain-superseded-v1-runs.ps1 -Execute
```

Before cancelling anything, the script re-reads the branch HEAD; if the branch moved, the operation aborts and must be re-run from a fresh snapshot.

## 2. Restore the four V1 authority workflows

During the Actions backlog, four high-fan-out workflows were temporarily disabled as a queue brake:

- `V1 GA acceptance`;
- `Declarative QML API`;
- `Transparent QPA Proxy`;
- `Git Flow policy`.

The V1 convergence branch now carries per-workflow/per-ref concurrency with `cancel-in-progress: true`, so new stale runs are cancelled instead of accumulating. After the superseded-run drain, restore the four workflows and verify they are `active`:

```powershell
pwsh .github/scripts/restore-v1-workflows.ps1
```

Activation itself is not test evidence. Only subsequent step-level executions on the exact accepted candidate count.

## 3. Finalize repository settings

Run the repository settings helper from an authenticated administrator environment:

```powershell
pwsh .github/scripts/finalize-v1-repository-settings.ps1
```

It verifies/enforces:

- the four V1 authority workflows are active;
- the default branch remains `main`;
- the repository remains public for the V1 publication line;
- merged PR head branches are deleted automatically (`delete_branch_on_merge=true`);
- GitHub private vulnerability reporting is enabled.

Private vulnerability reporting is the preferred sensitive-report path documented by `SECURITY.md`; do not claim it is available until the repository setting is actually enabled and verified.

## 4. Converge historical branch refs

Branch history belongs in commits, PRs, issues and tags; task branches are temporary work cursors. The one-time cleanup manifest is SHA-locked and refuses to delete any ref that moved or became an open PR head.

First run a dry-run:

```powershell
pwsh .github/scripts/prune-stale-branches.ps1
```

After reviewing the preflight output, perform the cleanup:

```powershell
pwsh .github/scripts/prune-stale-branches.ps1 -Execute
```

The retained long/current refs for the V1 convergence period are:

```text
main
develop
feature/104-v1-ga-acceptance-matrix
```

After #106 is merged and its lifecycle ends, its head branch should also be deleted.

## 5. Branch-protection/enforcement boundary

Repository API evidence on 2026-09-17 shows no required-status-check enforcement on `main` or `develop`. The Git Flow workflow, release-authority checks and mainline push audit therefore remain policy/audit controls rather than a substitute for server-side pre-push branch protection.

Until stronger server-side enforcement is deliberately configured, V1 release governance uses the documented `transitional-explicit` model:

- release work must use the documented PR topology;
- a direct-push audit failure on `main` or `develop` must be reconciled before release authority can close;
- no release PR may bypass the issue-state release authorities;
- the annotated release tag must point at the exact accepted `main` HEAD;
- post-release backmerge restores the `0.0.0` develop sentinel.

Do not describe the current workflow/audit layer as equivalent to branch protection.

## 6. Completion record

Before #33 authorizes `v1.0.0.0`, record the following repository facts in the release evidence envelope:

- [ ] superseded pre-concurrency V1 Actions runs drained without cancelling current-HEAD runs;
- [ ] four V1 authority workflows verified `active`;
- [ ] `delete_branch_on_merge=true` verified;
- [ ] private vulnerability reporting verified enabled;
- [ ] SHA-locked historical branch cleanup completed;
- [ ] only intended active/release refs remain;
- [ ] no unresolved direct-push audit failure on `main`/`develop`;
- [ ] #104 actual Windows/Linux acceptance complete;
- [ ] #109 physical/native coexistence evidence complete.

The first seven items are repository governance readiness. The last two are product acceptance. Neither category may be used to substitute for the other.
