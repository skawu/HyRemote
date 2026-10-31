# #589 W2 real-viewer acceptance — 2026-10-31

Environment: Windows x86_64, Qt 6.8.3 `msvc2022_64`, MSVC 14.44.35207, Release, deployed prefix built at
`60d8f5a508e5257c4fbbd1fcfd6e76ec4f43cdd6` (contains W2 code merged via #726–#729). Viewer: `vncdotool` 3.x
(`-s 127.0.0.1::5921 capture`), the same standard viewer family the candidate product-fit uses.

## 1. Default policy (enabled, no explicit position)

`-plugin hyremote:watermark=CONFIDENTIAL` → remote frame: single line, bottom-right, white, 0.35 opacity.

- `wm-control.png` — same app with no watermark (`-plugin hyremote:port=5921`).
- `wm-default.png` — watermark enabled with defaults.
- Pixel diff (every 2nd pixel, channel sum > 6): bounding box **(289,160)-(452,178)**; changed samples per quadrant
  TL 0 / TR 0 / BL 0 / **BR 319**. The default changes only the bottom-right region.

## 2. Explicit values win over defaults

`-plugin hyremote:watermark=CONFIDENTIAL;watermark-position=tiled;watermark-color=#000000;watermark-opacity=0.5`

- `wm-explicit-tiled.png` — tiled, black, 0.5.
- Diff vs control: bounding box **(0,16)-(423,164)**; samples TL 795 / TR 795 / BL 547 / BR 545 — all four quadrants.

## 3. Disable restores the original picture

The control capture above is the same application with the watermark absent; it differs from the watermarked frames
only inside the regions listed above, i.e. nothing outside the watermark is altered.

## 4. Multiple viewers

One application, watermark on, two viewers connected concurrently, then a third connection after both closed:

| Capture | SHA-256 |
| --- | --- |
| `wm-viewerA.png` (concurrent) | `1D60F23E428BCF4908A7687D462A2D8D0F6A0E47BD0939FC876CED2735A76273` |
| `wm-viewerB.png` (concurrent) | `1D60F23E428BCF4908A7687D462A2D8D0F6A0E47BD0939FC876CED2735A76273` |
| `wm-viewerC.png` (after both closed) | `1D60F23E428BCF4908A7687D462A2D8D0F6A0E47BD0939FC876CED2735A76273` |

Identical bytes: viewers share one composed frame, and closing them leaves no watermark residue.

## 5. QPA route

`-platform hyremote:hyremote-watermark=QPA-WATERMARK` → `wm-qpa.png`; the mark is visible at the bottom-right of the
remote picture and the native console content is otherwise unchanged.

## 6. Cost

| Configuration | Window with no viewer, 10 s | Forced-repaint storm (12 window resizes, 1 viewer), CPU |
| --- | --- | --- |
| watermark disabled | 15.6 ms | 234.4 ms / 531.2 ms |
| watermark enabled | 0.0 ms | 718.8 ms / 890.6 ms |

Reading: with no viewer there is no per-frame work in either configuration (frames are demand-driven), so the disabled
path costs nothing extra — structurally it installs no decorator and copies nothing. Under forced full-frame production
the enabled path roughly doubles process CPU, which is consistent with one full-frame copy plus a text paint per frame.

Honest limit: this is an indicative external measurement (two samples, ±2x variance), not a throughput benchmark. A
rigorous per-frame number needs a continuously animating target or a compositor micro-benchmark, which this repository
does not have yet — the examples are static. It is **not** reported as a measured frame rate.

## 7. Still not covered by this evidence

- Raw vs TRLE wire-level assertion that the *encoded* rectangles carry watermarked pixels (the decorator runs before
  encoding, and unit tests cover the composed buffer, but no wire-level test decodes a TRLE rect and asserts the mark).
- Incremental damage correctness with the mark, and mid-session framebuffer resize/DPR change, at wire level.
- Viewer-side visual check on a second machine (all captures here are viewer framebuffers on the same host).
