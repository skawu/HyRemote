# V1 Repository Administration Gate

Status: **repository preparation / administration gate; not product acceptance evidence**

This document records the repository-level actions that must be complete before `v1.0.0.0` release authorization. These actions keep the GitHub repository governable and auditable; they do **not** replace #104 Windows/Linux product execution or #109 physical/native coexistence evidence.

## 1. Restore the four V1 authority workflows

During the 2026-09-17 Actions backlog, four high-fan-out workflows were temporarily disabled as a queue brake:

- `V1 GA acceptance`;
- `Declarative QML API`;
- `Transparent QPA Proxy`;
- `Git Flow policy`.

The V1 convergence branch now carries per-workflow/per-ref concurrency with `cancel-in-progress: true`, so stale runs are cancelled instead of accumulating. Before #104 can be accepted, restore the four workflows and verify they are `active`:

```powershell
pwsh .github/scripts/restore-v1-workflows.ps1
```

Activation itself is not test evidence. Only subsequent step-level executions on the exact accepted candidate count.

## 2. Finalize repository settings

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

## 3. Converge historical branch refs

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

## 4. Branch-protection/enforcement boundary

Repository API evidence on 2026-09-17 shows no required-status-check enforcement on `main` or `develop`. The Git Flow workflow, release-authority checks and mainline push audit therefore remain policy/audit controls rather than a substitute for server-side pre-push branch protection.

Until stronger server-side enforcement is deliberately configured, V1 release governance uses the documented `transitional-explicit` model:

- release work must use the documented PR topology;
- a direct-push audit failure on `main` or `develop` must be reconciled before release authority can close;
- no release PR may bypass the issue-state release authorities;
- the annotated release tag must point at the exact accepted `main` HEAD;
- post-release backmerge restores the `0.0.0` develop sentinel.

Do not describe the current workflow/audit layer as equivalent to branch protection.

## 5. Completion record

Before #33 authorizes `v1.0.0.0`, record the following repository facts in the release evidence envelope:

- [ ] four V1 authority workflows verified `active`;
- [ ] `delete_branch_on_merge=true` verified;
- [ ] private vulnerability reporting verified enabled;
- [ ] SHA-locked historical branch cleanup completed;
- [ ] only intended active/release refs remain;
- [ ] no unresolved direct-push audit failure on `main`/`develop`;
- [ ] #104 actual Windows/Linux acceptance complete;
- [ ] #109 physical/native coexistence evidence complete.

The first six items are repository governance readiness. The last two are product acceptance. Neither category may be used to substitute for the other.
