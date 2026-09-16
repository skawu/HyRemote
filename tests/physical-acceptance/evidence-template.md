# HyRemote physical acceptance evidence record

Copy this file for each executed OS/candidate evidence set. Do not overwrite prior accepted evidence.

## Candidate identity

- Milestone issue/version:
- Candidate branch:
- Candidate commit SHA:
- Date/time/timezone:
- Executor / physical host identity:

## Environment

- OS edition/distribution:
- OS build/version:
- Architecture:
- Compiler/toolchain:
- Qt version:
- Qt installation/package identity:
- CPU:
- GPU / driver:
- Physical display(s), resolution, scaling/DPR:
- Native Qt platform plugin:
- Quick/RHI backend where relevant:
- HyRemote build type/options:
- Installed SDK prefix:
- Viewer product/version:
- Viewer host/network path:

## Preparation

- Configure command:
- Build command:
- Test command/result:
- Install/deploy command:
- Environment/path/plugin-path cleanup performed:
- Security/network boundary used for this test:

## Matrix

| Cell | Local visible | Local pointer/key/text | Remote view | View-only isolation | Remote input | Reconnect | Surface/resize behavior | Slow-viewer responsiveness | Abrupt-held-input reset | Result |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| E1 Widgets |  |  |  |  |  |  |  |  |  | NOT RUN |
| E2 Quick |  |  |  |  |  |  |  |  |  | NOT RUN |
| E3 QML |  |  |  |  |  |  |  |  |  | NOT RUN |
| E4 QPA native baseline |  |  | n/a | n/a | n/a | n/a |  | n/a | n/a | NOT RUN |
| E4 QPA view-only |  |  |  |  | n/a |  |  |  | n/a | NOT RUN |
| E4 QPA control |  |  |  | n/a |  |  |  |  |  | NOT RUN |

Use only `PASS`, `FAIL`, `BLOCKED`, or `NOT RUN` as final cell states. A partial observation is not PASS.

## Detailed execution notes

### E1 Widgets

- Exact launch/configuration:
- Local-before-remote observations:
- Viewer connect/view observations:
- Local-while-viewer-connected observations:
- View-only observation:
- Explicit remote-control observation:
- Reconnect/stop observation:
- Anomalies:
- Result:

### E2 Quick

- Exact launch/configuration:
- Local-before-remote observations:
- Text focus/input:
- Resize/DPR/backend behavior:
- Viewer/view-only/control/reconnect observations:
- Anomalies:
- Result:

### E3 QML

- Exact launch/configuration:
- Local-before-remote observations:
- `connectedClientCount` lifecycle observed:
- View-only/control/reconnect observations:
- Anomalies:
- Result:

### E4 Transparent QPA — native baseline

- Exact native launch:
- Local display/input/dialog/menu/window observations:
- Result:

### E4 Transparent QPA — view-only

- Exact deployed launch:
- Confirmation no manual/debug `QT_PLUGIN_PATH` dependency:
- Native local display/input while viewer connected:
- Remote framebuffer:
- Remote input rejected by default:
- Dialog/menu/secondary-window continuity:
- Disconnect/reconnect:
- Result:

### E4 Transparent QPA — explicit control

- Exact deployed launch including `hyremote-input=true`:
- Native local display/input while viewer connected:
- Remote pointer/keyboard/text:
- Interleaved local+remote interaction:
- Surface churn/reconnect:
- Result:

## Responsiveness / backpressure

- Method used to create slow/non-consuming viewer condition:
- Duration:
- Local UI/render response during condition:
- Recovery after condition:
- Result:

## Abrupt held-input disconnect

- Mode/cell:
- Held input before abrupt disconnect:
- Disconnect method:
- Target state immediately after disconnect:
- Local input immediately after disconnect:
- Subsequent viewer clean-state result:
- Result:

## Attachments

- Logs:
- Screenshots:
- Video:
- Other diagnostics:

## Final result for this OS/candidate

- Overall: PASS / FAIL / BLOCKED / NOT RUN
- Blocking defects/issues:
- Known limitations observed:
- Compatibility/support document updates required:
- Milestone issue comment/evidence link:

No release or tag is authorized by this record alone. The milestone authority and Git Flow release gate remain controlling.
