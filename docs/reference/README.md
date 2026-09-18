# Reference — product final-state contracts

This is the **reference zone**: the documents that describe what HyRemote *is* at its final product shape, as opposed
to how to operate it. If you are integrating HyRemote, this is where the contract lives; if you are looking for
instructions, start at [`../README.md`](../README.md) and the guide zone.

Language rule: this zone is **English-canonical**. The contract authority is the English document. A Chinese mirror
at `docs/en/reference/<path>` is optional; when one exists it carries the same one-line language switch and moves in
the same change as the English document. The rule is recorded in [`../../CONTRIBUTING.md`](../../CONTRIBUTING.md) and
pinned by the release-readiness documentation gate.

## Contracts

| Document | Contract it owns |
| --- | --- |
| [`architecture.md`](architecture.md) | Frozen V1 architecture: layers, dependency rules, threading principles |
| [`core-architecture.md`](core-architecture.md) | Proposal-era Core design input, explicitly not the frozen V1 architecture |
| [`product-overview.md`](product-overview.md) | The canonical long-form product description and non-goals |
| [`v1-api-stability.md`](v1-api-stability.md) | What is stable in the V1 public API and what is not |
| [`compatibility.md`](compatibility.md) | Exact per-configuration status matrix and the evidence each row needs |
| [`known-limitations.md`](known-limitations.md) | Limitations that are true of the current product |
| [`security.md`](security.md) | The security boundary that actually exists today |
| [`security-model.md`](security-model.md) | Threat model and the release security bar |
| [`versioning.md`](versioning.md) | Product version and milestone semantics |
| [`release-package-manifest.md`](release-package-manifest.md) | What a V1 release package contains |
| [`dependency-policy.md`](dependency-policy.md) | Dependency policy, including build-time and CI-only tooling |
| [`widgets-capture.md`](widgets-capture.md) | Widgets capture model |
| [`quick-capture.md`](quick-capture.md) | Qt Quick capture model |
| [`input-model.md`](input-model.md) | Input model and lifecycle boundaries |
| [`sdk-consumption.md`](sdk-consumption.md) | The user-facing SDK consumption contract |
| [`qml-consumption.md`](qml-consumption.md) | The user-facing QML consumption contract |

## Authoring rules for this zone

- **Final shape only.** No issue numbers, schedules, milestone chronicles, investigation logs or one-off checklists.
- **A capability is documented when it lands**, never in advance and never in a way that implies progress; the
  statement rule in [`../development-roadmap.md`](../development-roadmap.md) applies to the status-bearing documents
  here.
- **Status words are earned.** `Candidate`, `Unverified` and `Experimental` are evidence states, not marketing. A row
  that has not been measured says so.
- **Moving a document updates every reference in the same change**; the release-readiness documentation gate fails
  when a contract leaves this zone, when a guide document loses its English mirror, or when the language rule above
  is edited away.
- **Evidence is not written here.** Measured acceptance/review evidence belongs to `docs/acceptance/`, one directory
  per candidate.
