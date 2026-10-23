# Windows maintained-viewer cursor visual cell (2026-10-23)

## Scope

This is the independent visual cursor-shape check for #175 on Windows x86_64. The host changes its
Qt cursor between IBeam and Cross; the viewer is expected to reflect the remote shape.

## Baseline

```text
7d2307f929d787c98ce102d6b44376b9ac46cc7d
Windows 11 25H2 x86_64
Qt 6.8.3 msvc2022_64
TightVNC 2.8.85
RealVNC 7.15.1
```

## Method

1. Host toggles its QWidget cursor between `Qt::IBeamCursor` and `Qt::CrossCursor` every 3 s.
2. A Python `ctypes` probe brings the viewer forward, places the pointer over the viewer content,
   and samples `GetCursorInfo().hCursor` for 10 s.
3. A single handle across the host's IBeam/Cross transitions means the viewer did not reflect the
   remote cursor shape.

## Result

```text
Host log:        CURSOR CROSS / CURSOR IBEAM observed
TightVNC:        one cursor handle only (28454264)
RealVNC:         one cursor handle only (65539)
```

Both viewers kept a local cursor handle constant across host shape changes. The wire-level cursor
pseudo-encoding tests pass separately, but the maintained viewers did not expose a visual shape
change through `GetCursorInfo`.

## Interpretation

```text
Host cursor pseudo-encoding: PASS (wire tests)
Maintained-viewer visual cursor shape: NOT OBSERVED with these viewers
```

This is a viewer-side limitation/observation, not proof that the host sends no cursor update. It may
also reflect the viewer using a local arrow rather than the remote cursor.

## Owner disposition

For V0.3.3.0, the Owner accepts this as a documented viewer-local cursor limitation:

- pointer position, buttons, wheel and remote input are qualified separately;
- the host still emits the cursor pseudo-encoding and its wire tests PASS;
- visual cursor shape is not guaranteed for the maintained viewers in this matrix.

No visual cursor-shape PASS is claimed. A future viewer qualification can replace this accepted
limitation with a visual PASS.

## Evidence

```text
F:/workspace/hyremote/HyRemote-evidence/2026-10-23-cursor-visual/
├── summary.txt
├── cursor-result.json
└── host.log
```
