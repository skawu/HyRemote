# V0.3.5.0 RC6 `916c4916` — Local Owner physical evidence

`RC6_SHA=916c4916153d8b27ba6fdd957003e45d9c20866a` · `RC6_TREE=1a971e0960d69afc14fae9e2ef3e418abf0c9176`
`ENVIRONMENT=Windows 11 25H2 x64, Qt 6.8.3 msvc2022_64, MSVC 14.44.35207, Ninja Release`
`VIEWER=RealVNC Viewer 7.15.1 (r18), same host`

## Local G6 — install first, install tree kept in place

```text
build.cmd install --config=build/local-qt683.yml   -> HYREMOTE-MANIFEST SOURCE_SHA=916c4916…, Qt 6.8.3, Release
build.cmd test    --config=build/local-qt683.yml   -> 100% tests passed, 0 failed out of 79, 474.56 s,
                                                      EXECUTED_TEST_COUNT=79
```

## RealVNC physical checks

Host: 1280×720 Qt Quick window on the installed RC6 QML module, `remoteInputEnabled: true`, port 5921,
watermark at bottom-right. Stored policy is 9 code units:
`U+0052 U+0043 U+0036 U+0020 U+6C34 U+5370 U+0020 U+9A8C U+6536`.

| File | Observation |
| --- | --- |
| `t1-watermark.png` | Watermark rendered in the maintained Viewer; viewer title shows `(HyRemote)` (input enabled) |
| `t2-idle.png` | After 25 s with no host change the Viewer frame is **pixel identical** to `t1` (changed fraction 0.000000) — no drift, no black frame |
| `t3-click.png` | A click injected into the Viewer window reaches the host: host window title went `clicks=1`, and `CLICKS=1` is visible in the Viewer |
| `t3b-key.png` | A keystroke to the Viewer reaches the host: `keys=1`, `KEYS=1` visible in the Viewer |
| `t4-reconnect.png` | After closing the Viewer for 5 s and reconnecting, the listener is still up and the Viewer receives the current frame (`CLICKS=1 KEYS=1`) with the watermark intact |

`t2` is the static-scene wake-up evidence: the picture stays correct through a long idle, and the first change
after that idle (the click) reaches the Viewer immediately rather than leaving a stale or partially painted frame.
