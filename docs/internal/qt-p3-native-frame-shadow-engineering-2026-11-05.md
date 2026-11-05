# #609 P3 engineering-only branch — Human authorization A

This branch is isolated from `develop` and **not approved for merge or release**.

## Slice P3-S0 (implemented)

- Build option `HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW=OFF` by default.
- Even when built ON, the observer requires `HYREMOTE_P3_SHADOW_OBSERVER=1` to observe native frames.
- Only exact Qt 6.8.3 and matching `Qt6::QuickPrivate` / `Qt6::GuiPrivate` are accepted. Qt 5 Quick remains experimental and unchanged.
- The observer attaches to the existing Qt6 Quick `afterRendering` stage. It counts valid RHI+swapchain access without readback, staging GPU commands, requesting render or modifying image data.
- No new frame is published. Current `grabToImage`, Core scheduling, Viewer bootstrap, frame storage, RFB, authorization, watermark and privacy paths are unchanged even when the observer is on.
- A pure policy test prevents accidental production publication. Off-path builds use no private-Qt types or extra libraries.

## Pending BLOCKERS before any pixel publication

P3-R1 native swapchain vs `contentItem` geometry/alpha/DPR/Popup and security equivalence;
P3-R2 asynchronous callback ownership across stop/device-loss/resize and Core requestId mapping;
P3-R5 static Viewer join/reconnect/full-frame initialization/fallback;
P3-R6 server-side watermark/privacy/authorization must remain fail-closed.
Qt5 full-frame sync cost, Qt6 high-frequency readback, cross-platform ABI/license and RealVNC/RK3588 A/B remain open.
The **next slice** is a bounded QRhi asynchronous CPU frame readback in the shadow buffer only, with lifecycle/drain regression tests and no remote publication. Do not turn it on before exact Kit compile and tests pass.

## Developer verification

1. Normal default Qt6/Qt5 builds: `HYREMOTE_EXPERIMENTAL_P3_NATIVE_FRAME_SHADOW:BOOL=OFF`. Existing Quick CTests plus `hyremote-quick-native-frame-policy-test` must pass.
2. Separately, exact Qt6.8.3/MSVC developer build: configure ON and verify `Qt6::QuickPrivate`/GuiPrivate link. Never use the Release deployment profile or publish an installer from this build.
3. Local Qt6 native-window smoke: first without env opt-in, then with `HYREMOTE_P3_SHADOW_OBSERVER=1`. Verify existing Viewer pixels remain from the original capture path; only developer diagnostic `HyRemote P3 SHADOW ONLY ... pixels_published=0` may appear on stop.
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
