# Branch Lifecycle and Retention

HyRemote treats Git branches as **temporary work cursors**, not permanent archive objects. Durable history lives in Issues, Pull Requests, commits, release tags and acceptance evidence.

This policy complements `docs/internal/git-flow-release.md` and does not change the release topology defined there.

## Retained branches

The normal steady-state repository keeps only:

- `main` — accepted release history;
- `develop` — current integration history;
- the head branch of each genuinely open Pull Request.

During a release or hotfix lifecycle, the corresponding `release/v*`, `backmerge/v*` or `hotfix/*` branch exists only while that workflow is active.

For the current V1 convergence phase, the intended visible branch set is therefore:

```text
main
develop
feature/104-v1-ga-acceptance-matrix
```

until another real PR/release branch is opened.

## Repository setting

GitHub's repository setting **Automatically delete head branches** (`delete_branch_on_merge`) should be enabled. The V1 repository audit found it disabled, which is one direct reason merged task refs can accumulate even when the development flow itself is correct.

This setting handles merged Pull Requests automatically. Superseded or abandoned PR heads still follow the explicit deletion rule below.

## Deletion rule

A short-lived branch should be deleted when its Pull Request is merged, superseded or closed as abandoned, provided it is not the head of another open PR.

Deleting the branch ref does **not** rewrite the associated Issue/PR discussion or accepted repository history. Do not preserve stale refs merely as a substitute for audit records.

Never delete or force-move a branch only because its commit IDs differ from a later squash/reimplementation line. Before pruning an old ref, use repository/PR facts to establish that it is no longer an execution line. A branch with new activity after a cleanup manifest was produced must fail closed and be reviewed again.

## Current V1 convergence cleanup

The repository contains historical task branches created before the single V1 convergence line was established. Only PR #106 is currently open. Those historical task refs are cleanup candidates rather than parallel V1 development lines.

`.github/scripts/prune-stale-branches.ps1` carries the one-time V1 cleanup manifest. It deliberately:

1. protects `main`, `develop` and `feature/104-v1-ga-acceptance-matrix`;
2. SHA-locks every historical cleanup candidate to the exact ref observed during the repository audit;
3. re-queries open Pull Request heads before deletion;
4. aborts **before deleting anything** if a candidate moved, disappeared or became an open PR;
5. defaults to dry-run and requires explicit `-Execute` for deletion.

Run the preflight first:

```powershell
pwsh .github/scripts/prune-stale-branches.ps1
```

Then, only after the preflight reports PASS:

```powershell
pwsh .github/scripts/prune-stale-branches.ps1 -Execute
```

The cleanup is intentionally one operation, not a manual branch-by-branch checklist.

## New branch discipline

Use only the branch families defined by `docs/internal/git-flow-release.md`:

```text
feature/<issue>-<topic>
release/vX.Y.Z.W
backmerge/vX.Y.Z.W
hotfix/<issue>-<topic>
```

Temporary spike branches may be used for bounded experiments, but once the conclusion is recorded as an evaluation record under `docs/internal/` and the related PR/Issue is closed, the branch ref is deleted under the same lifecycle rule.

Do not create long-lived branches named after architectural layers (`core/*`, `qpa/*`, `qml/*`, etc.) as permanent parallel product lines. Product architecture is represented by repository modules; work sequencing is represented by Issues/PRs.
