# HyRemote Git Flow, Testing and Release Policy

This is the canonical repository workflow for developing, testing and releasing HyRemote milestones. It complements `docs/versioning.md`; it does not redefine the frozen four-part version semantics.

Governance mode remains `transitional-explicit` until #14 passes reusable ADS machine acceptance.

## 1. Version and tag rule

HyRemote product versions use:

```text
Major.Minor.Feature.Maintenance
```

Current authorized release trains are progressive, and every one of them is a real public release with a narrower
support contract than GA (#24, #95):

```text
v0.1.0.0   Use It / Developer Preview        (loopback-only, primary C++/Generic paths)
v0.2.0.0   Trust It / Operational Preview    (security, session, network)
v0.3.0.0   Productize It / Product Preview   (four product-deliverable integrations, deployment, examples)
v0.4.0.0   Qualify It / Release Candidate Line, with v0.4.0.x maintenance releases
v1.0.0.0   Stabilize It / first GA, promoted from one mature V0.4 lineage
```

The retired planning labels `v0.0.1.0`, `v0.0.2.0` and `v0.0.3.0` are not release trains and must not be revived; they
appear in this document only to say so.

A tag is a **release fact**, never a progress marker. Implementation existing on `develop` is not enough to create a tag.

Every taggable release owns matching candidate release notes under `docs/releases/<version>.md`.

During acceptance the notes keep:

```text
Status: **candidate / acceptance pending**
```

Remove that marker only after the complete milestone gate passes and before the release PR leaves Draft.

## 2. Long-lived branches

### `main`

`main` contains accepted release history only.

- no direct feature development;
- no speculative support claim;
- release branches merge only after complete acceptance;
- every milestone tag is annotated and points to the exact accepted current `main` release HEAD.

### `develop`

`develop` is the integration branch.

- feature branches start from `develop`;
- accepted feature PRs merge to `develop`;
- root project version remains development sentinel `0.0.0`;
- `develop` may contain source for later V1 modes before those modes are released;
- contents of `develop` are not automatically product/support claims.

## 3. Short-lived branches

### `feature/<issue>-<topic>`

- create from `develop`;
- link an Issue/WBS item;
- target `develop`;
- stay Draft while mandatory evidence is unavailable;
- include bounded implementation/tests/docs in the same line when practical;
- delete the branch ref after its PR is merged, superseded or closed, provided it is not the head of another open PR.

Historical Issue/PR/commit evidence is durable audit history; a stale branch ref is not. Do not force-push historical refs merely to make ancestry look linear, but do prune refs whose work line is no longer active. See `docs/internal/branch-lifecycle.md`.

### `release/vX.Y.Z.W`

Create only for a feature-complete milestone candidate.

- create from `develop`;
- change root `project(VERSION ...)` to the exact four-part milestone version;
- target `main` as Draft;
- no new product capability is added on the release branch;
- only release blockers, acceptance corrections, security/license/compatibility/release metadata and docs may change;
- run the milestone-specific acceptance matrix;
- remove the candidate release-note marker only after every mandatory gate passes;
- merge to `main`, then create the annotated tag on the exact accepted `main` HEAD;
- reconcile released history through `backmerge/vX.Y.Z.W` and restore `develop` to `0.0.0`.

### `backmerge/vX.Y.Z.W`

- create from the released history after the annotated tag exists;
- target `develop`;
- must contain the tagged release commit;
- restore the development version sentinel `0.0.0` before merging.

### `hotfix/<issue>-<topic>`

Use only for corrections to an already released `main` version. A hotfix may not smuggle in a new integration mode or platform capability.

## 4. Product-only development default

A normal source configure builds the standard C++ product, not repository development infrastructure.

Default product shape:

- Core: ON, internal static composition;
- `HyRemote::RemoteAccess`: ON, shared product facade;
- Widgets/Quick adapters: ON when the matching Qt modules exist;
- bounded C++ RFB correctness transport: ON;
- tests/examples: OFF (there is no experimental tree to switch on);
- QML API: opt-in;
- Transparent QPA: opt-in and exact-private-ABI qualified.

Maintainers/CI explicitly enable tests/examples. `add_subdirectory()` consumers must not need to know internal Core/adapter/RFB switches to obtain the normal C++ product.

## 5. Version-derived milestone product profiles

`develop` intentionally integrates the full future V1 source while its version is `0.0.0`. Formal milestone releases must not expose later unaccepted modes merely because their source is already present.

The four-part project version is therefore also the release-profile authority; **there is no second public release-profile option**.

The four-part version no longer selects which product modes a release may contain:

| Release version | Meaning for frontends |
| --- | --- |
| any current train | which frontends are enabled, preview, qualified or supported is a release-train scope and compatibility decision - never a version digit |
| `V0.0.x` (retired) | rejected as a planning label; the sequential `V0.0.1.0 = C++`, `V0.0.2.0 = QML`, `V0.0.3.0 = QPA` map is not revived |

The root CMake graph enforces exactly this much and no more: a formal version in the retired `V0.0.x` family is refused,
and every current train - `V0.1`, `V0.2`, `V0.3`, `V0.4` (including `V0.4.0.x` maintenance) and `V1.0` - is accepted
whatever frontends the build enables.

CI follows the capability model instead of the version model: the classifier selects the affected frontends for a
change, and release readiness consumes the current train's own scope (#95) rather than one static V1 list.

This prevents a correct early release profile from being falsely failed by a future-mode workflow, while also preventing an early tag from becoming an accidental support claim for later modes.

## 6. Development/integration gate

Before a feature PR can leave Draft and merge to `develop`:

1. implementation remains inside frozen product/architecture boundaries;
2. required tests actually executed and passed for the claimed scope;
3. unverified/unsupported combinations remain explicit;
4. required examples/docs are current;
5. no correctness blocker is hidden behind future performance/hardware work;
6. dependencies/merge order are valid;
7. review/acceptance requirements are satisfied.

A GitHub Actions job with no runner and no executed steps is infrastructure evidence only. It is neither pass nor code failure.

Local Developer Agent/physical hosts are used only for genuinely local-only evidence such as physical native local-display/local-input coexistence. They are not substitutes for broken hosted CI.

## 7. Current milestone release map

| Train | Release branch | Release notes | Final tag |
| --- | --- | --- | --- |
| `V0.1.0.0` | `release/v0.1.0.0` | `docs/releases/v0.1.0.0.md` | `v0.1.0.0` |
| `V0.2.0.0` | `release/v0.2.0.0` | `docs/releases/v0.2.0.0.md` | `v0.2.0.0` |
| `V0.3.0.0` | `release/v0.3.0.0` | `docs/releases/v0.3.0.0.md` | `v0.3.0.0` |
| `V0.4.0.0` (+ `V0.4.0.x`) | `release/v0.4.0.0` | `docs/releases/v0.4.0.0.md` | `v0.4.0.0` |
| `V1.0.0.0` | `release/v1.0.0.0` | `docs/releases/v1.0.0.0.md` | `v1.0.0.0` |

The retired `#30` / `#31` / `#32` single-frontend milestone authorities are history and are not release trains. Each
current train is authorized by its own readiness scope under #1, #24 and #95; `release/v1.0.0.0` may not be cut until
one mature V0.4 lineage has been accepted **and** all of the following exact-candidate closure gates are accepted:

- #101 — V1 public C++/QML/CMake/API/package freeze;
- #104 — integrated all-three-mode GA automation has actually executed and passed on both Windows and Linux reference environments;
- #107 — release notes, notices, package manifest and deterministic release-readiness metadata are complete and aligned with the frozen V1 artifact model;
- #109 — required physical native local-display/local-input + remote coexistence evidence has passed for the applicable E1/E2/E3/E4 envelope.

A #74 job that never receives a runner is not #104 acceptance. Local/physical evidence is not a substitute for #104 build/install/deployment automation, and hosted/Xvfb evidence is not a substitute for #109.

Creating a release branch is not acceptance and never authorizes its tag.

## 8. Release acceptance and finalization

For every claimed Windows/Linux x86 milestone, evidence must exist independently for both operating systems where the milestone requires both.

For V1.0.0.0, at minimum verify:

- all three integration modes;
- Widgets + Quick paths;
- remote view/input and reconnect;
- bounded backpressure/lifecycle/protocol/input behavior;
- safe loopback/input defaults and the current security boundary (SecurityType None unless an authenticated profile provides RFB VNC authentication; no encryption);
- local + remote coexistence where claimed;
- clean installed SDK and source consumer;
- one deployment-helper contract;
- examples/user documentation;
- licenses/notices;
- compatibility/known limitations;
- exact Qt/OS statements;
- release notes;
- #101, #104, #107 and #109 against the same exact release candidate.

The frozen V1 package shape itself is part of acceptance: `hyremote-core` remains static source/internal composition and is not installed/exported as a normal SDK target; `HyRemote::RemoteAccess` is the one shared C++ product target; QML is a thin payload over that runtime; and `qhyremote` is a package-owned QPA platform MODULE rather than an application link target.

A candidate remains unreleased while any mandatory item is blocked, failed or unexecuted.

After all gates pass:

1. freeze release content except finalization metadata;
2. remove the candidate marker from matching release notes;
3. verify root version equals release branch version;
4. rerun affected metadata/policy checks;
5. mark the release PR ready;
6. merge to `main`;
7. verify exact intended release HEAD;
8. create the annotated milestone tag on that exact `main` HEAD;
9. run post-tag audit;
10. backmerge/reconcile to `develop` and restore `0.0.0`.

Do not tag a release-branch commit that was never merged to `main`; do not create lightweight milestone tags; do not move/reuse release tags.

## 9. Maintenance releases

After V1 GA, a maintenance release such as:

```text
release/v1.0.0.1
v1.0.0.1
```

follows the same discipline. Maintenance may carry fixes/security/packaging/docs/small-scope optimizations, but not a new product capability. Each new maintenance release must add its own release notes and explicit policy authorization.

## 10. Current V1 convergence and branch retention

This policy was introduced after substantial V1 work already existed in historical stacked/task branches.

- preserve Issue/PR/commit/evidence history; do not rewrite it for appearance;
- branch refs are temporary work cursors, not the archive mechanism;
- current repository-accessible V1 work converges through the single #106 feature line, then `develop`;
- closed/superseded historical task branches must not receive new V1 development and should be pruned once they are no longer open-PR heads;
- the intended steady-state visible set is `main`, `develop`, plus currently open PR heads;
- the one-time V1 cleanup is SHA-locked and fail-closed in `.github/scripts/prune-stale-branches.ps1`;
- do not create retroactive tags for milestones that have not passed current acceptance.

See `docs/internal/branch-lifecycle.md` for the retention/deletion procedure. This preserves auditability while preventing stale task refs from masquerading as parallel product lines.
