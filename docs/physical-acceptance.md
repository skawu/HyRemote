# HyRemote physical local + remote acceptance plan

Status: **execution plan only — no PASS claim until recorded evidence exists**

Authority: #109, feeding milestone authorities #30, #31, #32 and #33.

This plan covers the native/physical behavior that hosted offscreen or Xvfb execution cannot prove. It must not be used as a replacement for mandatory hosted Windows/Linux automation when that automation has not actually executed.

## 1. Entry gate

Do not begin physical acceptance until the relevant release-candidate repository tree is feature-complete and its repository/hosted prerequisites have run far enough to make native validation meaningful.

In particular, a GitHub Actions job with no assigned runner / no executed steps is infrastructure evidence only. Do not compensate by calling a Local Developer Agent and then treating that result as hosted CI evidence.

When physical execution is authorized, record the exact candidate SHA first. Every result in this plan is candidate-specific.

## 2. Environment record

For each OS create a separate evidence record containing:

- candidate commit SHA and branch;
- OS edition/distribution and exact build/version;
- architecture;
- compiler/toolchain;
- Qt version and installation identity;
- GPU and driver where relevant;
- local display topology and scaling/DPR;
- native platform plugin (`windows` or `xcb` for the V1 QPA reference path);
- rendering backend where relevant (Quick/RHI/OpenGL);
- HyRemote build type and relevant build options;
- installed SDK prefix used for deployment tests;
- external RFB/VNC viewer product and version;
- viewer host relationship (same host vs second machine) and network path;
- timestamp/timezone of execution.

Never generalize one machine/OS record to the other reference platform.

## 3. Common preparation

Before mode-specific testing:

1. build/install the exact candidate using its documented release-like configuration;
2. run the candidate's automated local tests that are available on the physical host;
3. verify no unrelated stale HyRemote plugin/runtime is found earlier in PATH/plugin search paths;
4. use the installed/deployed product path where the acceptance cell requires SDK/deployment evidence;
5. keep listener address loopback unless the test explicitly requires a controlled second-host viewer; if a non-loopback bind is needed, use an isolated/trusted test network because the current SecurityType None baseline is unauthenticated/unencrypted;
6. record every command actually executed rather than relying on remembered defaults.

## 4. E1 — Embedded C++ / Widgets

Required on Windows and Linux.

### Baseline local behavior

- launch `widgets-basic` locally;
- verify the native window is visibly rendered;
- use local mouse to operate controls;
- use local keyboard and text input;
- record any focus, DPI or rendering anomaly before remote access is introduced.

### View-only coexistence

- start HyRemote using the example's documented explicit start path/default view-only policy;
- verify local rendering/input still works before viewer connection;
- connect a standard viewer;
- verify remote framebuffer content is correct;
- while viewer remains connected, continue local pointer/keyboard/text interaction;
- verify viewer-sent input is rejected in view-only mode;
- verify local UI responsiveness is not blocked by the viewer.

### Explicit remote control coexistence

- explicitly enable remote input using the supported public policy path;
- verify remote pointer, buttons, wheel, keyboard and text reach the application;
- interleave local input with remote input and confirm both remain functional;
- disconnect viewer and reconnect without restarting the application;
- stop remote access and verify the local app remains usable.

## 5. E2 — Embedded C++ / Qt Quick

Repeat the E1 shape using `quick-basic` and additionally record:

- text focus/input behavior;
- local window resize while remote view is active;
- tested display scale/DPR;
- resulting remote framebuffer resize/mapping behavior;
- any selected Quick/RHI backend relevant to the candidate.

Local Quick rendering/input must remain functional while remote view/control is active.

## 6. E3 — Declarative QML

Use `qml-basic` through `import HyRemote`, not a C++ facade substitute.

Required observations:

- normal local Quick/QML rendering and text input before remote connection;
- explicit declarative remote start;
- local behavior continues while viewer is connected;
- QML `connectedClientCount` reflects viewer connect/disconnect rather than lifecycle `Running` alone;
- view-only and explicit-control policies behave consistently with the shared runtime;
- pointer/key/text remote paths reach the QML application when enabled;
- reconnect does not recreate/restart the target application;
- local input remains functional throughout.

## 7. E4 — Transparent QPA existing application

This is the defining V0.0.3.0 physical proof.

The application source/target must remain an ordinary Qt application with no HyRemote application API integration.

### Native baseline

1. deploy/build the ordinary E4 application normally;
2. launch with the reference native platform (`-platform windows` on Windows; `-platform xcb` on Linux);
3. verify local display, pointer, keyboard, text, dialog/secondary-window/menu behavior;
4. record the native baseline before enabling HyRemote.

### QPA view-only coexistence

1. deploy the same ordinary application through the installed HyRemote QPA deployment contract;
2. clear diagnostic/manual plugin-path overrides that are not part of the normal deployed path;
3. launch with `-platform hyremote` and default view-only policy;
4. confirm the application is visibly rendered locally through the native delegate;
5. connect a standard viewer and confirm supported application content is visible remotely;
6. while connected, exercise local pointer/keyboard/text and supported dialog/menu/secondary-window behavior;
7. verify remote input is not accepted by default;
8. open/close/move supported secondary surfaces and confirm the viewer connection/listener is not restarted merely by surface churn;
9. disconnect/reconnect the viewer without restarting the application;
10. confirm local behavior remains normal after viewer disconnect.

### QPA explicit remote control coexistence

1. relaunch the same deployed ordinary application with explicit `hyremote-input=true` and the documented platform parameters;
2. confirm native local display/input still works before viewer connection;
3. connect viewer and verify remote pointer/keyboard/text delivery;
4. interleave local and remote interaction;
5. repeat supported dialog/menu/secondary-window operations;
6. disconnect/reconnect and confirm no application restart;
7. confirm local behavior remains usable after disconnect.

A PASS here means HyRemote is additive to the native delegate for the tested exact environment. Headless/Xvfb evidence alone cannot satisfy this section.

## 8. Slow-viewer / responsiveness observation

For each mode/environment, include a bounded correctness observation that remote activity does not indefinitely stall the local UI/render loop.

The goal is not a throughput benchmark. Acceptable evidence should show that the local application remains responsive while the remote client is deliberately slow or temporarily not consuming updates, using the normal product path and existing backpressure behavior.

Record the exact method used and any observed latency/recovery behavior.

## 9. Disconnect-held-input check

Where remote control is enabled, include an abrupt viewer disconnect while a pointer button or modifier/key is held, then verify:

- the target application does not retain a stuck remote button/key/modifier state;
- local input remains usable immediately after disconnect;
- a subsequently connected viewer begins from a clean input state.

This physical observation complements, but does not replace, the automated #90 transport/product-fit gate.

## 10. Evidence result rules

Each matrix cell is one of:

- `PASS` — all mandatory observations for that exact environment completed successfully;
- `FAIL` — a mandatory observation failed;
- `BLOCKED` — execution could not reach the observation for an identified external/environment reason;
- `NOT RUN` — no evidence exists.

Do not use `PASS` for a partial run. Do not convert `BLOCKED` or `NOT RUN` into `Supported` compatibility status.

Attach logs and, where useful, screenshots/video showing simultaneous local and remote state. Visual evidence supplements commands/logs; it does not replace the environment record.

## 11. Release relationship

Physical evidence is consumed by milestone release authorities:

- `v0.0.1.0` / #30 — applicable E1/E2 local-behavior evidence on both reference OSes;
- `v0.0.2.0` / #31 — E3 declarative parity evidence after #30;
- `v0.0.3.0` / #32 — full E4 defining QPA proof on both reference OSes;
- `v1.0.0.0` / #33 — accepted cross-mode evidence envelope.

Per #95, physical evidence alone never creates a release/tag. The milestone release branch must also pass its full automated/package/documentation gate, merge to `main`, and only then receive the annotated milestone tag on the exact accepted `main` release HEAD.
