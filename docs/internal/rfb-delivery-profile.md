# RFB delivery profiling (#261)

Status: V0.3.3.0 candidate prerequisite. This is risk/evidence work, not product implementation and not an encoding decision.

## Purpose

Select the practical compressed + incremental delivery profile for #144 using measured viewer support, bandwidth, latency and resource cost. #144 explicitly does **not** hard-code ZRLE: Raw, TRLE, ZRLE, Hextile, Tight/JPEG or another interoperable profile are all candidates until this profile says otherwise.

## Current baseline (code facts)

- RFB 3.8 transport supports **Raw** and **TRLE** rectangle encodings; Raw is the deterministic fallback.
- DesktopSize, Fence and Continuous Updates are negotiated per viewer.
- Incremental damage is accumulated/coalesced per viewer; pending damage is bounded (#371).
- `test_rfb_wire_robustness` covers incremental requests, continuous updates, damage bounds, multi-viewer isolation and resize/full-refresh behavior.
- No zlib/ZRLE dependency is present in the current build; adding one would be a dependency-policy decision, not a free encoding switch.
- The Tool and four frontends consume the same access plane; no per-frontend encoding path exists.

## Workloads

| ID | Workload | Purpose |
| --- | --- | --- |
| W1 | Static Qt Widgets window | Baseline bytes/FPS; static-scene keepalive must be zero payload |
| W2 | Localized change (text, cursor, small control) | Damage efficiency and latency |
| W3 | Scrolling/high-motion Qt Quick | Bandwidth, frame freshness, CPU, queue bounds |
| W4 | Resize / full refresh | Correct geometry and fallback behavior |

Run each workload at the reference cells (Windows x86_64, Linux x86_64, Qt 6.8.3) before considering ARM64/embedded data.

## Metrics

- encoded bytes/frame and bytes/second;
- interaction latency P50/P95 (input-to-visible update);
- frame age/freshness; dropped/coalesced frames;
- encode CPU time P50/P95; process CPU and memory;
- pending damage/encoded-payload queue bounds;
- viewer-visible correctness (geometry, colors, input, reconnect).

A profile is only acceptable if the slowest viewer cannot create an unbounded backlog and Raw fallback stays correct.

## Candidate profiles

| Profile | Viewer reach | Dependency | Current status |
| --- | --- | --- | --- |
| Raw | universal | none | baseline/fallback |
| TRLE | maintained standard viewers | none | implemented, needs measured comparison |
| ZRLE | maintained standard viewers | zlib | not implemented; dependency review required |
| Hextile | legacy viewers | none | not implemented |
| Tight/JPEG | viewer-dependent, lossy | libjpeg/turbojpeg | not planned unless evidence demands it |

This table is a candidate list, not a selection. The decision must use the measured matrix, not preference.

## Harness

1. Start a real HyRemote host from the installed examples (offscreen where possible).
2. Drive W1-W4 with a synthetic RFB client that records the negotiated encoding, bytes and frame timestamps.
3. Produce a JSON report per workload/platform/encoding; keep raw captures out of the repository.
4. Run the same matrix with at least one maintained third-party viewer for interoperability evidence (#175).
5. Keep profiling out of ordinary PR gates until the method and thresholds are stable; the release evidence references the recorded report.

The harness must not require a visible GUI for the automated part; physical viewer checks are separate evidence.

## Running the harness

The profiler is a mode of the existing wire-robustness test binary; the normal test suite is unchanged when the
environment variable is unset.

```powershell
# Windows
$env:HYREMOTE_RFB_PROFILE_FILE="$PWD/build/rfb-delivery-profile.json"
.\build\runtime\tests\hyremote-rfb-wire-robustness-test.exe
```

```sh
# POSIX
HYREMOTE_RFB_PROFILE_FILE="$PWD/build/rfb-delivery-profile.json" \
  ./build/runtime/tests/hyremote-rfb-wire-robustness-test
```

It runs Raw and TRLE against W1 静态、W2 局部变化、W3 高动态、W4 full refresh and writes one JSON sample per
encoding/workload. The numbers are local-loopback indicative measurements, not a release SLO and not a substitute
for maintained-viewer evidence.

Windows x86_64/Qt 6.8.3 run from `rfb-profile.yml` (16 samples per measurement, 256x256 frame):

| Workload | Raw bytes | TRLE bytes | Raw p50 ms | TRLE p50 ms |
| --- | ---: | ---: | ---: | ---: |
| static | 0 | 0 | 0 | 0 |
| localized | 262,400 | 1,840 | 0.23 | 0.19 |
| high-motion | 4,197,440 | 19,520 | 2.08 | 0.51 |
| full-refresh | 262,160 | 1,040 | 2.08 | 0.48 |

Linux x86_64/Qt 6.8.3 run from the manual `rfb-profile.yml` lane (same frame size and workload definitions):

| Workload | Raw bytes | TRLE bytes | Raw p50 ms | TRLE p50 ms |
| --- | ---: | ---: | ---: | ---: |
| static | 0 | 0 | 0 | 0 |
| localized | 262,400 | 1,840 | 0.15 | 0.10 |
| high-motion | 4,197,440 | 19,520 | 1.30 | 0.33 |
| full-refresh | 262,160 | 1,040 | 1.48 | 0.47 |

These are local-loopback indicative numbers. They are consistent across Windows and Linux: the existing TRLE path
already removes more than 99% of the repeated payload in the localized and full-refresh cases, and more than 99% in
the synthetic high-motion case.

## Selected delivery baseline (2026-10-20)

The profiling evidence selects:

```text
Raw       deterministic fallback
TRLE      compressed region/full-frame path when the viewer advertises encoding 15
damage    accumulated region delivery for incremental requests
```

ZRLE is **not selected**. It remains a candidate only if a maintained GUI viewer demonstrates a material failure of
the Raw + TRLE path. No zlib dependency is added on the current evidence. Raw remains mandatory and is selected when
the viewer does not advertise TRLE.

This selection is a candidate-level decision, not release acceptance. It still requires maintained-viewer evidence
and the #370 interaction/resource envelope before #144 can close.

The next required evidence is a maintained third-party GUI viewer on both platforms, not another codec. The manual
`rfb-profile.yml` lane records both the Linux and Windows JSON artifacts with the same workload definitions.

## Decision outputs

- selected compressed path and fallback order: **Raw + TRLE**, Raw fallback, damage-based incremental updates;
- measured improvement and regression bounds versus V0.3.0.0: synthetic bytes and loopback latency recorded; no
  interaction-SLO or CPU/memory envelope yet;
- dependency/licence decision: **no zlib/ZRLE dependency** on current evidence;
- viewer matrix and known limitations: **open**; no maintained GUI viewer evidence yet;
- concrete implementation tasks for #144: no codec addition; close remaining viewer/performance evidence and decide
  whether the ARM64 cell required by the issue text belongs to V0.3.3.0 or V0.3.4.0.

## Non-goals

- no new frontend, public API or user-facing compression setting;
- no ARM64/EGLFS/Wayland qualification in this profile (V0.3.4.0);
- no management-plane or TLS work (V0.3.5.0);
- no claim from synthetic-client data alone; maintained-viewer evidence is required.
