# W1 — shared static text watermark policy: acceptance record

Scope: V0.3.5.0 second delivery item (#589) W1. This file records the accepted behaviour and how it was measured, so the
claim survives outside a chat log. It is **not** a second design document: the design authority is
`src/runtime/src/detail/watermark_policy.hpp` and the Owner decision on #589/#594 (option B).

## Accepted state

| Item | Value |
| --- | --- |
| SHA | `a1bfb42414c253dea5b83bebd97d1e3b80b6646c` |
| OS / compiler | Windows x86_64 / MSVC 14.44.35207 |
| Qt Kit | `C:\Qt\6.8.3\msvc2022_64` (6.8.3) |
| Command | `build.cmd test --config=build/local-qt683.yml` |
| Result | `DISCOVERED_TEST_COUNT=79`, `EXECUTED_TEST_COUNT=79`, **79/79 PASS**, CTest 453.93 s, whole command 510.4 s |

W1 identities inside that run:

```text
38/79 hyremote-watermark-policy-test           Passed 0.10 s
39/79 hyremote-watermark-compositor-test       Passed 3.05 s
40/79 hyremote-watermark-frame-decorator-test  Passed 0.34 s
```

## Requirement → implementation → observable evidence

| Requirement | Implementation | Evidence from `hyremote-watermark-policy-test` |
| --- | --- | --- |
| one shared policy type | `detail/watermark_policy.{hpp,cpp}`; routes submit a `WatermarkPolicyRequest` only | the four routes cannot hold their own defaults |
| disabled by default | `enabled = false` | `ok: the watermark is disabled by default` |
| bottom-right single line when unset | `kWatermarkDefaultPosition` | `ok: enabling without a position keeps the single bottom-right default` |
| 0.35 opacity default | `kWatermarkDefaultOpacity` | `ok: the default opacity is 0.35` |
| bounded validation, refused not clamped | text ≤128 code units, opacity 0.05–1.0, font 8–96 pt, `#RRGGBB`, known position tokens | `ok: an invisible opacity is refused instead of clamped`, `ok: font size above 96 points is refused`, `ok: a colour that is not #RRGGBB is refused with the expected form` |
| UTF-8 / invisible input | per-code-point check (`toUcs4`) | `ok: zero-width characters are refused instead of rendering nothing`, `ok: bidi override characters are refused`, `ok: emoji and non-BMP ideographs are accepted` |
| accurate diagnostics | `WATERMARK_ENABLED` / `_POSITION` / `_OPACITY` / `_FONT_SIZE` / `_COLOR` / `_TEXT_CODE_UNITS` / `_POLICY_ERROR` | `ok: only the text length is reported`, `ok: the operator text never appears in the report` |
| invalid-enabled refuses output, host keeps running | `access_instance.cpp` → `InvalidConfiguration`, state returns to `Stopped` | `ok: an invalid enabled policy refuses to start`, `ok: the retained diagnostic survives the quiescence stop` |
| no per-route default drift | `checkProductSurfaceDefaults()` (added by #738) | `ok: the C++ surface opacity default equals the shared default` |

Delivery history: policy #725, C++ surface #728, QML/Generic/QPA surfaces plus user documentation #729, C++ default
guard #738. The wire-level consequences of an enabled watermark are covered separately by #739 and #740.

## Measurement caveat (applies to every timing claim about this suite)

Three canonical runs of the same tree on the same machine:

```text
421.94 s   pre-merge verification
436.37 s
453.93 s   this acceptance run
```

±7 % wall-clock variance for identical code, identical tests, identical machine. A single run therefore cannot support
"the registration merge saved/did not save time". What is measurable is the identity count (94 → 79) and the shape of
the cost: `hyremote-v01-example-smoke` ≈86 s and the four installed consumers at 40–55 s each dominate, and no part of
the watermark work touches them.

## Still open (W2 acceptance, not W1)

- per-frame throughput benchmark (no animating target or compositor micro-benchmark exists yet);
- a viewer capture from a second machine (all captures so far are same-host viewer framebuffers).
