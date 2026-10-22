# HyRemote - Local Owner execution brief (2026-10-21)

> Status: **accepted execution guidance** from #549 / merged PR #550, with desktop/ARM64 release-scope decision #558 / merged PR #559. This is a living Owner handoff, not a proposal awaiting first merge. It does not override #1/#24/#33/#95/#343 or `.github/release/release-trains.json`; later GA scope changes still need an explicit Owner decision and reviewed authority update.

## 0. Baseline and invariants

- This brief was prepared against `develop@d71526d`. That is a documentation baseline, not a permanent runtime lock. Every execution must re-check current `develop`, the latest authoritative Issue comments, the active PR, the actual code and the exact CI SHA.
- **V0.3.0.0 Product Preview was released on 2026-10-19**, see the [tag/release](https://github.com/skawu/HyRemote/releases/tag/v0.3.0.0). Historical tags, `main`, release branches and accepted artifacts cannot be rewritten by this guidance. The root version on `develop` remains `0.0.0`.
- `V0.3.3.0` is the activated **Qt 6.8.3 Windows/Linux x86_64 desktop** product-breadth Feature with mandatory children **#144 + #175** and candidate prerequisite #261. Owner scope decision [#558](https://github.com/skawu/HyRemote/issues/558) assigns ARM64/EGLFS to the mandatory **pre-V0.4** #236/#154/#203/#343 gate, not to this desktop Feature. `0.3.1.0` and `0.3.2.0` are retired and must not be revived. **V0.4.0.0 remains conditionally inactive and qualification-only**.
- Architecture freeze: one Core, one Shared Runtime; C++ / QML / Generic / QPA are peer integration technologies whose applicability is decided per exact cell; Widgets / Quick are UI targets; RFB is the first transport, not the product identity.
- Default security reality: listen on `0.0.0.0:5921`, with no authentication, no encryption and remote input disabled by default. Optional VNC password authentication is still **unencrypted**; `AuthenticatedEncrypted` is unavailable and fails closed. Do not claim the current product is safe for direct Internet exposure.
- This execution order does not add TLS, cloud relay, Session Registry, a first-party browser viewer or GPU/hardware encoding to GA. #123 is activated only after measurement proves a product blocker.

## 1. Owner and Agent split

**Mode: AGENT_PRIMARY_HUMAN_ASSISTED.** Cloud and local agents own latest-baseline verification, Issue/PR work, repeatable tests, minimal implementation, review-thread dispositions, evidence packaging, PASS/FAIL/BLOCKED decisions and status reporting. The Local Owner decides product scope, GA support changes, material risk acceptance, physical devices and operations that require identity or release authority. Do not hand installation, execution or screenshot archiving to a Human when the agent can perform them.

Before each work package, check for an existing merged fix or active parallel PR. Reuse the owning Issue; do not create a duplicate defect. Use `feature/<issue>-<topic> -> develop`; do not push protected branches, merge early or create tags. When a physical device is unavailable, finish the cloud-verifiable part and mark the remainder `PHYSICAL_BLOCKED`; never present hosted CI as physical qualification.

## 2. Current priorities and work packages

### Completed #553 correction baseline — regression watch, not active feature work

**Historical/merged on 2026-10-21:** [PR #553](https://github.com/skawu/HyRemote/pull/553) implements regression fixes for #533 Widgets first-key activation, #544 high-DPI cursor hit-testing, #543 cursor refresh/pixel-format timing and #546 ARM64 profile instructions, and adds review-gate checks. These must **not** be reimplemented by default. Retain deterministic regression coverage and check any outstanding review-thread dispositions at the actual latest SHA; open a bounded defect only if a fresh code/evidence check identifies a still-reproducible issue. Current product-work priority is the **V0.3.3.0 admission table**, not repeated P1/P2 historical triage.

### P0 - Close the V0.3.3.0 implementation-to-acceptance gap

Authorities: the [machine release train](../../.github/release/release-trains.json), [#144](https://github.com/skawu/HyRemote/issues/144), [#175](https://github.com/skawu/HyRemote/issues/175) and [#370](https://github.com/skawu/HyRemote/issues/370).

- Build a capability table with four columns: `implemented`, `automated-tested`, `physical-qualified`, `accepted`. Bind code, CI and physical evidence to an exact SHA.
- #144: prove Raw deterministic fallback, an actual compressed path, incremental requests and lossless damage, no redundant payload on static scenes, bounded slow-viewer/backpressure behavior, resize/full-refresh correctness and per-viewer state isolation. Combine it with #371 Continuous/Fence and #372 demand-driven capture. Do not turn ordinary users into encoding or queue tuners.
- #175: record **at least two maintained real VNC viewers**, not one viewer on one OS. Cover Windows and Linux viewer hosts and, for each product/version, record the target cell, Qt version, security and encoding negotiation, pointer/buttons/wheel/keyboard/basic text, cursor behavior, resize, connect/reconnect/stop, view-only and applicable input rejection. Internal protocol clients are useful but are not maintained GUI-viewer evidence. A single viewer or single OS cell is not acceptance.
- Performance: use the #370 W1 static, W2 localized, W3 interactive and W4 high-motion workloads. Record P50/P95, frame age, CPU/RSS, wire bytes, slow-client behavior and local UI impact. `1080p / wired RTT <= 20 ms / >= 100 Mbps / one qualified viewer`, interaction `P50 <= 200 ms` and `P95 <= 500 ms` are a **bounded envelope**, not an arbitrary-network guarantee. Track the known #410 multi-second stall history; one good run must not hide a tail defect.
- **V0.3.3.0 candidate freeze stays blocked until the desktop #144/#175 evidence and review gate are complete.** The earlier #144 ARM64 conflict was resolved by explicit Owner decision #558: ARM64/EGLFS remains **non-optional for the #343 GA baseline before V0.4 entry**, but does not block this *desktop* Feature. Use the synchronized #144/#175 acceptance and machine-train wording; never call the embedded cell qualified from desktop evidence. Qt5/TLS/cloud are not V0.3.3 children. Formal admission rows are in `docs/internal/v0.3.3.0-release-admission.md`.

### P1 - Parallel mandatory breadth preparation, without displacing the current Feature

**Qt LTS:** [Qt 6.12 LTS was released on 2026-09-30](https://www.qt.io/blog/qt-6.12-released); see the [official Qt release table](https://doc.qt.io/qt-6/qt-releases.html). Produce a no-code, probe-first preflight:

1. Keep the committed Qt 5.15, Qt 6.5 and Qt 6.8 GA baseline (#343/#57) intact. Qt 6.8.3 remains the exact release reference.
2. Check public C++/QML/Generic compilation, dependency and API differences for Qt 6.12.0. Treat QPA separately as an `EXACT PRIVATE ABI` risk; do not infer compatibility across patches.
3. Record installation source/license/toolchain, Qt5 qmake consumption, test cost, real-application coverage and expected maintenance cost. Give the Owner a **GO / DEFER / NO-GO** recommendation.
4. Until the Owner decides, Qt 6.12 is preflight only. Do not change #343/#33, the release map or the current Supported statement.

**Embedded:** #203 Quick-only appliance correctness comes before acceleration. Use RK3588 + Buildroot + Qt 6.8.3 + EGLFS/OpenGL ES as the reference: clean rootfs, no accidental QtWidgets dependency, local Quick display/input, remote view/control/reconnect and runtime/manifest loader provenance. Add one explicit Wayland reference cell; do not extrapolate one board to all ARM64/SoC. Any DMA-BUF/RKMPP/H.264 proposal must first provide #370 failure metrics and root-cause analysis under #123; otherwise DEFER. The parallel preparation and current toolchain blockers are recorded in `docs/internal/qt5-arm64-foundation-2026-10-22.md`; they do not expand V0.3.3.0.

**HyRemoteTool and product description:** align [#378](https://github.com/skawu/HyRemote/issues/378) with the actual shipped roles in the current release. Distinguish the SDK/Runtime (the core product), the Tool Host and Viewer work surfaces, and diagnostics/release tooling. Do not mix a host diagnostic tool, an RFB viewer and an authenticated management plane. Recheck any old V0.2 badge against the real release identity. State the LAN-only, default-insecure boundary separately from authenticated login.

### P2 - V0.4 / V1.0 decision boundary

- **V0.3-family completion:** the applicable reference cells in #343/#57 for Qt 5.15, 6.5 and 6.8; Windows/Linux; x86_64/ARM64; qwindows/qxcb/Wayland/EGLFS; #144/#175/#370; and the #203 embedded dependency. Do not invent a full Cartesian support claim.
- **V0.4 conditional activation:** only after the Product Owner explicitly confirms the GA baseline, compatibility matrix and evidence plumbing (#393) are ready, and after #235 and the machine authority activate it. V0.4 is qualification, defect repair and freeze only; it does not absorb unimplemented Qt/transport/device work.
- **V1.0:** #33/#101/**#104**/#107/#109 public API, package/support and physical local-plus-remote coexistence must close on the same candidate SHA. A GA scope change needs a Decision Record; a guidance document cannot silently upgrade or waive an existing support commitment.

## 3. Evidence and Review Gate format

Every PR or Issue evidence record contains at least:

```text
ISSUE/PR=                   BASE_SHA=       HEAD_SHA=
UI_FAMILY=                 ROUTE=          Qt/OS/CPU/QPA=
BUILD/INSTALL=PASS|FAIL|NOT_RUN (executed job link)
TARGET/VIEWER=product+exact version
ENV=network/graphics/DPR/device identifiers as applicable
TESTS=executed cases and counts, with logs/artifacts
PERF=workload, N, valid/miss, p50/p95, CPU/RSS, frame age, wire bytes
PHYSICAL=PASS|FAIL|BLOCKED|NOT_APPLICABLE (Human only where necessary)
CODE_REVIEW=SHA, findings, threaded disposition, unresolved_count
RESULT=PASS|FAIL|BLOCKED|INSUFFICIENT_EVIDENCE
OWNER_DECISION=required?|link; next owner + smallest next action
```

- **G0 local:** self-tests, static rules and focused tests. **G1 PR:** cheap Git Flow and independent Review-Disposition checks; the full `CI` workflow does **not** run on PR commits (Owner #571). **G1 postmerge:** on protected `develop` push, scoped affected Windows Qt compile/CTest/install evidence runs automatically as OBSERVE; a failure opens an Issue-linked repair task and does not retroactively block the merge. **G2 Feature:** exact-SHA Windows SDK/Viewer/performance evidence. **G3 RC/GA:** separately admitted manual exact-SHA FULL_GATE, long-run/real apps and package/tag agreement. Missing Linux/embedded user-validation evidence is nonblocking under #563. See [CI policy](ci-purpose-and-trigger-policy-2026-10-22.md).
- A workflow success must distinguish `job/step executed`, `skipped` and `runner unavailable`. A pass on an earlier SHA is not a pass on the current SHA. Real GUI, board-level and local-input evidence cannot be replaced by hosted or offscreen runs.
- When a physical environment is unavailable, keep cloud-verifiable checks moving and record the exact `PHYSICAL_BLOCKED` recipe. Ask for the smallest necessary one-off device or viewer interaction.

## 4. Owner decision list

| Decision | Current default | Escalate when |
| --- | --- | --- |
| Qt 6.12 in the first GA? | **Not approved; preflight only** | #57/#343 compatibility results and support cost are available |
| Shrink the #343 GA reference matrix? | **Do not shrink** | A required cell has reproducible evidence of a blocker and an acceptable alternative scope |
| Promise secure public-Internet remote access in the first GA? | **No** | Encryption, identity authorization, threat model and deployable contract exist |
| Turn HyRemoteTool into a separate full management platform? | **No** | A clear user scenario, access-security model and independent product benefit exist |
| A new RFB transport or hardware acceleration? | **Do not activate** | #370 proves an SLO/resource blocker in the exact environment and #123 approves it |
| Freeze the V0.3.3.0 candidate? | **Blocked by desktop #144/#175 fact gates, not ARM64/EGLFS** | Exact SHA, Windows/Linux maintained-viewer matrix, real-viewer #370 P50/P95/frame-age/resources, same-SHA L2 and review closure are ready |

## 5. Copyable directive for a Local Owner or execution agent

> Take over the current `skawu/HyRemote` `develop`. First read `AGENTS.md`, #549 and this brief, #1/#24/#33/#95/#343, `.github/release/release-trains.json` and the latest comments on the relevant Issues. Use **AGENT_PRIMARY_HUMAN_ASSISTED**. Treat merged #553 fixes for #533/#544/#543/#546 as historical regression baseline, not new implementation assignments; triage only evidence-backed current regressions or unresolved review debt. Start from the **NOT_READY** V0.3.3.0 admission matrix and produce exact-SHA implemented/automated/physical/accepted evidence for #144/#175/#370; close Linux maintained GUI viewer, desktop real-viewer SLO, cursor/auth and final same-SHA L2 gates. Run the Qt 6.12 LTS preflight in read-only mode and the #203 Quick-only ARM64 deployment review in parallel, but do not change the mandatory GA matrix without authorization. Follow G0 -> G1 -> G2 -> G3 and the independent Review Gate; mark unavailable physical cells BLOCKED. Every round reports HEAD, PASS/FAIL/BLOCKED, executed evidence links, remaining blockers and the smallest next task. Do not skip Owner scope decisions and do not directly modify `main`, release branches or tags.

## 6. Boundary of this brief

This guidance **landed on `develop` via PR #550** and was re-scoped through accepted #558/PR #559. It is not a replacement authority for #343, #33, #235 or the machine release map. The Qt 6.12 date comes from the official 2026-09-30 announcement. This brief does not claim that Qt 6.12 or ARM64 was compiled or run here. The merged #553 fixes are a regression baseline; the current P0 is the V0.3.3.0 admission gap, not a repeat of that historical triage.
