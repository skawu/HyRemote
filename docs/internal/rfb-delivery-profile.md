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

## Decision outputs

- selected compressed path and fallback order;
- measured improvement and regression bounds versus V0.3.0.0;
- dependency/licence decision if zlib or another codec is required;
- viewer matrix and known limitations;
- concrete implementation tasks for #144, without exposing encoder/queue knobs to normal users.

## Non-goals

- no new frontend, public API or user-facing compression setting;
- no ARM64/EGLFS/Wayland qualification in this profile (V0.3.4.0);
- no management-plane or TLS work (V0.3.5.0);
- no claim from synthetic-client data alone; maintained-viewer evidence is required.
