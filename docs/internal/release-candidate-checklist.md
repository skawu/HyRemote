# HyRemote milestone release-candidate checklist

This is the canonical release procedure for the authorized milestones:

- `v0.0.1.0` — Embedded C++ API
- `v0.0.2.0` — Declarative QML API
- `v0.0.3.0` — Transparent QPA Proxy
- `v1.0.0.0` — x86 Windows + Linux GA

A milestone is not a release because code exists or a branch builds. Its acceptance authorities must be **closed as completed**; `not_planned` is not release acceptance.

For V1, Draft PR #106 is the sole product-convergence line, #33 is GA authority, #95 is release-process authority, #157 owns scope convergence, and #165 owns the exact-candidate freeze/change-control transition.

## 1. Complete V1 convergence on `develop`

Before any `release/v1.0.0.0` branch exists:

- all V1-required implementation/docs/test PRs are integrated into #106;
- all mandatory issues in `.github/release/v1-mandatory-issues.json` are accepted/closed as completed when their lifecycle requires closure;
- all mandatory hosted/reference checks actually execute and pass on the same integrated head;
- compatibility/security/known-limitations describe the **implemented candidate truth**, not future intent;
- root `project(VERSION ...)` on `develop` remains the development sentinel `0.0.0`;
- no blocker is relabeled post-release merely to make V1 appear complete.

The V1 authority manifest is the machine-readable source used by the Git Flow gate. #33/#95/#107/#106 prose must stay aligned with it. Scope changes update the manifest and the owning authorities together.

Infrastructure failure/no-runner is non-evidence. A required job that never runs is neither a pass nor a product failure.

## 2. Freeze the exact release candidate before final physical acceptance

V1 uses #165:

```text
CONVERGING
  -> RC-FROZEN
  -> PHYSICAL-ACCEPTANCE
  -> ACCEPTED
```

`RC-FROZEN` may be declared only when the intended V1 scope is complete and the exact #106 head has passed all mandatory hosted/reference gates.

After freeze, every proposed change is classified as:

- `evidence-neutral` — demonstrably does not change accepted payload/behavior;
- `partial re-test` — invalidates named acceptance cells;
- `full reset` — changes product/runtime/package/default/security/QPA ABI/shared acceptance examples and revokes the frozen candidate;
- `defer` — useful but unnecessary for this release.

Do not begin final #109 evidence on a moving candidate.

## 3. Execute physical/native acceptance on the frozen SHA

The canonical repository execution/evidence template for #109 is `docs/internal/v1-physical-acceptance.md`. The runbook is preparation/evidence structure, while #109 remains the acceptance authority.

- Windows/Linux E1/E2/E3/E4 physical cells use the **same RC-FROZEN SHA**;
- native local display and local input must coexist with remote view/control as required;
- abrupt-disconnect held-state cleanup, reconnect, explicit HyRemote runtime stop/policy transition and no-late-input behavior are retained;
- hosted/Xvfb evidence cannot substitute for physical local-display/local-input evidence;
- physical evidence cannot substitute for hosted build/install/deploy gates.

A historical physical run whose candidate SHA differs from the frozen SHA is useful preflight evidence only.

## 4. Product/repository verification envelope

The exact accepted candidate must retain the frozen architecture:

- one normal installed C++ target `HyRemote::RemoteAccess` / one shared `HyRemoteRemoteAccess` runtime;
- QML is a thin wrapper over that runtime;
- `qhyremote` is an exact-private-ABI native-delegate-preserving platform payload, not an application link target;
- Core is STATIC internal/source composition, not an installed normal SDK target;
- one `hyremote_deploy(TARGET ... [QML] [QPA])` entry point;
- no backend/platform-specific type leaks into stable application API.

Required repository evidence includes, where applicable:

- clean configure/build on exact claimed Qt/toolchain rows;
- deterministic Core/RFB/RemoteAccess/Widgets/Quick/QML/QPA tests;
- installed and source consumers;
- clean relocated C++/QML/QPA/combined QML+QPA deployment without original SDK/build-tree path assistance;
- standard viewer connect/view/input/disconnect/reconnect;
- bounded slow/malformed/incomplete clients and input lifetime;
- truthful security behavior from #143 and safe defaults;
- truthful encoding/bandwidth behavior from #144/#9;
- exact compatibility disposition from #57;
- package contents, dependency licenses/notices and release metadata from #107;
- docs/examples correspond to the candidate being accepted.

Do not hard-code a transient implementation limitation such as `SecurityType None` as a permanent release invariant. The release must instead contain an explicit `## Security boundary` section describing the final implemented/accepted security profile.

## 5. Cut the release branch only after acceptance authorities allow it

Once #33 records the accepted evidence envelope and the authority manifest is satisfied:

1. create `release/v1.0.0.0` from the accepted/frozen integration point;
2. change root CMake version to exactly `1.0.0.0`;
3. open `release/v1.0.0.0 -> main` as Draft;
4. repeat applicable build/package/release verification on the exact versioned branch;
5. only release-blocking packaging/docs/evidence corrections may land directly on the release line; a material product change returns through #165 change control/reacceptance.

Editing release notes or toggling PR Draft state cannot substitute for authority closure.

The release branch is therefore a versioned tests/finalization line, not a place to finish missing product work.

## 6. Finalize, merge to `main`, and tag

Only after the exact versioned candidate passes its release checks:

1. remove `Status: **candidate / acceptance pending**` from the release notes;
2. move the release PR out of Draft;
3. merge it to `main`;
4. verify current `main` HEAD is the accepted release commit and CMake version is `1.0.0.0`;
5. create annotated `v1.0.0.0` on that exact current main HEAD;
6. let tag audit verify annotated-tag type, exact-main equality, CMake-version equality and finalized notes;
7. publish from that tag only.

No lightweight tag, ancestor tag, moved/reused tag or tag-first release flow is permitted.

## 7. Audited backmerge

After the release/tag fact exists:

1. create `backmerge/v1.0.0.0` from released `main` history;
2. preserve the tagged release commit;
3. restore root development version `0.0.0`;
4. open `backmerge/v1.0.0.0 -> develop`;
5. verify the annotated tag is in current `main` history and the backmerge contains the released commit;
6. reconcile later approved `develop` work through the normal PR merge.

Only then is the Git Flow release cycle closed.

## 8. Branch enforcement truth

Where repository settings do not provide pre-push branch protection, the Git Flow workflow's `main`/`develop` direct-push check is an **after-the-fact direct-push audit**. It cannot undo a push and must not be described as branch protection.

Merged topic branches should auto-delete. Residual unowned branches are audited through #156 and later #166 governance.

## 9. Post-V1 boundary

Under #147/#157, only inherently embedded/platform/hardware work stays outside the x86 V1 mandatory set, including #17 GBM/DMA-BUF, #10 RK3588/RKMPP and the embedded EGLFS/platform evidence line. Post-V1 governance improvement #166 also does not block V1.

Qualification infrastructure such as #134 may continue independently but cannot silently move the RC-FROZEN candidate or broaden support claims.
