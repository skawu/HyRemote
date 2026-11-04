# Stage 02 G6 — Architecture decision-record reconciliation (2026-11-04)

**Scope:** documentation and historical cross-repository decision trace only. No Native protocol, TLS, authorization, resource guard, or changed deployment behavior has been implemented by this work. Product architecture **is not globally frozen**.

## Authority and provenance

- Approved product source: `HYR-PD-001 / V14`, *HyRemote 产品设计文档_正式版_V14.docx*, Human-approved 2026-11-03. SHA-256 of the reviewed source file: `b9c6029faf686535ea8f9dc830ae2106a8d2ceef12779e805d70ab1fe336cd4c`. The private source document is **not committed** to the public repository.
- Human-approved decisions dated 2026-11-04: `D-02=A`, `D-08=A`, `AR-04=A`, `AR-05=A`, `H-02=H-02a`; see [ADR-0008](../adr/0008-stage02-human-approved-architecture-decisions.md). None grants G7 global freeze.
- Technical architecture annex sources reviewed in this conversation, not silently represented as merged repository specifications:
  - R15 canonical contract candidate zip SHA-256 `631fbcde3b0c7db7c5eff47d74e227d96098aa24180b1880a98734569771ddaf` (21 Native message types; 50 positive / 48 negative structural vectors; 15 state scenarios).
  - R16 host-grant, zero-code deployment, Native authentication and resource signoff candidate zip SHA-256 `4f9be4ab668383f310f4c218bbd4b80a31fc3054ddcdec03d16ddfbaa7aa47d6`.
  - R18 design signoff and record-draft zip SHA-256 `39db1ded732f7a54de7e48c8605c8c6b96b03efa86b50eda70df2f7890222e30`.
- Design tracking classified all 52 V14 second-level clauses; this is traceability evidence, **not a claim that all full-product features are implemented in Native 1.0**.

## Cross-repository supersession references

| Source | Record action | Historical preservation |
| --- | --- | --- |
| [ADR-0007](../adr/0007-four-integration-runtime-architecture.md) | Amend Qt Private prohibition for the shared Runtime's dedicated remote-input/high-DPI unit per AR-04=A | Original topology and reason for keeping Core private-free remain |
| [ADR-0008](../adr/0008-stage02-human-approved-architecture-decisions.md) | Single dated authoritative summary for all five local Human choices | Explicitly states overall architecture remains not frozen |
| [HyRemoteView #4](https://github.com/skawu/HyRemoteView/issues/4) | Supersede *base route* of separate Enhanced TLS channel by full independent Native, retaining RFB | Preserve original G2/G3 history; no rewrite of former decision as if it never existed |
| [HyRemote #246](https://github.com/skawu/HyRemote/issues/246) | Update server-side HRC-5 dependency: no mandatory third base channel | Future diagnostics and enterprise capabilities remain conditional |

## Freeze and execution boundaries

- `G1–G5 = DESIGN_PASS` (based on V14 tracking and the R15–R18 annexes); real implementation/interop/physical test evidence remains `NOT_RUN` for new Native capabilities.
- `G6 = PASS` only after this record change is accepted through the repository PR process **and** both historical Issue threads contain correctly linked supersession notes, with readback.
- `G7 = NOT_APPROVED` until Human explicitly grants the *whole product architecture freeze*; then document TOC/content/figures/layout planning may start. No source implementation, stage03 Word compilation, release activation, or new compatibility claim is authorized by G6.
- If GitHub branch protection, CI or review policy prevents merge, record the exact failing gate and continue nonblocked record checks rather than treating an open PR as a merge receipt.

Tracking Issue: [#814](https://github.com/skawu/HyRemote/issues/814).
