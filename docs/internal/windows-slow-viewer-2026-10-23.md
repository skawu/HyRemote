# Windows maintained-viewer slow/paused cell (2026-10-23)

## Scope

This is the #144 bounded slow-viewer cell on Windows x86_64. It uses two real TightVNC viewers and
a deterministic final state. It is local loopback and simulates a paused viewer by suspending its
process; it is not a WAN or release-SLO claim.

## Baseline

```text
7c3fe75b29e63f1cea3a56be1af9580eea741364
Windows 11 25H2 x86_64
Qt 6.8.3 msvc2022_64
HyRemote RemoteAccess 480x240 Widgets panel
60 FPS capture pacing
TightVNC 2.8.85
```

## Method

1. Host starts black; both viewers connect.
2. Capture both viewer pixels before suspension.
3. Suspend the slow Viewer with `NtSuspendProcess`.
4. Host switches to its final white state.
5. Capture only the fast Viewer while the slow Viewer is suspended. **No `PrintWindow` is called
   on a suspended process**, because that call blocks indefinitely.
6. Resume the slow Viewer and capture both viewers again.

## Result

```text
Before suspend:  fast=black   slow=black   slow Responding=false after suspend
Fast at final:   white
After resume:    fast=white   slow=white
```

Both viewers reached the final state; the fast viewer was not stalled by the suspended viewer.
The slow viewer caught up after resume.

## Boundaries

- Process suspension simulates a paused/backlogged viewer; it is not a real slow network.
- It does not measure sustained viewer CPU/RSS or wire bytes.
- It does not claim Linux/embedded behavior.
- The safe harness deliberately avoids capturing a suspended UI process.

## Evidence

```text
F:/workspace/hyremote/HyRemote-evidence/2026-10-23-slow-viewer/
├── summary.txt
├── steps.txt
└── host3.log
```

## Verdict

```text
#144 maintained-viewer slow/paused cell: PASS for Windows
#370 sustained resources and V0.3.0.0 comparison remain open
```
