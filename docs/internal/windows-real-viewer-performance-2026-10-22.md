# Windows real-viewer input-to-visible evidence (2026-10-22)

## Scope

This is the Windows x86_64 real-viewer cell for #370. It measures **input-to-visible** latency, not frame age or viewer CPU/RSS. It is local-loopback and indicative; it is not a release SLO.

Baseline:

```text
develop 5b98613
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

## Results

```text
TightVNC 2.8.85:  20/20  min 16.36  p50 17.00  p95 28.36  max 51.04 ms
RealVNC 7.15.1:   20/20  min 14.98  p50 17.20  p95 50.10  max 50.36 ms
```

Both viewers reached the visible toggle on every sample. The p50 is at the capture polling floor; the RealVNC p95 includes occasional multi-frame update delays.

## Boundaries

- This is **input-to-visible**, not frame age, frame freshness, viewer CPU/RSS or wire bytes.
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
#370 complete envelope: PARTIAL (frame-age and viewer resources still open)
```
