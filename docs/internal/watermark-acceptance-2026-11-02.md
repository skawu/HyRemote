# #589 watermark — consolidated acceptance record

This file is the reviewable archive of the "shared static text watermark" delivery. It states what was accepted, at which
SHA, with which evidence, and what remains unproven. It is not a design document: the design authority is the code and the
Owner decisions on #589 / #594 (option B).

## Delivered in code

| Item | PR | Merged as |
| --- | --- | --- |
| W1 shared policy (defaults, bounds, UTF-8, diagnostics, fail-closed) | #725 | `3e7777d` |
| W1 guard: the installed C++ surface's literal defaults pinned to the shared policy | #738 | `112506bd` |
| W2a server-side compositor (paint a copy, corner/tiled, DPR, refuse rather than clip) | #726 | `6bfd69c` |
| W2b transport wiring (one composition per captured frame, damage on the composed frame, fail closed) | #727 | `21c508c` |
| W2c-1 C++ product surface (`RemoteWatermark`, `setWatermark`, `clearWatermark`) | #728 | `4f20e47` |
| W2c-2 QML / Generic / QPA surfaces + user documentation | #729 | `9f0c1d3` |
| Wire-level proof that the composed frame reaches TRLE and Raw viewers | #739 | `4a181a13` |
| Wire-level proof across resize and per-frame device pixel ratio | #740 | `74526683` |
| Glyph realness (no tofu) for the fallback chain | #744 | `92ef1a26` |
| Persistence of an explicitly configured policy across restarts | #747 | `76371e42` |
| Desktop-size compose cost measurement | #749 | `ba20bf8a` |

## Acceptance at the frozen SHA

Frozen SHA `fdf87dbfcd9047e9113dbd03f7a7100ad00d0f12` (the first SHA containing both #747 and #748).

Environment: Windows x86_64, Qt 6.8.3 `msvc2022_64`, MSVC 14.44.35207, Release/Ninja.

| Claim | Verdict | Evidence |
| --- | --- | --- |
| Watermark visible on a real viewer | PASS | `freeze-wm-on.png` (evidence branch `84fc3edd`) |
| Disabling restores the original picture | PASS | `freeze-wm-off.png`; pixel diff vs the on-capture has bbox **(289,160)-(452,178)** — only the mark |
| Configured policy survives a restart, at its stored position | PASS | `restart-first.png` (bottom-left) vs `restart-second.png` (second launch passes only `port=5921`, mark still bottom-left) |
| QML route launches from the build tree and serves a remote picture | PASS | `qml-buildtree-remote.png` (branch `cb30294c`), viewer exit 0 |
| Second physical device can watch the session | PASS | peer record `192.168.1.5:46190 → 192.168.1.6:5921` (branch `24939cc1`), operator confirmed the picture |
| Desktop-size compose cost measured | PASS (measured, not asserted) | `[bench] 1920x1080 compose median: corner=0.657 ms, tiled=14.1138 ms` |

Unit and wire-level suites behind those claims: `hyremote-watermark-policy-test`,
`hyremote-watermark-compositor-test`, `hyremote-watermark-frame-decorator-test`, plus the watermark cases inside
`hyremote-rfb-wire-robustness-test`. Local canonical: 79/79 PASS; hosted Qt 6.8.3 and Qt 5.15.2 both pass.

## Known limitations, stated rather than hidden

1. **Second-machine evidence is a peer record, not a screenshot from that machine.** The host logged the peer address;
   the operator confirmed seeing the picture. A screenshot exists only for same-host captures.
2. **Font coverage is a host property.** The compositor delegates to Qt's per-character family chain. The test proves
   distinct CJK glyphs render to distinct pixels (a tofu render would be identical) and skips loudly where the host has
   no CJK family.
3. **Tiled is expensive by design.** 14.1 ms/frame at 1080p versus 0.66 ms for the default corner line; tiling stays
   opt-in for that reason and because Owner option B asks for a single corner line by default.
4. **Compose cost excludes capture, encoding and delivery.** It is a floor, not a frame budget.
5. **The state directory holds the policy only.** No credentials, no descriptor contents. A database path remains absent
   by design: no DB-backed feature exists, and exposing a selector without a consumer is exactly the defect #591 found.

## Out of scope

Docker (#592) and V0.4 third-party application qualification were excluded by the dispatch and untouched.
