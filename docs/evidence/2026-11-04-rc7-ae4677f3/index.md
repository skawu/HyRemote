# V0.3.5.0 RC7 `ae4677f3` — Local Owner physical evidence

`RC7_SHA=ae4677f321b140050813d35479639c302f710c2e` · `RC7_TREE=6e5a929d688e22de1a45770019416201a66d3162`
`ENVIRONMENT=Windows 11 25H2 x64, Qt 6.8.3 msvc2022_64, MSVC 14.44.35207, Ninja Release`
`VIEWER=RealVNC Viewer 7.15.1 (r18), same host`
`SYSTEM_DPI=96 (100% display scale — applied DPI registry value 96)`

## How the two DPI conditions were produced

| Condition | Construction | Honest label |
| --- | --- | --- |
| 100% | the session itself (system DPI 96) | genuine system scale |
| 200% | `QT_SCALE_FACTOR=2` on the host process | **forced device-pixel-ratio 2**, not a system display-scale change |

Changing the real Windows display scale to 200% needs a logoff/logon cycle and could not be constructed from this
process, so it is recorded as **NOT_RUN**. The forced case is verifiable: a 460x190 logical host window became
936x419 OS pixels (≈920x380 client), i.e. DPR 2.

## Local G6 (install first, install tree kept in place)

```text
build.cmd install --config=build/local-qt683.yml  -> HYREMOTE-MANIFEST SOURCE_SHA=ae4677f3…, Qt 6.8.3, Release
build.cmd test    --config=build/local-qt683.yml  -> 100% tests passed, 0 failed out of 79, 446.81 s,
                                                     EXECUTED_TEST_COUNT=79
```

## Watermark size at both scales (measured on the delivered RFB frame)

Ink measured as red-dominant pixels inside the requested corner. Margin rule is `max(8, 2%·min-side)`.

| Route | DPI | Frame size | Ink bbox | Ink size | Edge gaps (right/bottom) |
| --- | --- | --- | --- | --- | --- |
| Generic | 100% | 460x190 | 362,159-451,179 | **90x21** | 8 / 10 |
| Generic | 200% | 460x190 | 362,159-451,179 | **90x21** | 8 / 10 |
| QPA | 100% | 860x560 | 758,526-847,546 | **90x21** | 12 / 13 |
| QPA | 200% | 860x531 | 759,497-848,517 | **90x21** | 11 / 13 |

Observations: the watermark keeps the same logical size and the same reserved margins on both routes and both
scales — it is neither half-size nor clipped. The RFB frame stays in logical pixels (Generic 460x190; QPA 860x560).
At 200% the QPA frame is 860x531 because the 2x window (1120 device px tall) exceeds the 1080-px screen; the
watermark still sits inside its margin.

Files: `frame-generic-100.png`, `frame-generic-200.png`, `frame-qpa-100.png`, `frame-qpa-200.png`.

## Maintained Viewer screenshots

`viewer-generic-100.png`, `viewer-generic-200.png`, `viewer-qpa-100.png`, `viewer-qpa-200.png` — the mark is
displayed bottom-right in all four, normal size, no clipping.

## Oversized watermark fails closed (120 code units, bottom-right)

| Route/DPI | STATE | LAST_ERROR | Listener | Client |
| --- | --- | --- | --- | --- |
| generic/100 | `Faulted` | `RuntimeFailure` — `the watermark text does not fit a 460x190 frame at 16 points` | closed | refused |
| generic/200 | `Faulted` | `RuntimeFailure` — `… does not fit a 460x190 frame at 16 points` | closed | refused |
| qpa/100 | `Faulted` | `RuntimeFailure` — `… does not fit a 860x560 frame at 16 points` | closed | refused |
| qpa/200 | `Faulted` | `RuntimeFailure` — `… does not fit a 860x531 frame at 16 points` | closed | refused |

No unwatermarked fallback frame was served in any of the four cases.

## Static scene, input and reconnect (1280x720 Qt Quick host, installed RC7 module)

| File | Observation |
| --- | --- |
| `dynamic-initial.png` | First frame with the watermark |
| `dynamic-idle-25s.png` | After 25 s with no host change: **pixel identical** to the initial frame (changed fraction 0.000000) |
| `dynamic-input.png` | Click injected through the Viewer → host `clicks=1`, Viewer shows `CLICKS=1`; keystroke → `keys=1` |
| `dynamic-reconnect.png` | Viewer closed 5 s, host alive, listener up; reconnect delivers the current frame (`CLICKS=1 KEYS=1`) with the watermark intact |
