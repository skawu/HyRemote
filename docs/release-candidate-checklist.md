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
- compatibility/support documents use `Supported`, `Candidate`, `Experimental`, `Unsupported`, and `Unverified` truthfully;
- the milestone acceptance authority records the final evidence envelope and is **closed as completed**; `not_planned` is not release acceptance;
- root `project(VERSION ...)` on `develop` is still the development sentinel `0.0.0`.

Infrastructure failure such as #74 is not a product pass. If required jobs never execute, the milestone remains unaccepted and its authority issue remains open.

For **V1.0.0.0**, repository implementation on Draft PR #106 is the single convergence line, but that branch is not itself release authorization. Before `release/v1.0.0.0` may be cut, #30/#31/#32/#39/#41, #101/#104/#107/#109 and final GA authority #33 must all be accepted/closed as completed. This means #104 must contain actually executed Windows/Linux evidence and #109 must contain the distinct physical/native evidence before the release branch can exist legitimately. The canonical repository execution/evidence template for #109 is `docs/v1-physical-acceptance.md`; completing or merely possessing that file does not replace #109 review/closure, and its candidate SHA must match the accepted integration point.

The Git Flow workflow reads those issue states for `release/v* -> main`; it does not close issues or create acceptance on the user's behalf. Editing release notes or toggling PR Draft state cannot substitute for authority closure.

Repository API inspection currently reports both `main` and `develop` as **unprotected**. The current repository/plan also does not expose a usable repository ruleset for this private repository, so Git Flow checks are policy and audit rather than pre-push enforcement. The workflow therefore audits every push to `main`/`develop` and fails when the new head is not associated with a merged PR into that same branch. This is deliberately an **after-the-fact direct-push audit**: it cannot undo a push and must not be described as branch protection. Release operators must still use the documented PR topology; any direct-push audit failure is a governance incident that must be investigated and reconciled before release evidence is accepted.

## 2. Cut the release branch

Only after section 1 is complete:

1. freeze the accepted `develop` integration point;
2. create `release/vMajor.Minor.Feature.Maintenance` from that point;
3. update root `project(HyRemote VERSION ...)` to exactly the release-branch version;
4. prepare/finalize release notes, package metadata, compatibility matrix and third-party notices for that exact candidate;
5. open the release PR from `release/v*` to `main` as Draft while release-candidate verification is repeated on the exact versioned branch.

The release branch is therefore a versioned verification/finalization line, not a place to complete missing product acceptance. No new product feature enters it. Only release-blocking fixes, packaging corrections, documentation corrections, and evidence corrections are allowed; any material product change must be reaccepted by the applicable authority before merge.

`develop` may continue with later work after the release branch is cut; the release candidate is not required to absorb unrelated later `develop` commits.

## 3. Release-candidate verification

The release PR must pass all gates applicable to the milestone on the exact versioned candidate even though the milestone authority was already accepted before the branch was cut.

Common gates:

- clean configure/build on exact claimed Qt/toolchain combinations;
- the same full CTest suite used by development, including release metadata/package checks;
- unit/integration/product E2E tests;
- installed SDK clean-consumer configure/build/run;
- source-consumption validation;
- runtime deployment validation;
- all applicable `hyremote_deploy()` call shapes are executed rather than inferred from independent payload tests; for V1 this includes C++, QML, QPA and combined `QML QPA`;
- clean source/add_subdirectory deployment resolves from its deployed tree and does not rely on the original build tree or Qt SDK remaining reachable through an embedded build RUNPATH;
- clean installed QML+QPA deployment loads the deployed `HyRemote` module, starts transparent QPA over the same shared runtime, accepts/reaccepts an RFB viewer, and does so without SDK/plugin/QML/runtime environment overrides;
- standard VNC viewer connect/view/input/disconnect/reconnect as required;
- abrupt viewer disconnect while supported remote input is held does not leave stale key/button state and the next viewer begins clean;
- simultaneous viewer disconnect/release does not clear another viewer's still-held logical key/button state;
- explicit HyRemote runtime stop/policy transition while supported remote input is held returns the still-running target to neutral state, drops undelivered pending remote input, and does not synthesize duplicate releases on repeated teardown;
- safe-default checks: loopback listener and remote input disabled unless explicitly enabled;
- no Core/backend/private-QPA types leak into stable installed application APIs;
- package contents and licenses/notices are complete;
- docs and examples correspond to the release candidate, not another branch;
- support/compatibility rows remain Candidate/Unverified until their exact required evidence has actually executed and passed.

The explicit-stop cleanup requirement is an internal target-adapter/runtime guarantee. Multi-viewer held-state aggregation is likewise a private transport correctness rule. Neither may be satisfied by adding a new public application reset/per-client API, second runtime, or test-only bypass.

Branch/version authorization belongs to the Git Flow workflow. Repository metadata CTest must remain valid after an authorized release branch changes the project version from `0.0.0` to its exact milestone version.

Milestone-specific authorities:

- `v0.0.1.0`: #30
- `v0.0.2.0`: #31, after #30
- `v0.0.3.0`: #32, after #30/#31 and including required physical native local + remote display/input coexistence
- `v1.0.0.0`: #33, after accepted #30 + #31 + #32 + #39 + #41

V1.0.0.0 additionally requires these explicit engineering/release-closure gates on the exact candidate:

- **#101 — public API/package freeze:** one normal installed C++ target (`HyRemote::RemoteAccess`), no installed/exported Core or QPA application link target, stable C++/QML/deploy surface;
- **#104 — integrated GA automation:** all three modes coexist in one exact-Qt-6.8.3 candidate and the Windows/Linux jobs actually execute and pass; a no-runner result does not satisfy it;
- **#107 — release readiness:** release notes, NOTICE/dependency classification, package manifest, LICENSE/NOTICE installation and deterministic metadata checks are complete and aligned with the frozen artifact model;
- **#109 — physical/native coexistence:** the required Windows/Linux E1/E2/E3/E4 local-visible/local-input + remote envelope passes where specified by #32/#33, including abrupt-disconnect held-state cleanup and explicit-stop/policy-transition cleanup with no late queued remote input. Execute and retain that exact-candidate record through `docs/v1-physical-acceptance.md`; the runbook is preparation/evidence structure, while #109 remains the acceptance authority.

These are not optional documentation references. #33 closes only after these gates and predecessor authorities have been accepted; the Git Flow release-authority check then prevents a `release/v1.0.0.0` PR from bypassing them.

For the current candidate, hosted/Xvfb execution may prove protocol/runtime/Qt-target correctness but cannot substitute for #109 physical evidence. Conversely, local physical evidence cannot substitute for #104 Windows/Linux build/install/deployment automation.

## 4. Merge to `main` and tag

Only after the exact versioned release candidate has repeated its applicable verification successfully:

1. finalize the applicable release note so it no longer says acceptance is pending;
2. move the release PR out of Draft only after the versioned-candidate checks are complete;
3. merge `release/vX.Y.Z.W` into `main`;
4. verify the resulting current `main` HEAD still contains the accepted candidate and exact four-part CMake version;
5. create an **annotated** tag `vX.Y.Z.W` on that exact current `main` HEAD;
6. allow the tag audit workflow to verify the authorized milestone, annotated-tag object, exact `main` HEAD equality, and CMake-version equality;
7. publish release artifacts/notes from that tag only.

A tag is never created first and validated later as the normal workflow. The tag workflow is a second-line audit; the release PR is the blocking gate. Do not use a lightweight release tag, tag an older `main` ancestor, or move an existing milestone tag.

## 5. Audited backmerge to `develop`

After the release/tag fact exists:

1. create `backmerge/vX.Y.Z.W` from the released `main` history;
2. preserve the released tagged commit in that branch;
3. reset only root `project(VERSION ...)` to the `0.0.0` development sentinel;
4. open `backmerge/vX.Y.Z.W -> develop`;
5. let the Git Flow workflow verify that the matching annotated tag exists in current `main` history and that the backmerge branch contains its released commit;
6. reconcile any already-approved later `develop` history through the normal PR merge;
7. record the release tag and backmerge commit in the milestone issue/roadmap.

Only then treat the Git Flow release cycle as closed. The backmerge must not add new product capability or rewrite the released commit.

## 6. Hotfixes

A production hotfix starts from `main` on `hotfix/*`, is validated against the affected released support matrix, merges to `main`, receives an explicitly authorized four-part maintenance tag, and is reconciled back into `develop` while restoring the `0.0.0` development sentinel.

Maintenance tags are not implicitly authorized by the V1 milestone list. Add the exact maintenance version to release policy deliberately before cutting/publishing it.

## Non-negotiable V1 product boundaries

Release mechanics must never change the frozen product architecture merely to make a gate easier to pass:

- `hyremote-core` remains transport-neutral, Qt-private-free, STATIC source/internal composition and is not installed/exported as a normal V1 SDK target;
- Embedded C++ public API remains the one shared `HyRemote::RemoteAccess` product facade;
- Declarative QML remains a thin surface over the same shared runtime rather than a second runtime stack;
- Transparent QPA remains a package-owned platform MODULE/native-delegate-preserving proxy/decorator, not a replacement-only qvnc clone and not an application link target;
- Qt Widgets and Qt Quick remain first-class peers;
- one `hyremote_deploy()` entry point owns all four C++/QML/QPA/combined-QML+QPA payload compositions;
- target-input terminal cleanup remains internal: Session/transport callbacks quiesce before sink shutdown, pending undelivered input is discarded, and delivered supported held state is balanced without exposing a new application reset API;
- simultaneous-viewer held-state isolation remains private transport bookkeeping and must not grow a public client-ownership/control channel;
- the current V1 RFB correctness baseline remains explicitly unauthenticated/unencrypted (`SecurityType None`) unless a separately reviewed release changes that fact;
- no support claim is made without reproducible evidence for the exact environment;
- no Local Agent result is used to paper over #74 no-runner infrastructure, and no hosted/headless result is relabeled as #109 physical evidence.
