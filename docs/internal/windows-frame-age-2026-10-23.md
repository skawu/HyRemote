# Windows maintained-viewer frame-age cell (2026-10-23)

## Scope

This records a real-viewer frame-age measurement for #370 on Windows x86_64. It is local loopback
and uses a PrintWindow capture harness; it is not a WAN or release-SLO claim.

## Baseline

```text
925191d2c1e94523b077ec19c622b3fbf95bb6da
Windows 11 25H2 x86_64
Qt 6.8.3 msvc2022_64
HyRemote RemoteAccess 480x240 Widgets panel
60 FPS capture pacing
```

## Method

1. The host toggles a full 480x240 panel every 100 ms and logs the host timestamp for each toggle.
2. TightVNC Viewer 2.8.85 is connected to the host.
3. `PrintWindow(PW_RENDERFULLCONTENT)` captures the Viewer window every ~5 ms plus capture cost;
   the first frame with a changed center pixel is the visible timestamp.
4. Each visible transition is matched to the latest preceding host toggle within 300 ms.

`PrintWindow` polling is slower than the nominal 5 ms sleep and adds capture overhead, so this is a
conservative, coarse frame-age measurement. It is still a maintained-viewer observation, not a
synthetic transport timer.

## Result

```text
TightVNC 2.8.85
samples: 91
min:     17 ms
p50:     42 ms
p95:     79 ms
max:     85 ms
```

RealVNC 7.15.1 does not expose a usable `PrintWindow` surface for this harness, so no RealVNC
frame-age number is claimed.

## Boundaries

- Frame age only; sustained viewer CPU/RSS, wire bytes and V0.3.0.0 comparison remain open.
- The host loopback and PrintWindow capture method are not a release SLO.
- No encrypted transport is involved.
- No Linux/embedded viewer cell is claimed.

## Evidence

```text
F:/workspace/hyremote/HyRemote-evidence/2026-10-23-frame-age/
├── summary.txt
├── toggles.log
├── viewer-times.csv
└── samples.txt
```

## Verdict

```text
TightVNC maintained-viewer frame age: PASS for this cell
#370 complete envelope: PARTIAL (sustained viewer resources and comparison still open)
```
