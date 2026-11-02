# HyRemote V0.3.5.0 RC4 — Local Owner P2 real-Viewer work order

**Status: QUEUED_FOR_MANUAL_LOCAL_OWNER / NOT_DISPATCHED_BY_MACHINE_WORKER.** This document is an auditable repository work order, not a worker ACK, automatic dispatch, test result or Human approval. The schema-1 machine worker only supports `hosted-l2`, `canonical-g6` and `installed-generic-qpa-unicode`; a new arbitrary task is deliberately **not** added to its fail-closed inbox.

- Authority: [#95](https://github.com/skawu/HyRemote/issues/95), [#784](https://github.com/skawu/HyRemote/issues/784)
- Exact release ref: `release/v0.3.5.0`
- **Immutable candidate SHA:** `1e1f797dfb1ba41f28dc5cac765bc746ca2ab6db`
- Version: `0.3.5.0`; notes status: candidate / acceptance pending
- Reference: Windows x64, Qt 6.8.3 MSVC2022 Release; maintained **real RealVNC Viewer 7.15.1**
- Required evidence: clean exact-SHA source and tree, Kit/OS, unfiltered G6 run and installed build identity, logs/screenshots for observed real Viewer, one isolated state directory per scenario, exact case-by-case PASS/FAIL/NOT_RUN/BLOCKED, and machine/physical provenance. Never post plaintext watermark text or secrets in diagnostic output.
- Do not treat RC3 `d9f8f6ae` evidence or PR CI on `faf00bc` as RC4 physical PASS.

## P2 acceptance matrix

**A — explicit QML OFF vs omission (#784 A).** Seed a persisted nonempty watermark in a disposable `HYREMOTE_STATE_DIR`. Fresh QML instance with watermark property **omitted** restores the persisted mark; a separate fresh QML instance with explicit `watermarkText: ""` (also exercise programmatic explicit setter) must clear policy, persist OFF, and show **no watermark** in the actual Viewer on startup/restart. Record policy existence/state and redacted diagnostics. Distinguish declarative property assignment from default empty getter.

**B — rejected Running-phase changes (#784 B).** Start with mark A, change text and corner position while Running; Runtime must reject and the QML getters/effective remote image must remain A. Stop, apply mark B and a different corner, restart; getters, persisted policy and actual Viewer must agree on B. No stale equality-guard suppression; report errors truthfully without leaking text.

**C — corner bounds (#784 C).** Use deterministic near-width and near-height text/frame combinations for all four corners. Cases that do not reserve the real margin and glyph ink must fail closed with `does not fit`, empty output/no clipped or fallback unwatermarked remote frame. Confirm ordinary 1920×1080 corner and tiled paths, W2 compositor and Raw/TRLE remote correctness remain functional.

**Regression cross-check.** Installed Generic/QPA CJK+space, explicit OFF, modifier-only rejection, Unicode config/state/secret paths, QPA `delegate=windows` remain verified on the same exact RC4 build, using the separate machine order. Do not silently upgrade Qt5 QML, Linux/ARM64/RK3588, GPU, TLS, or Docker support.

## Delivery and admission

Post one **observed** Local Owner result to #95 referencing this work order and exact candidate SHA; include source identity, build/test commands, per-case evidence, Viewer identity and real limitations. Any FAIL or missing mandatory case => RC4 release HOLD; no note finalization or promotion. No machine-worker ACK is implied by this document. If the local machine/worker is not available, report BLOCKED/NOT_RUN rather than PASS. Cloud Owner performs independent evidence audit and final candidate admission.
