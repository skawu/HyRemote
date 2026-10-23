# RealVNC TRLE real-viewer evidence (2026-10-23)

## Scope

This records a maintained-viewer TRLE negotiation and wire-byte sample for #144/#370 on Windows
x86_64. It uses a local loopback TCP proxy.

## Baseline

```text
aee08bbc1bd143f74ad4b2a6a4c62481f5b2c055
Windows 11 25H2 x86_64
Qt 6.8.3 msvc2022_64
RealVNC Viewer 7.15.1
480x240 Widgets panel toggled at 10 Hz
```

## Configuration

RealVNC was started with:

```text
-SecurityNotificationTimeout=0
-WarnUnencrypted=0
-EnableUdpRfb=false
-PreferredEncoding=TRLE
```

The TCP proxy captured the client `SetEncodings`; the list contains `15` (TRLE):

```text
24, 15, 5, 16, 22, 21, 6, 2, 0, 1, -314, -239, -223
```

## Result

```text
bytes upstream:   266
bytes downstream: 3,343,134
bytes total:      3,343,400 in 10 s (~334 KB/s)
```

For comparison, TightVNC 2.8.85 does not advertise `15` and used Raw at approximately 58.5 MB/10 s
(~5.85 MB/s).

## Interpretation

```text
RealVNC TRLE negotiation: PASS
TRLE wire profile on this target: ~334 KB/s
TightVNC Raw fallback: ~5.85 MB/s
```

This confirms that the selected TRLE path is usable by a maintained viewer and materially reduces
wire bytes for the same target. It does not claim identical behavior for every viewer or WAN.

## Boundaries

- Local loopback only; one RealVNC viewer.
- The proxy path uses TCP and disables RealVNC Direct UDP.
- V0.3.0.0 comparison remains separate.
- TightVNC remains Raw fallback; that is truthful but high-bandwidth.

## Evidence

```text
F:/workspace/hyremote/HyRemote-evidence/2026-10-23-realvnc-trle/
├── summary.txt
└── result.json
```
