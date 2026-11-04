# release/v0.3.5.0 `b0680d5d` — Local Owner acceptance and #609 baseline evidence

`SHA=b0680d5df4f76d8b84b02a1b950030a65a89ddca` · `TREE=141d07a897f838a6ade5e9e4854f88edf022717a`
`ENVIRONMENT=Windows 11 26200, Qt 6.8.3 msvc2022_64, MSVC 14.44.35207, Ninja Release`
`HOST=i7-13700K (16C/24T), 31.7 GB RAM, HP E24 G4 1920x1080 @ 100% scale`
`VIEWER=RealVNC Viewer 7.15.1 (r18), same host`

## A. Acceptance on the new SHA

```text
build.cmd install --config=build/local-qt683.yml   -> SOURCE_SHA=b0680d5d…, Qt 6.8.3, Release
build.cmd test    --config=build/local-qt683.yml   -> 79/79 passed, 432.22 s, EXECUTED_TEST_COUNT=79
Hosted L2 41779002386 (release-readiness, expected_sha=b0680d5d…) -> Qt6 79/79 (162.35 s), Qt5 56/56 (47.96 s), governance success
```

`generic-watermark-viewer.png` / `qpa-watermark-viewer.png`: the CJK watermark
(`U+6301 U+4E45 U+0020 U+5019 U+9009`) renders bottom-right in the maintained Viewer on both routes; the persisted
policy is exactly those five code units (the space survives).

`corrupt-store-fail-closed.txt`: four corrupted-store probes — malformed key/value and out-of-bounds
(130 code units, opacity 9.9) on Generic, malformed on QPA all end in `STATE=Stopped`,
`LAST_ERROR_CODE=InvalidConfiguration`, a message ending in *"remote output is refused instead of streaming
unwatermarked frames"*, and **no listener**; the explicit `watermark-state=off` launch is the only one that runs,
with the corrupted store removed.

`viewer-reconnect.png`: after closing the Viewer for 5 s the host stayed alive with `0.0.0.0:5921` still listening,
and the reconnected Viewer received a frame **pixel identical** to the pre-disconnect frame (changed fraction 0.000000).

## B. #609 1080p baseline (B0/B1/B2, static scene, 40 samples × 1 s after 10 s warm-up)

Harness (attached for reproducibility): `harness-widgets1080.cpp` (frameless 1920×1080 QWidget scene, no HyRemote
code, Generic plugin via `-plugin`) and `harness-quick1080.qml` (frameless 1920×1080 ApplicationWindow, HyRemote QML
module; `--no-remote` disables it for B0). Both report a **1920×1080** client and, where remote was enabled, a
**1920×1080** RFB frame — verified by capture, not assumed.

| Case | Host CPU (% of one core) | WS mean (MB) | WS max (MB) | Private mean (MB) | Private max (MB) | Viewer CPU | Viewer WS |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Widgets B0 (no HyRemote) | 0.0 (0.000 s) | 80.4 | 80.4 | 46.8 | 46.8 | — | — |
| Widgets B1 (zero viewers) | 2.1 | 93.1 | 93.1 | 55.4 | 55.4 | — | — |
| Widgets B2 (one static viewer) | 7.5 | 93.7 | 100.5 | 56.1 | 70.6 | 0.0 | 47.4 |
| Quick B0 (no HyRemote) | 0.2 | 179.1 | 179.1 | 141.3 | 141.3 | — | — |
| Quick B1 (zero viewers) | 0.1 | 203.6 | 203.6 | 165.2 | 165.2 | — | — |
| Quick B2 (one static viewer) | 9.8 | 214.1 | 228.6 | 186.4 | 201.0 | 0.0 | 90.5 |

Raw per-sample series: `baseline-{w,q}-{b0,b1,b2}.json` (each holds the full 40-sample working-set/private series in
the CSV written beside it on the host; the JSON summarises mean/max and the exact command line).

Discarded, not part of the baseline: three earlier attempts that used the shipped 460×190 learning example resized at
runtime — the RFB framebuffer stayed 460×190, so they are not 1080p and are excluded.

## C. Limits

Single run per case on one machine, no GPU counters, no frame-age/FPS instrumentation, no second viewer (B5) and no
interactive-load cases (B3/B4) yet. Viewer CPU is sampled as a whole process, so a 0.0 % value only means "below one
sampling quantum over 41 s" for a static scene.
