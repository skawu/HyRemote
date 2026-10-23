# Windows sustained Viewer resource cell (2026-10-23)

## Scope

This records sustained Viewer CPU/RSS for #370 on Windows x86_64 while a 480x240 Widgets panel
animates at 10 Hz. It is local loopback and one Viewer; it is not a WAN or release-SLO claim.

## Baseline

```text
1357dfcddd39a42758e9ed8d3f16da7682fe1ec3
Windows 11 25H2 x86_64
Qt 6.8.3 msvc2022_64
TightVNC 2.8.85
60 FPS capture pacing
```

## Method

A Python harness using `psutil` started the HyRemote host and TightVNC Viewer, waited for the
connection, then sampled both processes every 0.5 s for 10 s:

```text
viewer CPU = process user+system delta / wall time
viewer RSS = process working set
```

## Result

```text
samples: 20
Viewer CPU avg: 0.31% of one core
Viewer CPU max: 3.12%
Viewer RSS avg: 58.6 MiB
Viewer RSS max: 58.7 MiB
Host CPU avg:   4.21%
Host RSS avg:   75.7 MiB
```

The Viewer remains lightweight under the 10 Hz animated 480x240 target.

## Boundaries

- One TightVNC Viewer, local loopback, 480x240.
- No wire-byte capture, frame-age, or V0.3.0.0 comparison in this cell.
- Not a WAN or release-SLO claim.
- RealVNC sustained resources remain unmeasured.

## Evidence

```text
F:/workspace/hyremote/HyRemote-evidence/2026-10-23-sustained-resources/
├── summary.txt
├── resource-result.json
└── resource_probe.py
```

## Verdict

```text
TightVNC sustained Viewer CPU/RSS: PASS for this cell
#370 complete envelope: PARTIAL (wire bytes, RealVNC resource sample and V0.3.0.0 comparison remain)
```
