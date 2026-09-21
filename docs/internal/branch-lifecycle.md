# Branch Lifecycle and Retention

HyRemote treats Git branches as **temporary work cursors**, not permanent archive objects. Durable history lives in Issues, Pull Requests, commits, release tags and acceptance evidence.

This policy complements `docs/internal/git-flow-release.md` and does not change the release topology defined there.

## Retained branches

The normal steady-state repository keeps only:

- `main` — accepted release history;
- `develop` — current integration history;
- the head branch of each genuinely open Pull Request.

During a release or hotfix lifecycle, the corresponding `release/v*`, `backmerge/v*` or `hotfix/*` branch exists only while that workflow is active.

The exact set of short-lived refs is therefore discovered from current open PR facts; this document does not hard-code a historical PR branch as a permanent exception.

## Repository setting and scheduled enforcement

GitHub's repository setting **Automatically delete head branches** (`delete_branch_on_merge`) is enabled. It handles merged pull requests, but a PR closed without merging can still leave its head ref behind.

`.github/workflows/branch-hygiene.yml` applies the same deletion rule weekly and on demand. Manual dispatch is dry-run by default. The workflow:

- never deletes `main`, `develop`, `release/*`, `backmerge/*` or `hotfix/*`;
- keeps any branch that is the head of an open PR;
- fails closed when it cannot prove a branch belongs to a merged/closed PR;
- deletes only when `execute=true` is explicitly requested for manual runs or when the workflow's configured execution policy permits it.

The scheduled workflow complements, rather than replaces, the repository setting.

## Deletion rule

A short-lived branch should be deleted when its Pull Request is merged, superseded or closed as abandoned, provided it is not the head of another open PR.

Deleting the branch ref does **not** rewrite the associated Issue/PR discussion or accepted repository history. Do not preserve stale refs merely as a substitute for audit records.

Never delete or force-move a branch only because its commit IDs differ from a later squash/reimplementation line. Before pruning an old ref, use repository/PR facts to establish that it is no longer an execution line.

## Historical V1 cleanup

The one-time 2026-09-20 cleanup removed stale task refs whose PRs were already merged/closed. `.github/scripts/prune-stale-branches.ps1` records that bounded cleanup history and SHA-locked safety model. It is not the normal recurring branch lifecycle mechanism; `delete_branch_on_merge` plus `branch-hygiene.yml` now owns recurrence prevention.

If the historical script is run for audit purposes, use dry-run first and do not alter its safety checks merely to make a stale manifest execute again.

## New branch discipline

Use only the branch families defined by `docs/internal/git-flow-release.md`:

```text
feature/<issue>-<topic>
release/vX.Y.Z.W
backmerge/vX.Y.Z.W
hotfix/<issue>-<topic>
```

Temporary experiment branches may be used for bounded work, but once the conclusion is recorded and the related PR/Issue is closed, the branch ref follows the same deletion rule.

Do not create long-lived branches named after architectural layers (`core/*`, `runtime/*`, `qpa/*`, `qml/*`, etc.) as permanent parallel product lines. Product architecture is represented by repository modules; work sequencing is represented by Issues/PRs.
