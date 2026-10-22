# HyRemote - Local Owner execution brief (2026-10-21)

> Status: a work-guidance and rebaseline proposal submitted through #549. It tells the Local Owner and execution agents how to sequence work, verify claims and request decisions. It does not change the product commitments in #1/#24/#33/#95/#343 and does not replace the machine release authority in `.github/release/release-trains.json`. Any change to the GA matrix or a release gate needs an explicit Owner decision and a reviewed atomic authority update.

## 0. Baseline and invariants

- This brief was prepared against `develop@d71526d`. That is a documentation baseline, not a permanent runtime lock. Every execution must re-check current `develop`, the latest authoritative Issue comments, the active PR, the actual code and the exact CI SHA.
- **V0.3.0.0 Product Preview was released on 2026-10-19**, see the [tag/release](https://github.com/skawu/HyRemote/releases/tag/v0.3.0.0). Historical tags, `main`, release branches and accepted artifacts cannot be rewritten by this guidance. The root version on `develop` remains `0.0.0`.
- `V0.3.3.0` is the activated next product-breadth Feature in the machine release map. Its mandatory children are **#144 + #175** and its candidate prerequisite is #261. `0.3.1.0` and `0.3.2.0` are retired technical slots and must not be revived. **V0.4.0.0 remains conditionally inactive and qualification-only**; do not describe it as activated.
- Architecture freeze: one Core, one Shared Runtime; C++ / QML / Generic / QPA are peer integration technologies whose applicability is decided per exact cell; Widgets / Quick are UI targets; RFB is the first transport, not the product identity.
- Default security reality: listen on `0.0.0.0:5921`, with no authentication, no encryption and remote input disabled by default. Optional VNC password authentication is still **unencrypted**; `AuthenticatedEncrypted` is unavailable and fails closed. Do not claim the current product is safe for direct Internet exposure.
- This execution order does not add TLS, cloud relay, Session Registry, a first-party browser viewer or GPU/hardware encoding to GA. #123 is activated only after measurement proves a product blocker.

## 1. Owner and Agent split

**Mode: AGENT_PRIMARY_HUMAN_ASSISTED.** Cloud and local agents own latest-baseline verification, Issue/PR work, repeatable tests, minimal implementation, review-thread dispositions, evidence packaging, PASS/FAIL/BLOCKED decisions and status reporting. The Local Owner decides product scope, GA support changes, material risk acceptance, physical devices and operations that require identity or release authority. Do not hand installation, execution or screenshot archiving to a Human when the agent can perform them.

Before each work package, check for an existing merged fix or active parallel PR. Reuse the owning Issue; do not create a duplicate defect. Use `feature/<issue>-<topic> -> develop`; do not push protected branches, merge early or create tags. When a physical device is unavailable, finish the cloud-verifiable part and mark the remainder `PHYSICAL_BLOCKED`; never present hosted CI as physical qualification.

## 2. Current priorities and work packages

### P0 - Clear the known input-correctness and review-quality risks

The Owner assigns one bounded defect triage. The four findings below do not have to land in one PR.

| Evidence/root cause | Minimum verification or fix | Automated acceptance |
| --- | --- | --- |
| [PR #533](https://github.com/skawu/HyRemote/pull/533) P1: first key after Widgets activation | Verify the path where `keyboardReceiver(root)` is cached before `activateWindow()` and falls back to the root; preserve application scope and OS foreground-lock limits | Inactive-window switch, first and continuous keys in a text field, KeyUp and rejected-focus cases |
| [PR #544](https://github.com/skawu/HyRemote/pull/544) P1: RFB pointer to HiDPI Qt logical coordinates | Use the same framebuffer-to-logical transform for cursor hit-testing and input; do not assume DPR=1 | Executable DPR 1/1.5/2 Widgets and Quick hit-test plus resize/reconnect |
| [PR #543](https://github.com/skawu/HyRemote/pull/543) P2: cursor poll timing | Do not count a 250 ms poll when no cursor was sent; verify a viewer that sends `SetEncodings` before `SetPixelFormat` | 8/16 to 32 bpp, first FramebufferUpdateRequest, static first cursor and sustained connection |
| [PR #546](https://github.com/skawu/HyRemote/pull/546) P2: AArch64 profile guidance | When copying `build.yml`, set `build.examples=false` and only the intended `integrations`; avoid duplicate `CMAKE_BUILD_TYPE`/`build.type` | Profile parsing assertion plus docs path/self-service checks |

These items are risks to re-verify, not proof that every platform reproduced them. A merged PR is not proof of a fix. Close each item with exact-SHA test evidence, the relevant code path, a threaded `Review-Disposition:` reply and an explicit resolution or Owner-accepted risk record. Do not remove a P1 merely because CI is green.

### P1 - Close the V0.3.3.0 implementation-to-acceptance gap

Authorities: the [machine release train](../../.github/release/release-trains.json), [#144](https://github.com/skawu/HyRemote/issues/144), [#175](https://github.com/skawu/HyRemote/issues/175) and [#370](https://github.com/skawu/HyRemote/issues/370).

- Build a capability table with four columns: `implemented`, `automated-tested`, `physical-qualified`, `accepted`. Bind code, CI and physical evidence to an exact SHA.
- #144: prove Raw deterministic fallback, an actual compressed path, incremental requests and lossless damage, no redundant payload on static scenes, bounded slow-viewer/backpressure behavior, resize/full-refresh correctness and per-viewer state isolation. Combine it with #371 Continuous/Fence and #372 demand-driven capture. Do not turn ordinary users into encoding or queue tuners.
- #175: record **at least two maintained real VNC viewers**, not one viewer on one OS. Cover Windows and Linux viewer hosts and, for each product/version, record the target cell, Qt version, security and encoding negotiation, pointer/buttons/wheel/keyboard/basic text, cursor behavior, resize, connect/reconnect/stop, view-only and applicable input rejection. Internal protocol clients are useful but are not maintained GUI-viewer evidence. A single viewer or single OS cell is not acceptance.
- Performance: use the #370 W1 static, W2 localized, W3 interactive and W4 high-motion workloads. Record P50/P95, frame age, CPU/RSS, wire bytes, slow-client behavior and local UI impact. `1080p / wired RTT <= 20 ms / >= 100 Mbps / one qualified viewer`, interaction `P50 <= 200 ms` and `P95 <= 500 ms` are a **bounded envelope**, not an arbitrary-network guarantee. Track the known #410 multi-second stall history; one good run must not hide a tail defect.
- **V0.3.3.0 candidate freeze is blocked until #144/#175 acceptance is complete.** #144's current acceptance text requires an ARM64/embedded reference cell. The release-train rule says V0.3.3 does not reserve ARM64 as a new product expansion, but that does not erase the acceptance requirement in the Issue. Do not classify ARM64 as a non-blocker and do not freeze the candidate until either that cell exists or an explicit Owner scope decision is recorded in #144 and the machine authority. Qt5, TLS and cloud work are not implied children of this Feature; ARM64 is a separate recorded conflict, not a silent exemption.

### P2 - Parallel technical preflight, without displacing the current Feature

**Qt LTS:** [Qt 6.12 LTS was released on 2026-09-30](https://www.qt.io/blog/qt-6.12-released); see the [official Qt release table](https://doc.qt.io/qt-6/qt-releases.html). Produce a no-code, probe-first preflight:

1. Keep the committed Qt 5.15, Qt 6.5 and Qt 6.8 GA baseline (#343/#57) intact. Qt 6.8.3 remains the exact release reference.
2. Check public C++/QML/Generic compilation, dependency and API differences for Qt 6.12.0. Treat QPA separately as an `EXACT PRIVATE ABI` risk; do not infer compatibility across patches.
3. Record installation source/license/toolchain, Qt5 qmake consumption, test cost, real-application coverage and expected maintenance cost. Give the Owner a **GO / DEFER / NO-GO** recommendation.
4. Until the Owner decides, Qt 6.12 is preflight only. Do not change #343/#33, the release map or the current Supported statement.

**Embedded:** #203 Quick-only appliance correctness comes before acceleration. Use RK3588 + Buildroot + Qt 6.8.3 + EGLFS/OpenGL ES as the reference: clean rootfs, no accidental QtWidgets dependency, local Quick display/input, remote view/control/reconnect and runtime/manifest loader provenance. Add one explicit Wayland reference cell; do not extrapolate one board to all ARM64/SoC. Any DMA-BUF/RKMPP/H.264 proposal must first provide #370 failure metrics and root-cause analysis under #123; otherwise DEFER.

**HyRemoteTool and product description:** align [#378](https://github.com/skawu/HyRemote/issues/378) with the actual shipped roles in the current release. Distinguish the SDK/Runtime (the core product), the Tool Host and Viewer work surfaces, and diagnostics/release tooling. Do not mix a host diagnostic tool, an RFB viewer and an authenticated management plane. Recheck any old V0.2 badge against the real release identity. State the LAN-only, default-insecure boundary separately from authenticated login.

### P3 - V0.4 / V1.0 decision boundary

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

- **G0 local:** self-tests, static rules and focused tests. **G1 PR:** affected real Windows/Linux lanes, actual steps, independent Review Gate, zero unresolved major threads or an explicit traceable Owner disposition. **G2 Feature:** clean build/install/deploy, SDK consumer, maintained RFB viewer and W1-W4 evidence on the same SHA. **G3 RC/GA:** full qualification, long-run stability, real applications, required physical cells, and exact tag/artifact/checksum agreement.
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
| Freeze the V0.3.3.0 candidate? | **Blocked by #144/#175 fact gates, including the unresolved ARM64 acceptance cell** | Exact SHA, CI, maintained-viewer matrix, ARM64 decision/evidence and review closure are ready |

## 5. Copyable directive for a Local Owner or execution agent

> Take over the current `skawu/HyRemote` `develop`. First read `AGENTS.md`, #549 and this brief, #1/#24/#33/#95/#343, `.github/release/release-trains.json` and the latest comments on the relevant Issues. Use **AGENT_PRIMARY_HUMAN_ASSISTED**. First recheck whether the merged #533/#544/#543/#546 review findings still exist on the current HEAD; use small PRs under the owning Issue and add deterministic regressions. Then produce the exact-SHA implemented/automated/physical/accepted gap table for V0.3.3.0 #144/#175 and complete the minimum implementation plus viewer/performance admission. Run the Qt 6.12 LTS preflight in read-only mode and the #203 Quick-only ARM64 deployment review in parallel, but do not change the mandatory GA matrix without authorization. Follow G0 -> G1 -> G2 -> G3 and the independent Review Gate; mark unavailable physical cells BLOCKED. Every round reports HEAD, PASS/FAIL/BLOCKED, executed evidence links, remaining blockers and the smallest next task. Do not skip Owner scope decisions and do not directly modify `main`, release branches or tags.

## 6. Boundary of this brief

This guidance may land on `develop` after the Review Gate passes. It is not a replacement authority for #343, #33, #235 or the machine release map. The Qt 6.12 date comes from the official 2026-09-30 announcement. This brief does not claim that Qt 6.12 or ARM64 was compiled or run here, and does not claim that the P0 defects are already fixed.
