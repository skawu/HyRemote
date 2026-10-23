# TightVNC encoding negotiation finding (2026-10-23)

## Scope

This records the `SetEncodings` list actually advertised by TightVNC after the TRLE-preference fix.
It explains the high wire-byte result and qualifies the #144 TRLE viewer claim.

## Baseline

```text
ecee5d1 (TRLE preference fix head; code included in develop 58a9e3a)
Windows 11 25H2 x86_64
Qt 6.8.3 msvc2022_64
TightVNC 2.8.85
```

## Result

The Python TCP proxy captured TightVNC's client-to-server `SetEncodings`:

```text
7, 1, 16, 5, 2, 0, -26, -223, -224, -232, -239
```

It does **not** advertise TRLE (`15`).

Therefore the host correctly falls back to Raw. The same run produced:

```text
bytes upstream:   1,372
bytes downstream: 58,524,766
bytes total:      58,526,138 in 10 s (~5.85 MB/s)
```

This is consistent with the earlier 59.4 MB/10 s sample; the TRLE-preference fix cannot help a
viewer that does not advertise encoding 15.

## Implication for #144

The previous admission language "Windows viewers negotiated/decoded TRLE" is not true for
TightVNC:

```text
TightVNC TRLE negotiation: NOT ADVERTISED
TightVNC behavior:         Raw fallback (correct)
RealVNC TRLE negotiation:  not captured; still unverified
```

TRLE remains implemented and its wire tests pass, but a maintained viewer must actually advertise
and decode it before the compressed-path viewer claim is accepted.

## Boundaries

- Local loopback, one viewer.
- The proxy capture is a client-to-server stream; it does not claim RealVNC behavior.
- Not a release SLO.

## Evidence

```text
F:/workspace/hyremote/HyRemote-evidence/2026-10-23-encoding-negotiation/
├── summary.txt
└── proxy-result.json
```
