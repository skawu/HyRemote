# HyRemote V1 physical/native acceptance runbook

Status: **repository preparation only — this document is not physical acceptance evidence**

Authority: #109, with final V1 release authority in #33.

This runbook turns the frozen #109 physical/native acceptance matrix into a repository-versioned execution record. It does not create a fourth integration mode, a test-only API, a second runtime, or a hidden live input-policy channel.

## Entry condition

Do not execute the physical/native acceptance merely to compensate for hosted CI that has not run. The exact candidate under test must first be repository-complete enough that the corresponding hosted/reference Windows/Linux gates have actually executed far enough to make native validation meaningful.

A queued/no-step hosted job is not a reason to substitute a Local Agent run. The Local Agent or other physical host is appropriate only after #109 reaches this execution entry condition.

## Candidate identity — fill before executing

Record one immutable candidate for the complete evidence set:

- Candidate commit SHA: `<required>`
- Branch / release candidate: `<required>`
- PR: `#106` or the authorized release PR derived from the accepted integration line
- Build type: `<required>`
- Qt version: **6.8.3** for the V1 reference candidate
- Evidence record owner/date: `<required>`

If the candidate SHA changes after a product/runtime/deployment change, the affected physical cells must be rerun. Do not combine observations from different product commits into one PASS envelope without recording why they remain applicable.

## Environment record — one per operating system

### Windows x86_64

- Candidate SHA: `<required>`
- Windows edition/build: `<required>`
- CPU/GPU: `<required>`
- Toolchain: `<required>`
- Qt: `6.8.3`
- Native delegate: `qwindows`
- Rendering backend: `<required>`
- Physical display(s), scale/DPR: `<required>`
- Viewer name/version: `<required>`
- Build/deploy commands: `<required>`
- Logs/screenshots/video links: `<required>`

### Linux x86_64

- Candidate SHA: `<required>`
- Distribution/version/kernel: `<required>`
- Display server/session: `<X11/other qualified native path>`
- CPU/GPU/driver: `<required>`
- Toolchain: `<required>`
- Qt: `6.8.3`
- Native delegate: `qxcb`
- Physical display(s), scale/DPR: `<required>`
- Viewer name/version: `<required>`
- Build/deploy commands: `<required>`
- Logs/screenshots/video links: `<required>`

Headless/offscreen/Xvfb evidence may complement these records but cannot replace the native local-display/local-input observations below.

## Common evidence rules

For every E1–E4 OS/mode cell, record:

- exact launch command/configuration;
- visible native local display before and during remote viewing;
- local pointer/keyboard/text behavior;
- remote view behavior;
- remote input behavior when disabled and when enabled;
- viewer disconnect/reconnect result;
- application/runtime lifecycle used for the policy transition;
- held-input abrupt-disconnect result where required;
- held-input explicit-stop/policy-transition result where required;
- confirmation that no queued remote input is delivered after explicit stop;
- bounded slow-viewer/responsiveness observation;
- anomalies/known limitations;
- evidence links;
- final cell result: `PASS` or `FAIL`.

A partial/headless-only record cannot mark an OS/mode Supported.

## E1 — Embedded C++ / Widgets

Run on both Windows x86_64 and Linux x86_64 using the normal E1 Widgets application and only the public `HyRemote::RemoteAccess` integration contract.

1. Launch the target application on the native platform and prove visible local rendering plus local pointer, keyboard and text input before a remote viewer connects.
2. Start HyRemote in the default view-only policy and connect the standard viewer. Prove local input remains authoritative/responsive and attempted remote input is not delivered.
3. Keep the target application running. Stop the **HyRemote remote runtime only**, set `remoteInputEnabled=true` while Stopped, then start HyRemote again. Reconnect the viewer as needed. This is the frozen `stop -> configure -> start` policy transition; do not introduce a hidden live policy channel.
4. Prove remote pointer/keyboard/text reaches the same still-running application while local input continues to work.
5. Abrupt-disconnect case: hold a supported remote modifier and/or mouse button, terminate/disconnect the viewer, and prove the target returns to neutral input state; reconnect and prove the next viewer begins clean.
6. Explicit-stop case: during control mode hold a supported remote modifier and/or button, call/trigger `RemoteAccess::stop()` while the application keeps running, and prove delivered held state is balanced.
7. Immediately after explicit stop, interact locally and observe long enough to prove no previously queued remote key/button is delivered late. Restart the remote runtime and prove clean input state.
8. Disconnect/reconnect again without restarting the target application.
9. Stop HyRemote and confirm the local UI remains normally usable. Where practical, transition back to view-only via `stop -> configure -> start` and repeat the terminal-input-cleanup observation.
10. Under a deliberately slow/stalled remote viewer within the existing product path, confirm remote activity does not indefinitely stall the native UI/render loop.

Record per OS:

- Windows E1: `<PASS/FAIL + evidence links>`
- Linux E1: `<PASS/FAIL + evidence links>`

## E2 — Embedded C++ / Qt Quick

Repeat the E1 lifecycle on both operating systems with the normal `QQuickWindow` path.

In addition to the E1 evidence, include:

- text focus/input on a real Quick control;
- native resize behavior;
- a display/DPR interaction relevant to the physical host;

The **semantics** behind the resize/DPR and routing expectations are already covered deterministically, so this cell is a
host-specific observation rather than the first place they are discovered: `hyremote-widgets-capture-test-dpr150` and
`hyremote-quick-capture-test-dpr150` pin the "device pixel ratio applied exactly once" contract at a non-1 ratio (the
`HYREMOTE_EXPECT_DPR` gate keeps that coverage from being vacuous), and `hyremote-widgets-input-routing-test` /
`hyremote-quick-input-routing-test` pin nested, transparent and disabled child routing, the held drag grab, and
item-level focus/text delivery. What remains genuinely physical here is how the host's own compositor and display report
and apply the scale while a viewer is attached.
- abrupt-disconnect held-input cleanup;
- explicit-stop held-input cleanup;
- no late queued remote input after stop;
- same-process `stop -> configure -> start` policy transition;
- bounded slow-viewer/native-responsiveness observation.

Record per OS:

- Windows E2: `<PASS/FAIL + evidence links>`
- Linux E2: `<PASS/FAIL + evidence links>`

## E3 — Declarative QML

Run on both operating systems using only `import HyRemote` and the QML `RemoteAccess` type over the same shared runtime.

1. Prove native local display/input before and during default view-only remote viewing.
2. Prove attempted remote input is not delivered while view-only.
3. Keep the application running. Disable/stop the QML RemoteAccess runtime, change `remoteInputEnabled`, then re-enable/restart it; reconnect the viewer as needed. This is the declarative form of the same stopped-runtime policy transition, not a second runtime and not a live authorization bypass.
4. Prove remote pointer/keyboard/text plus continued native local input.
5. Hold a supported remote key/button, disable/stop the QML RemoteAccess runtime, and prove neutral local input state with no late queued remote event after stop.
6. Disconnect/reconnect without restarting the application and verify diagnostics/lifecycle remain consistent with the shared C++ runtime.
7. Include a bounded slow-viewer/native-responsiveness observation.

Record per OS:

- Windows E3: `<PASS/FAIL + evidence links>`
- Linux E3: `<PASS/FAIL + evidence links>`

## E4 — Transparent QPA Proxy / existing Qt-only application

This is the defining physical proof that HyRemote is a native-delegate-preserving proxy rather than replacement-only `qvnc` usage. Run on both operating systems using the ordinary Qt-only E4 application deployed through the installed SDK/QPA path.

### Native baseline

1. Run the deployed application using the normal native platform (`windows` on Windows, `xcb` on Linux).
2. Prove native local display, pointer, keyboard, text, secondary-window/dialog/menu behavior.

### HyRemote view-only run

1. Relaunch the **same ordinary application product target** through `-platform hyremote` with default view-only policy.
2. Prove the application remains visibly rendered locally through the native delegate.
3. Connect a viewer and prove remote view while local pointer/keyboard/text and supported secondary-window/dialog/menu behavior remain functional.
4. Prove attempted remote input is not delivered.
5. Disconnect/reconnect the viewer within this run and verify native local behavior remains intact.

### HyRemote control run

1. End the view-only process normally and relaunch the same application using `-platform "hyremote:hyremote-input=true"` (plus the selected test port if needed). The V1 policy transition is a documented **process relaunch**; preserving the same process/viewer socket is not required.
2. Prove remote pointer/keyboard/text coexists with continued native local input/display.
3. Exercise supported second-window/dialog/menu/popup churn during this control-mode run.
4. If a qualified child surface is hidden/removed while it owns delivered remote held input, prove the still-running application does not retain that synthetic press.
5. Disconnect/reconnect the viewer without restarting the application during the control run.
6. Confirm local display/input remain functional after remote disconnect.
7. Terminate the control-mode process normally after remote input activity and prove no teardown crash/hang or late remote-input behavior.
8. Include a bounded slow-viewer/native-responsiveness observation.

Record per OS:

- Windows E4: `<PASS/FAIL + evidence links>`
- Linux E4: `<PASS/FAIL + evidence links>`

## Cross-cell held-input checklist

The physical evidence envelope is incomplete unless it includes, at minimum:

- E1 Windows: abrupt viewer disconnect while held + explicit runtime stop while held + no late queued remote input;
- E1 Linux: same;
- E2 Windows: same, independently observed on Quick;
- E2 Linux: same;
- E3 Windows/Linux: explicit declarative stop/disable while held + no late queued remote input;
- E4 Windows/Linux: child-surface cleanup where applicable and normal process teardown after remote input.

Deterministic CTest evidence for disconnect/backpressure/terminal cleanup complements these observations but does not replace them.

## Final physical acceptance record

Complete only after every required cell above has evidence on the exact candidate:

- Candidate SHA: `<required>`
- Windows E1: `<PASS/FAIL>`
- Windows E2: `<PASS/FAIL>`
- Windows E3: `<PASS/FAIL>`
- Windows E4: `<PASS/FAIL>`
- Linux E1: `<PASS/FAIL>`
- Linux E2: `<PASS/FAIL>`
- Linux E3: `<PASS/FAIL>`
- Linux E4: `<PASS/FAIL>`
- Windows bounded responsiveness: `<PASS/FAIL>`
- Linux bounded responsiveness: `<PASS/FAIL>`
- Evidence bundle links: `<required>`
- Known limitations/anomalies: `<required, use none if none>`
- #109 decision: `<OPEN until evidence reviewed; PASS only after review>`

**Do not mark #109 or V1.0.0.0 accepted merely because this runbook exists or has been partially filled.** #109 closes only after the exact candidate has accepted physical/native Windows and Linux evidence. #104 hosted/reference execution and the remaining #33 release authorities remain independent mandatory gates.
