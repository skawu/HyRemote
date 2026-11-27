# #962 Work session reuse evidence

The five files under `raw/` are original bytes from the Work workspace. No benchmark was rerun for this archive. This was a Windows synthetic 384×240 Qt Quick scene, approximately one color transition per second, local loopback, RealVNC Viewer 7.15.1 (r18), Qt 6.8.3 MSVC Release. A1/B/A2 used one research binary, OpenGL in every arm, 250 ms public fallback, and only B enabled the research native provider.

## Source identity

The isolated Runtime checkout's Git HEAD is `732bd66feddfc7c7c1fbebc1c4bab379d9d01391`, with local overlays for #959/#960, the #961 compatibility change and uncommitted diagnostic/credit changes. These results have **no exact historical source commit**, and cannot be assigned to #963. The SHA-256 of the surviving Runtime DLL is `cba370617bc72844e94aa0fe152710ea23a051efb95e592d90f9ef2062b67069`. The host binary was rebuilt after A/B/A to add the stop-during-readback probe. The A/B/A host binary hash and exact pre-rebuild source snapshot are unavailable. Files under `reference/` are the surviving post-lifecycle-test implementation and commands, not an immutable build ID for the earlier run.

The Runtime cache used Ninja Release, Qt 6.8.3 MSVC2022_64, MSVC 14.44, C++ integration, Quick adapter ON, tests OFF, CPU probe OFF. The host was built with `/O2 /std:c++17 /DHYREMOTE_RESEARCH_NATURAL_CREDIT`, linked to the local Runtime import library and Qt Core/Gui/Network/Quick. Runtime flags: `QSG_RHI_BACKEND=opengl`, `HYREMOTE_SYNTHETIC_VIEWER_TRACE=1`, `HYREMOTE_SYNTHETIC_STATIC_125=0`; `HYREMOTE_RESEARCH_NATIVE_GL=0/1/0`. The saved observer was run with `--seconds 23 --graceful-host`; local loopback Viewer flags were `-SecurityNotificationTimeout=0 -WarnUnencrypted=0 -EnableUdpRfb=false`.

## Recomputed from original records

| Metric | A1 public | B reused native + public | A2 public |
| --- | ---: | ---: | ---: |
| Correct steady changes, sequences 6–22 | 17/17 | 17/17 | 17/17 |
| Changes attributed to native route | 0 | 14 | 0 |
| Viewer detection P50 (ms) | 199.6538 | 40.2779 | 169.6643 |
| Viewer detection P95/P99 (ms) | 311.3702 | 75.5738 | 515.0288 |
| Host CPU (% of one core) | 8.75 | 5.48 | 5.24 |
| Viewer CPU (% of one core) | 1.06 | 2.47 | 1.57 |

Nearest-rank P95 and P99 both equal the maximum at n=17. The Host A baseline changed by 3.52 percentage points, about 40% of A1. The B Host CPU lies between the controls; Viewer CPU rose in B on this short run. A single middle arm does not establish stability. B reported one context and one PBO across 19 submissions, cleanup successful, host exit 0. The separate zero-Viewer run reported zero native allocations/submissions. The separate stop-in-flight run reported one pending job, cleanup successful, host exit 0, and no native frame published. There is no simultaneous two-Viewer measurement.

The synthetic source logs QPC before scheduling a Quick item color change. The observer scans a 12×12 region of the RealVNC client on the Windows composed desktop and matches four pixels, recording the first matching scan and available scan interval. This is desktop visibility detection, not an instrumented Viewer present event, DWM/scanout timestamp or input-to-photon latency. GPU utilization, Host RSS, local Qt input/render stalls and physical presentation were not measured. `#963` requires separate fixed-head measurements and lifecycle evidence before any default-on decision.

Raw-file SHA-256: A1 `c0ad486453ed468590d97104fcec0bf4bb015c4ebbb93b833645ffd11524dcf5`, B `18d42e744a72654c67b35b8905353d0d41a6933806960f0fa22dd7ae0d799467`, A2 `a24b0070c33a5551258029f76b46b1839e10d4e128b7ab14b43ea68bcf6adc8c`.
