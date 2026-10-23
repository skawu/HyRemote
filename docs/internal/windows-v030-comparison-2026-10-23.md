# V0.3.0.0 versus V0.3.3.0 Windows interaction comparison

## Scope

This is a directional comparison for #370. The two releases used different harnesses and target
applications, so it is not a controlled A/B microbenchmark.

## Baselines

V0.3.0.0 (#410 wired-LAN real-user interaction result):

```text
P50 74.92 ms
P95 95.60 ms
n=39/40 valid
about 60 ms of the P50 was the synthetic gesture hold
```

V0.3.3.0 (this cycle, local loopback input-to-visible):

```text
RealVNC  P50 49.74 ms / P95 84.46 ms
TightVNC P50 52.26 ms / P95 84.41 ms
```

## Direction

Both V0.3.3.0 maintained viewers show a lower measured P50 and comparable or lower P95 than the
V0.3.0.0 real-user result. The V0.3.3.0 values are local-loopback and include the separate capture
harness floor; the V0.3.0.0 value includes a wired-LAN leg and a gesture hold.

```text
Directional result: V0.3.3.0 input-to-visible is not a regression
Controlled A/B:     not claimed
```

## Boundary

- Different harnesses, targets and network scope.
- V0.3.3.0 values are local loopback; V0.3.0.0 value is wired LAN.
- No WAN or release-SLO claim.

## Evidence

- `docs/releases/v0.3.0.0.md` (#410 baseline)
- `F:/workspace/hyremote/HyRemote-evidence/2026-10-22-real-viewer-perf/`
