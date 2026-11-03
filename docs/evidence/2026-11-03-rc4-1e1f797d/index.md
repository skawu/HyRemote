# V0.3.5.0 RC4 `1e1f797d` — Local Owner physical evidence index

`RC4_SHA=1e1f797dfb1ba41f28dc5cac765bc746ca2ab6db`
`RC4_TREE=d52b438985c1870e0c599cfeb5bb6dc37f40105b`
`RELEASE_BRANCH=release/v0.3.5.0` (read-only during this gate)
`ENVIRONMENT=Windows 10.0.26200 x64 / Qt 6.8.3 msvc2022_64 / MSVC 14.44 / Ninja Release`
`MAINTAINED_VIEWER=RealVNC Viewer 7.15.1 (r18)`

This folder holds only observed artefacts. It is an evidence branch, not product source, and is never merged.

## Files

| File | Observation |
| --- | --- |
| `qml-a1-omitted-restores.png` | Fresh QML instance with the watermark property **omitted** restored the persisted mark (bottom-right) |
| `qml-a2-explicit-empty.png` | Fresh QML instance with declarative `watermarkText: ""` — no mark, `watermark.policy` deleted |
| `qml-a2-realvnc-explicit-off.png` | Same explicit-OFF instance seen through the real RealVNC 7.15.1 window |
| `qml-a2b-programmatic-empty.png` | Programmatic `watermarkText = ""` before start — no mark, stored policy removed |
| `qml-a3-restart-stays-off.png` | Restart that says nothing about watermarks keeps the explicit OFF |
| `qml-b-running-mark-a.png` | Running instance with MARK-A at bottom-right |
| `qml-b-rejected-mark-a-kept.png` | After a Running-phase text/position change was refused: image still MARK-A at bottom-right |
| `qml-b-restarted-mark-b.png` | Stop → apply MARK-B + top-left → restart: image, getters and stored policy agree |
| `c2-1080p-corner.png` | Ordinary 1920x1061 framebuffer, bottom-right corner marked |
| `c2-1080p-tiled.png` | Ordinary 1920x1061 framebuffer, tiled marks across the whole frame |
| `realvnc-generic-cjk-space.png` | Real RealVNC window, Generic route, CJK text with a space (U+6301 U+4E45 U+0020 U+5019 U+9009) |
| `realvnc-qpa-cjk.png` | Real RealVNC window, QPA route, CJK text (U+6C34 U+5370 U+5019 U+9009) |
| `qpa-delegate-windows-frame.png` | QPA `delegate=windows` — trampoline reports the native `windows` delegate and a frame is produced |

## Measurements

- Watermark ink stays inside the reserved corner margin: 1920x1061 frame, bottom-right ink bbox
  `1756,1019-1896,1034`, edge gaps right 23 px / bottom 26 px against a margin of max(8, 2% of 1061) = 21.2 px.
- 460x190 frame, bottom-right: ink bbox `322,160-452,178`, gaps right 7 px / bottom 11 px.
- Oversized corner text fails closed at all four corners: `LAST_ERROR_CODE=RuntimeFailure`,
  `the watermark text does not fit a 460x190 frame at 16 points`, `STATE=Faulted`, listener closed, no frame sent.
- Tiled 1920x1061 frame: 140,244 ink pixels spanning `1,17-1918,1060`.
