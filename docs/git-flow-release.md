# HyRemote Git Flow, Testing and Release Policy

This document is the canonical repository workflow for developing, testing and releasing HyRemote product milestones. It complements [`versioning.md`](versioning.md); it does not redefine the frozen four-part product version semantics.

Governance mode remains `transitional-explicit` until the repository's reusable ADS machine acceptance tracked by #14 is complete.

## 1. Product version versus Git tag

HyRemote product versions use the frozen four-part form:

```text
Major.Minor.Feature.Maintenance
```

Product documentation may write the version with an uppercase `V`, for example `V0.0.3.0` or `V1.0.0.0`.

Repository release tags use the same four numeric fields with a lowercase `v` prefix:

```text
v0.0.1.0
v0.0.2.0
v0.0.3.0
v1.0.0.0
```

A tag is a **release fact**, not a progress marker. Never create a milestone tag merely because implementation exists. The corresponding product milestone must satisfy its complete acceptance gate first.

Release tags are **annotated tags**. A milestone tag must peel to the exact current `main` release HEAD produced by the accepted release-branch merge; tagging an older ancestor of `main`, using a lightweight tag, or moving/reusing a prior release tag is not an accepted release operation.

## 2. Long-lived branches

### `main`

`main` contains accepted release history only.

Rules:

- no direct feature development;
- no speculative compatibility claim;
- no implementation-only milestone merge;
- every product milestone tag points to the exact accepted `main` release HEAD for that milestone;
- a release branch may merge into `main` only after its full acceptance gate is green.

### `develop`

`develop` is the integration branch for the next product release line.

Rules:

- new feature branches start from `develop`;
- accepted feature PRs merge back into `develop`;
- `develop` may contain completed features that are not yet a released product milestone;
- a milestone release branch is cut from `develop` only when the milestone's feature scope is complete and all predecessor product dependencies are integrated;
- the root CMake project version on `develop` remains the development sentinel `0.0.0`; an externally meaningful milestone version exists only on an authorized `release/v*` candidate and the corresponding released `main` history.

`develop` is not a substitute for `main`: its contents are not automatically a supported release claim.

## 3. Short-lived branches

### `feature/<issue>-<topic>`

Use for new product/engineering work.

Example:

```text
feature/94-qpa-deploy
```

Rules:

- create from `develop`;
- link a GitHub Issue/WBS item;
- include implementation and its bounded tests/evidence in the same branch when practical;
- PR target is `develop`;
- retain the `0.0.0` development version sentinel;
- remain Draft while mandatory evidence is unavailable;
- do not merge merely to make `develop` appear complete.

Historical atomic/stacked branches created before this policy are not force-renamed or history-rewritten. Their accepted contents converge into `develop` in dependency order.

### `release/vX.Y.Z.W`

Use only for a feature-complete product milestone candidate.

Examples:

```text
release/v0.0.1.0
release/v0.0.2.0
release/v0.0.3.0
release/v1.0.0.0
```

Rules:

- create from `develop` only after the milestone scope is integrated;
- change the root CMake `project(VERSION ...)` from the development sentinel to the exact four-part milestone version;
- only release-blocking defects, acceptance fixes, release metadata, license/security/compatibility corrections and release documentation may enter the branch;
- no new product capability is added on a release branch;
- full milestone acceptance runs from this branch, including the same normal CTest suite used during development;
- final PR target is `main`;
- after release/tag creation, reconcile released history back to `develop` through the bounded `backmerge/vX.Y.Z.W` path below.

### `backmerge/vX.Y.Z.W`

Use only after the matching release has been accepted, merged to `main`, and published as an annotated `vX.Y.Z.W` tag.

Rules:

- start from the released `main` history containing the matching annotated tag;
- contain the already-released commit; it must not recreate or rewrite release history;
- reset root CMake `project(VERSION ...)` to the `0.0.0` development sentinel before targeting `develop`;
- PR target is `develop` only;
- carry release-only fixes/metadata forward without adding new product capability;
- the machine gate verifies the corresponding annotated tag exists in `main` history and is contained by the backmerge branch.

This explicit backmerge path keeps `develop` ready for the next milestone without leaving unreleased feature builds stamped with the previous release version.

### `hotfix/<issue>-<topic>`

Use for an urgent correction to an already released `main` version.

Rules:

- create from `main`;
- scope must fit the frozen Maintenance-field definition unless a new product release is explicitly authorized;
- validate against the released compatibility matrix;
- merge the accepted fix into `main`;
- create the corresponding maintenance release/tag only after acceptance;
- reconcile the same correction back into `develop` under the same development-version discipline.

A hotfix must not be used to smuggle in a new platform or integration-mode capability.

## 4. Development gate

Before a feature PR can leave Draft state, its scope must have the evidence required by the linked issue. Depending on the feature this includes:

- deterministic unit/contract tests;
- build/configure/generate checks;
- standard-viewer product-fit tests;
- clean installed-SDK/source consumers;
- exact Qt/OS compatibility tests;
- documentation and known-limitations updates;
- architecture/API review where public or private boundaries change.

A failed job that never received a runner or executed repository steps is infrastructure evidence only. It is not a feature test failure and it is not passing evidence.

Local Developer Agent or physical hardware evidence is used only when the acceptance requirement genuinely needs a capability unavailable to hosted/repository execution, such as physical local-display/local-input coexistence. It must not be used to disguise broken hosted CI/account infrastructure.

## 5. Integration gate (`feature/*` -> `develop`)

Merge a feature PR to `develop` only when all of the following are true:

1. the implementation stays inside frozen product/architecture boundaries;
2. required feature-level tests actually executed and passed for the claimed scope;
3. unsupported/unverified combinations remain explicitly classified;
4. required docs/examples are updated;
5. no unresolved correctness blocker is hidden by a performance or future-hardware promise;
6. stacked predecessor PRs are already integrated or the merge order makes the dependency graph valid;
7. the PR is no longer Draft and review/acceptance requirements are satisfied;
8. root CMake still carries the `0.0.0` development version sentinel.

Do not merge a Draft PR solely because its code is useful to another branch. Stacked/convergence branches may preserve predecessor history until those predecessors are accepted.

## 6. Release-candidate gate (`develop` -> `release/vX.Y.Z.W`)

A release branch may be created only when the corresponding product milestone is feature-complete on `develop`.

For the current pre-GA line:

| Milestone issue | Product version | Release branch | Final tag |
| --- | --- | --- | --- |
| #30 | `V0.0.1.0` | `release/v0.0.1.0` | `v0.0.1.0` |
| #31 | `V0.0.2.0` | `release/v0.0.2.0` | `v0.0.2.0` |
| #32 | `V0.0.3.0` | `release/v0.0.3.0` | `v0.0.3.0` |
| #33 | `V1.0.0.0` | `release/v1.0.0.0` | `v1.0.0.0` |

The V1.0 GA release branch may not be cut until #30, #31, #32, #39 and #41 are accepted and integrated on `develop`.

## 7. Release-branch acceptance

The release branch owns final product acceptance, not new feature development.

For x86 V1 milestones, the required matrix must include the exact claimed Windows x86_64 and Linux x86_64 environments. Where the milestone claims both operating systems, evidence from one does not substitute for the other.

For `V1.0.0.0`, at minimum verify the #33 gate:

- Embedded C++ API;
- Declarative QML API;
- Transparent QPA Proxy;
- Widgets and Quick product paths;
- local + remote coexistence for the claimed modes/environments;
- remote view/input, disconnect/reconnect and bounded backpressure;
- loopback/input safe defaults and current security limitations;
- clean installed SDK/source consumers;
- deployment helper paths;
- examples and getting-started documentation;
- license/third-party notices;
- compatibility and known limitations;
- exact Qt/OS support statements;
- release notes.

The repository metadata CTest validates metadata/package consistency on both development and release-candidate versions. Git branch/version authorization is owned separately by the Git Flow workflow so changing `project(VERSION ...)` to the authorized release version does not disable the normal release CTest suite.

A release candidate remains unreleased while any mandatory acceptance item is unexecuted, failed or blocked.

## 8. Merge to `main`, tag, and backmerge

After the release branch passes its complete milestone gate:

1. freeze the accepted release-branch head;
2. merge the release branch into `main` without rewriting away the accepted history;
3. verify the resulting `main` HEAD is the intended release commit;
4. create an **annotated** milestone tag on that exact `main` HEAD;
5. verify the tag peels to the same `main` release commit and the root CMake version matches the tag;
6. publish release notes/artifacts from the same tag where repository tooling supports them;
7. create `backmerge/vX.Y.Z.W` from released `main`, reset only the root development version to `0.0.0`, and PR it to `develop`;
8. verify the backmerge contains the released tagged commit and preserves any already-approved later `develop` history through normal PR merge/reconciliation;
9. only then close the milestone issue as released.

Do not tag a release-branch commit that was never merged to `main`, do not use a lightweight milestone tag, do not tag an older `main` ancestor, and do not move/reuse an existing release tag to point at another commit.

## 9. Maintenance releases

Maintenance increments follow the same release discipline. Example after `V1.0.0.0`:

```text
release/v1.0.0.1
v1.0.0.1
```

The Maintenance field may carry fixes, security corrections, packaging/docs corrections and small-scope optimizations that do not add a new product capability.

## 10. Current migration rule

This policy was introduced after substantial V1 work already existed as atomic/stacked PRs based on the historical `main` workflow.

Migration is deliberately non-destructive:

- do not force-push or rename existing historical feature branches merely for cosmetics;
- keep their Issue/PR/canonical evidence records intact;
- create new work from `develop` using `feature/*`;
- converge accepted historical work into `develop` in dependency order;
- keep `develop` at the `0.0.0` development version sentinel;
- do not create retroactive milestone tags for versions that have not actually passed their current Windows/Linux acceptance criteria.

This preserves auditability while making Git Flow authoritative from this point forward.
