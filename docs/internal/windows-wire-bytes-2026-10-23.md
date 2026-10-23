# Windows maintained-viewer wire bytes (2026-10-23)

## Scope

This records a real-viewer wire-byte sample for #370 on Windows x86_64 through a local loopback TCP
proxy. It is not a WAN or release-SLO claim.

## Baseline

```text
2df9a552ee4e6ce47546aa4709c80b5e9546b710
Windows 11 25H2 x86_64
Qt 6.8.3 msvc2022_64
TightVNC 2.8.85
480x240 Widgets panel toggled at 10 Hz
```

## Method

A Python TCP proxy listened on 5936 and forwarded to the host on 5935, counting bytes in each
direction while the viewer decoded a 10 s animated stream. Viewer CPU/RSS were sampled with
`psutil`.

## Result

```text
TightVNC 2.8.85
bytes upstream:   1,392
bytes downstream: 59,446,398
bytes total:      59,447,790 in 10 s (~5.9 MB/s)
viewer CPU avg:   1.09%
viewer RSS avg:   16.0 MiB
```

RealVNC 7.15.1 through the same proxy produced only 31 bytes and no frame flow; that capture is
invalid, so no RealVNC wire-byte number is claimed.

## Interpretation

The current TightVNC path carries about 5.9 MB/s for a 480x240 target changing at 10 Hz. This is
consistent with a Raw fallback/full-frame path rather than an efficiently compressed stream and is
a material #370 finding, not a release-quality wire profile.

## Boundaries

- Local loopback only; one viewer.
- TightVNC path; RealVNC proxy capture invalid.
- No V0.3.0.0 comparison, no WAN, no release SLO.
- The high byte count is recorded as a risk; no cache/encoding optimization is claimed here.

## Evidence

```text
F:/workspace/hyremote/HyRemote-evidence/2026-10-23-wire-bytes/
├── summary.txt
├── wire-tight.json
└── wire-real.json
```

## Verdict

```text
TightVNC wire bytes measured; current profile is high (~5.9 MB/s)
#370 remained PARTIAL; compression/bandwidth work and V0.3.0.0 comparison remain
```
