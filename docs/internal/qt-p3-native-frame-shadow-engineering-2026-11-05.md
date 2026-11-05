# #609 P3 engineering-only branch — Human authorization A

This branch is isolated from `develop` and **not approved for merge or release**.

## Slice P3-S0 (implemented)

- Build option `HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW=OFF` by default.
- Even when built ON, the observer requires `HYREMOTE_P3_SHADOW_OBSERVER=1` to observe native frames.
- Only exact Qt 6.8.3 and matching `Qt6::QuickPrivate` / `Qt6::GuiPrivate` are accepted. Qt 5 Quick remains experimental and unchanged.
- The observer attaches to the existing Qt6 Quick `afterRendering` stage. S0 counts valid RHI+swapchain access without readback, staging GPU commands, requesting render or modifying image data.
- No new frame is published. Current `grabToImage`, Core scheduling, Viewer bootstrap, frame storage, RFB, authorization, watermark and privacy paths are unchanged even when the observer is on.
- A pure policy test prevents accidental production publication. Off-path builds use no private-Qt types or extra libraries.

## Slice P3-S1 (implemented, shadow only)

- Bounded asynchronous QRhi readback of the current swapchain image: at most **8 submissions**, minimum **250 ms** interval, **at most one request in flight**; further attempts are counted as dropped.
- `QRhiReadbackDescription{}` (null texture = current swapchain image) recorded through `swapchain->currentFrameCommandBuffer()->resourceUpdate()`; **no `rhi->finish()`**, no extra render, no forced backend, no `grabToImage`/`grabWindow`.
- The completion callback owns and deletes its `QRhiReadbackResult`; it touches only atomics and the result, never the window, render target or any Qt GUI object. `stop()` disconnects first and does not wait on the render thread.
- Read-back pixels are counted (`readback_discarded_bytes`) and discarded; nothing reaches Core, RFB or a Viewer. The stop diagnostic gains `readback_submitted/completed/dropped/discarded_bytes` while keeping `pixels_published=0`.
- Regression covered by `hyremote-quick-native-frame-policy-test` (bound constants) plus local Viewer smoke: env absent, env `=1`, and repeated window resize during sampling, all with clean `HYREMOTE_STOP=Stopped`.

## Slice P3-S1R (implemented, shadow only)

- Sampling is restricted to the **D3D11 RHI backend** (`QRhi::D3D11`); other backends only count `readback_backend_skipped` and never allocate a readback result. Rationale from the exact Qt 6.8.3 sources: `QRhiD3D11::destroy()` calls `finishActiveReadbacks()` before releasing the device/context, and Vulkan does the same; the GLES2 backend does not advertise `ReadBackAnyTextureFormat`, so it is not a valid sampling target.
- Diagnostics now carry lifecycle/integrity facts: `readback_same_frame/later_frame`, `readback_empty`, `readback_data_size_mismatch` (bytes vs WxHx4), `readback_submit_size_diff` (completion size vs submit size), `swapchain_size_changes`, `scenegraph_invalidations`, `readback_after_invalidation`, `readback_in_flight_at_stop`, `readback_after_stop`, `readback_across_recreation`, `readback_max_flight_ms`, `readback_min/max_bytes`, `readback_format`, `readback_size`. A `HyRemote P3 SHADOW LATE COMPLETION` line is emitted when a completion arrives after stop/invalidation.
- Verified on exact Qt 6.8.3 / D3D11 with a 1920x1080 looping `OpacityAnimator` host:
  - stop during a pending readback: `in_flight_at_stop=1`, late completion with `bytes=8294400 format=1 1920x1080 flight_ms≈25`, no crash/hang;
  - window destroy during a pending readback: late completion `flight_ms≈950–980` delivered by the RHI destruction drain, same valid bytes/format/size;
  - repeated resize: `swapchain_size_changes=12–16`, readbacks observed at both `4096000` (1280x800) and `8294400` (1920x1080), `data_size_mismatch=0`, one `submit_size_diff` proving a readback submitted on the old swapchain completed after recreation with a self-consistent payload.
- Pixels remain discarded; no Core/RFB/Viewer publication.

## Pending BLOCKERS before any pixel publication

P3-R1 native swapchain vs `contentItem` geometry/alpha/DPR/Popup and security equivalence;
P3-R2 asynchronous callback ownership across stop/device-loss/resize and Core requestId mapping;
P3-R5 static Viewer join/reconnect/full-frame initialization/fallback;
P3-R6 server-side watermark/privacy/authorization must remain fail-closed.
Qt5 full-frame sync cost, Qt6 high-frequency readback, cross-platform ABI/license and RealVNC/RK3588 A/B remain open.
P3-S1 and S1R (bounded asynchronous shadow readback plus lifecycle closure) are implemented and locally verified; they still publish nothing. `QQuickWindow::sceneGraphInvalidated` did not fire in the destroy runs, so that specific hook remains unexercised; RHI destruction drain was exercised instead. Remaining before any publication decision: device-loss/backend-change runs, geometry/alpha/DPR equivalence and Viewer A/B, all behind the same compile-time OFF + runtime opt-in gates.

## Test governance (2026-11-05)

- Every new, modified or executed test requires a prior five-point justification (name and goal; real risk; why existing evidence cannot replace it; timing/environment/scope/run cap; PASS/FAIL, consequence and cost) and explicit Human confirmation. Code review, QtPrivate/QRhi analysis, design and ordinary engineering fixes remain autonomous.
- No self-expansion of the test matrix, no ad-hoc CTest additions, no repeated full-suite runs and no unbounded fix-retest loops.
- Status at `fc1bf73d`: S1R lifecycle evidence is complete. The final-SHA Viewer smoke is **pending confirmation**; the resize-repetition candidate was withdrawn as redundant (mechanism existence, not frequency, is what S1R needs). Unapproved sweep/hit scripts are marked non-runnable.
- Interactive runtime checks must print per-stage progress and carry a hard timeout so a normal wait is never mistaken for a hang.

## Developer verification

1. Normal default Qt6/Qt5 builds: `HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW:BOOL=OFF`. Existing Quick CTests plus `hyremote-quick-native-frame-policy-test` must pass.
2. Separately, exact Qt6.8.3/MSVC developer build: configure ON and verify `Qt6::QuickPrivate`/GuiPrivate link. Never use the Release deployment profile or publish an installer from this build.
3. Local Qt6 native-window smoke: first without env opt-in, then with `HYREMOTE_P3_SHADOW_OBSERVER=1`. Verify existing Viewer pixels remain from the original capture path; only developer diagnostic `HyRemote P3 SHADOW ONLY ... readback_submitted/completed/dropped/discarded_bytes ... pixels_published=0` may appear on stop.
4. If a QtPrivate link failure, render/GUI deadlock, regression or unqualified surface appears, stop. This does not authorize temporary Qt patching or bypassing the gate.

No performance/CPU benefit is claimed by P3-S0. All experiments remain #609 P1 OPEN.


## Local Owner delegated engineering authority (2026-11-05)

Human clarified that Local Owner runs Codebuddy (DeepSeek-V4.1-Flash) as a capable autonomous coding agent, not a manual command relay. **Cloud Owner defines architecture, safety and final merge gates; Local Owner owns the Windows Qt engineering workstream end-to-end** under the existing Human-A isolated/opt-in authorization.

### Local Owner can proceed autonomously without repeated Human/Cloud forwarding

- Inspect all #853 changed files and their integration call sites; perform an independent substantive code review and resolve product correctness defects **within the already authorized shadow-only scope**.
- In `feature/609-p3-native-frame-shadow-qt6` (after rebasing/refreshing its local worktree safely), repair ordinary CMake/configure mistakes, source compile/link errors, test catalog/test logic issues, and bounded Qt6.8.3 runtime lifecycle mistakes. Add/revise **relevant tests** with catalog entries in the same commit. Commit and push scoped fixes; preserve PR #853 as **Draft**. Cite exact SHA/evidence and review noteworthy assumptions.
- Re-run the normal OFF build & strategy CTest; run separate exact Qt6.8.3 ON build; test both env-var absent and `HYREMOTE_P3_SHADOW_OBSERVER=1` in a minimal real Quick+Viewer application; check screenshot/pixel and exit/stop correctness and console `pixels_published=0`. Do not infer Windows tests from green CI because hosted Qt jobs were skipped.
- After S0 passes, autonomously review/design a **separate P3-S1 shadow-only asynchronous QRhi readback slice** on the same Draft feature branch, with reviewable commits and deterministic tests, **without any Core frame publication**. Set a tight readback budget (e.g. 6 Hz, at most one in-flight) and no `rhi->finish()`, no forced graphics backend, no extra `update/grab`. Do not publish a potentially unsafe S1 commit as working until the result/callback lifecycle is demonstrably safe through `stop`, resize, expose/hide, target destruction and QRhi loss; a failed proof should be a documented STOP for S1 only, not an invitation to bypass ownership constraints.
- Maintain a single concise #853 / #609 result with exact commit, Qt Kit, tested operation, PASS/FAIL/UNKNOWN, warnings, and any unresolved blockers. Avoid repeated status-only issues or redundant large matrices.

### Mandatory escalation / cannot self-authorize

- Changing Core/Shared Runtime public ABI or protocols; altering ViewerCaptureDemand for other entry modes; replacing `grabToImage` with native readback in remote output; watermark/privacy/authorization policy; removing protective bootstrap/fallback.
- Production enablement, QtPrivate version widening, Qt SDK patch/rebuild/distribution, new backends/OS-specific capture, broad acceptance/release claims, pushing to `develop`, merging PR, or publishing packages.
- A risk where pixels, asynchronous resources or authentication could become observable after `stop`, or an irreducible Qt/renderer ownership ambiguity. Record the exact first blocker and the best bounded options; Cloud Owner decides architecture next, Human approves product-implementation scope.

**Current fixes**: S0 originally failed the root semantic CTest catalog gate, not Qt linking. Cloud Owner independently fixed the missing `hyremote-quick-native-frame-policy-test` row in commit `85e50f703f1382f024f7846e994648e289588f9f`. Local Owner should update to this or newer SHA and resume; **do not repeat the already-completed catalog repair**. Green GitHub CI for #853 skipped all Windows Qt builds, so compile and runtime remain `NOT_VERIFIED` until Local Owner evidence.

The objective is to complete authorized engineering work, not to stop on every self-fixable error. Human only participates again when a previously reserved architectural, security, merge or release decision is genuinely required.
