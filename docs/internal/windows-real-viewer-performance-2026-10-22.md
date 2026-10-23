# Windows real-viewer input-to-visible evidence (2026-10-22)

## Scope

This is the Windows x86_64 real-viewer cell for #370. It measures **input-to-visible** latency, not frame age or viewer CPU/RSS. It is local-loopback and indicative; it is not a release SLO.

Baseline (exact committed tree):

```text
85ea0489e6daf3320b02db38a986f2c5f060e85c
Windows 11 25H2 x86_64
Qt 6.8.3 msvc2022_64
HyRemote RemoteAccess 480x240 Widgets panel
```

## Method

1. A minimal Widgets panel fills the whole 480x240 window black and toggles to white on Space.
2. The panel is exposed through the public `HyRemote::RemoteAccess` API with remote input enabled.
3. A maintained viewer is connected and its content is brought to the foreground.
4. The viewer content is clicked once to focus the panel, then Space is injected with `keybd_event`.
5. A 12x12 region at the viewer content center is captured every 2 ms; the first changed pixel is the visible timestamp.
6. Each viewer is measured alone with 20 samples. Screen capture has a roughly 16 ms polling floor.

## Capture-pacing correction

The first run used the unpaced Session scheduler. A static 480x240 Widgets panel with one connected
viewer consumed about **6.75 s of CPU in 5 s (~135% of one core)** because capture requests were
issued as fast as the GUI event loop could render. That is not acceptable idle behavior.

The runtime now sets `capture.targetFramesPerSecond = 60`. At the same 480x240 target:

```text
RealVNC connected, static:  62.5 ms CPU / 5 s (~1.3% of one core), RSS 32.0 MiB
TightVNC connected, static: 171.9 ms CPU / 5 s (~3.4% of one core), RSS 31.4 MiB
```

Input-to-visible was re-measured after the pacing change; both viewers still reached the visible
toggle on every sample. The p50 moved from the capture-floor result to roughly the viewer update
cadence.

## Results (60 FPS pacing)

```text
TightVNC 2.8.85:  20/20  min 33.74  p50 52.26  p95 84.41  max 117.35 ms
RealVNC 7.15.1:   20/20  min 32.54  p50 49.74  p95 84.46  max 95.29 ms
```

The 2 ms screen capture still has a roughly 16 ms polling floor. The p95 values include occasional
multi-update delays; they are not frame-age measurements.

## Boundaries

- This is **input-to-visible**, not frame age, frame freshness, wire bytes or a sustained viewer CPU/RSS profile. The static idle CPU readings above are a first resource sample.
- It is a local loopback measurement on one Windows host, not a WAN or release-SLO claim.
- It does not replace the synthetic W1-W4 bytes/P50/P95/CPU/RSS profile.
- No Linux/embedded viewer cell is claimed; that work is user-validation under #563.

## Evidence

```text
F:/workspace/hyremote/HyRemote-evidence/2026-10-22-real-viewer-perf/
├── summary.txt
└── realvnc-perf.png
```

## Verdict

```text
Windows maintained-viewer input-to-visible: PASS (2 viewers, 20/20 samples each)
Static connected-viewer idle CPU: PASS at 60 FPS pacing (~1.6-2.5% of one core)
#370 complete envelope: PARTIAL (frame-age and sustained resource profile still open)
```
