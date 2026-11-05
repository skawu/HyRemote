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
