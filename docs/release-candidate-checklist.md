# HyRemote milestone release-candidate checklist

This checklist is the canonical release procedure for the currently authorized milestones:

- `v0.0.1.0` — Embedded C++ API
- `v0.0.2.0` — Declarative QML API
- `v0.0.3.0` — Transparent QPA Proxy
- `v1.0.0.0` — x86 Windows + Linux GA

A milestone is not a release merely because its implementation branch exists or its code builds locally. The milestone acceptance issue is the release authority.

## 1. Feature completion on `develop`

Before a release branch is cut:

- all milestone-required feature PRs have completed review and have executable evidence for their claimed environments;
- required documentation/examples are present;
- required automated tests are green on the exact claimed Windows/Linux + Qt matrix;
- any milestone-required physical/manual acceptance has recorded reproducible evidence;
- no unresolved blocker is reclassified as post-release merely to make the milestone appear complete;
- compatibility/support documents use `supported`, `experimental`, `unsupported`, and `unverified` truthfully;
- the milestone acceptance issue records the final evidence envelope and is ready to close.

Infrastructure failure such as #74 is not a product pass. If required jobs never execute, the milestone remains unaccepted.

## 2. Cut the release branch

Only after section 1 is complete:

1. freeze the accepted `develop` integration point;
2. create `release/vMajor.Minor.Feature.Maintenance` from that point;
3. update root `project(HyRemote VERSION ...)` to exactly the release-branch version;
4. prepare release notes, package metadata, compatibility matrix and third-party notices for that exact candidate;
5. open the release PR from `release/v*` to `main`.

No new product feature enters the release branch. Only release-blocking fixes, packaging corrections, documentation corrections, and evidence fixes are allowed.

`develop` may continue with later work after the release branch is cut; the release candidate is not required to absorb unrelated later `develop` commits.

## 3. Release-candidate verification

The release PR must pass all gates applicable to the milestone.

Common gates:

- clean configure/build on exact claimed Qt/toolchain combinations;
- unit/integration/product E2E tests;
- installed SDK clean-consumer configure/build/run;
- source-consumption validation;
- runtime deployment validation;
- standard VNC viewer connect/view/input/disconnect/reconnect as required;
- safe-default checks: loopback listener and remote input disabled unless explicitly enabled;
- no backend/private-QPA types leak into stable public APIs;
- package contents and licenses/notices are complete;
- docs and examples correspond to the release candidate, not another branch.

Milestone-specific gates remain authoritative in their issue:

- `v0.0.1.0`: #30
- `v0.0.2.0`: #31, after #30
- `v0.0.3.0`: #32, including physical local + remote display/input coexistence on both supported OSes
- `v1.0.0.0`: #33, requiring #30 + #31 + #32 + #39 + #41

## 4. Merge to `main` and tag

Only after the release candidate is accepted:

1. merge `release/vX.Y.Z.W` into `main`;
2. verify the resulting current `main` HEAD still contains the accepted candidate and exact four-part CMake version;
3. create an **annotated** tag `vX.Y.Z.W` on that exact current `main` HEAD;
4. allow the tag audit workflow to verify the authorized milestone, annotated-tag object, exact `main` HEAD equality, and CMake-version equality;
5. publish release artifacts/notes from that tag only.

A tag is never created first and validated later as the normal workflow. The tag workflow is a second-line audit; the release PR is the blocking gate. Do not use a lightweight release tag, tag an older `main` ancestor, or move an existing milestone tag.

## 5. Back-merge to `develop`

After the release/tag fact exists:

- merge the release result back into `develop` so release-only version/docs/packaging fixes are not lost;
- resolve conflicts in favor of the accepted release facts plus already-approved later development;
- record the release tag and back-merge commit in the milestone issue/roadmap;
- only then treat the Git Flow release cycle as closed.

## 6. Hotfixes

A production hotfix starts from `main` on `hotfix/*`, is validated against the affected released support matrix, merges to `main`, receives an explicitly authorized four-part maintenance tag, and is also merged to `develop`.

Maintenance tags are not implicitly authorized by the V1 milestone list. Add the exact maintenance version to release policy deliberately before cutting/publishing it.

## Non-negotiable product boundaries

Release mechanics must never change the frozen product architecture merely to make a gate easier to pass:

- HyRemote Core remains transport-neutral and Qt-private-free;
- Embedded C++ public API remains `HyRemote::RemoteAccess`;
- Declarative QML remains a thin surface over the same runtime;
- Transparent QPA remains a native-delegate-preserving proxy/decorator, not a replacement-only qvnc clone;
- Qt Widgets and Qt Quick remain first-class peers;
- no support claim is made without reproducible evidence for the exact environment.
