# V0.3.5.0 RC5 `dfbc6cda` — Local Owner physical evidence index

`RC5_SHA=dfbc6cda9a6d407fdc496d2f2e7dca0c15ab2637`
`RC5_TREE=c1506bf1184bfe3b66f611474831c4c89003bc7e`
`ENVIRONMENT=Windows 10.0.26200 x64 / Qt 6.8.3 msvc2022_64 / MSVC 14.44.35207 / Ninja Release`
`VIEWER=RealVNC Viewer 7.15.1 (r18), same host`
`HOSTED_L2=41569668963 (head_sha=dfbc6cda…, success: Qt6 79/79 in 117.96 s, Qt5 56/56 in 47.56 s)`
`LOCAL_G6=FAIL (60/79 passed, 19 failed, reproducible twice; all 19 pass in isolation and in sub-ranges)`

Evidence-only branch. Never merged. Catalogue: no operator watermark text is written here; texts are cited as code points.

## Files

| File | Observation |
| --- | --- |
| `realvnc-generic-cjk-space.png` | Real Viewer, Generic route, CJK text with an internal space (`U+6301 U+4E45 U+0020 U+5019 U+9009`) persisted byte-exactly |
| `realvnc-qpa-cjk.png` | Real Viewer, QPA route, CJK text (`U+6C34 U+5370 U+5019 U+9009`) |
| `realvnc-explicit-off.png` | Real Viewer after an explicit off switch removed the stored policy |
| `realvnc-qml-omitted-restores.png` | QML instance that says nothing about watermarks restores the persisted mark |
| `realvnc-qml-explicit-empty.png` | QML instance with a declarative empty string: no mark, stored policy removed |
| `qpa-delegate-windows-frame.png` | QPA explicit `delegate=windows` — trampoline reports the native `windows` delegate and a frame is served |
| `qml-running-rejected-keeps-mark-a.png` | Running-phase edit refused; live frame still the original mark |
| `qml-restarted-mark-b.png` | After stop → apply → restart the frame, getters and policy agree on the new mark |
| `probe793-793-store-switching.txt` | Installed C++ API probe output for descriptor switching (`SUMMARY failures=0`) |

## #793 probe facts (`probe793`, installed prefix, C++ API)

- fresh instance, no descriptor: effective watermark disabled (`text=''`)
- select A → `MARK-A`; start/stop; select B → `MARK-B`; back to A → `MARK-A`
- reverse on a second instance: B → `MARK-B`, then A → `MARK-A`
- A → `MARK-A`, then a descriptor whose store has no policy → disabled again (no cross-store carry)
- explicit request `EXPLICIT-MARK` overrides the restored A mark, persists only into the newly selected store, leaves B untouched, and the 927-byte diagnostic report does not contain the operator text
- `HYREMOTE_STATE_DIR` set explicitly outranks the descriptor-relative store (`MARK-ENV` wins over A)
