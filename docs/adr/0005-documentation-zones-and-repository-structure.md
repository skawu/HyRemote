# ADR-0005: Documentation zones, repository structure and their enforcement

Status: **Accepted for implementation (product direction 2026-09-18)**. Supersedes the implicit assumption that
declaring a documentation policy in prose is sufficient.

## Context

**Product positioning** (`docs/product-overview.md`): HyRemote is an independent, open-source Qt remote-access
framework whose value is a *small, stable, application-facing* contract - three mandatory integration modes (Embedded
C++, Declarative QML, Transparent QPA) over one shared runtime - sold to users as prebuilt SDKs and as source
consumption. Its differentiator list explicitly includes "richer examples and documentation ... with exact
compatibility claims".

**Project requirements in force**: `CONTRIBUTING.md` (repository paths are architecture boundaries; the three
documentation zones with their language rules), `docs/README.md` (the bilingual navigation root, the three zones and
the writing conventions), `docs/repository-layout.md` (the canonical repository layout, the V1 layout freeze, recorded
evidence living under `docs/acceptance/`), and `docs/development-roadmap.md` (the statement rule).

**Measured state versus those requirements**:

| Requirement | Declared in | Measured state |
| --- | --- | --- |
| `docs/guide/**` = end-user final shape, Chinese primary + `docs/en/**` mirror | `CONTRIBUTING.md` 132-139, `docs/README.md` 20-29 | `docs/guide/` exists with `install.md` and `cross-compilation.md`, both mirrored. `deployment.md`, `viewer-connection.md` and `troubleshooting.md` are user-facing per `docs/README.md` 16-18 but sit flat at `docs/` |
| `docs/reference/**` = product final-state contracts, same bilingual rule | same | **The directory does not exist.** All 12 contracts named by `docs/README.md` 33-44 (`architecture.md`, `v1-api-stability.md`, `compatibility.md`, `known-limitations.md`, `security.md`, `security-model.md`, `versioning.md`, `release-package-manifest.md`, `dependency-policy.md`, `widgets-capture.md`, `quick-capture.md`, `input-model.md`) plus `core-architecture.md`, `product-overview.md`, `sdk-consumption.md`, `qml-consumption.md` sit flat in a 50-file `docs/` |
| Recorded evidence lives under `docs/acceptance/` | `docs/repository-layout.md` 58, 112-114 | **The directory does not exist**, and the acceptance evidence produced for #109 exists only as GitHub comments |
| The zones and the bilingual pairs are enforced | `docs/repository-layout.md` 31 (repository layout gate) | `tests/release-readiness/check_release_documentation_layout.cmake` checks **tokens inside documents**; nothing machine-checks that a zone exists, that a document is in the right zone, or that a declared mirror exists |
| Declared structure is what the repository actually has | - | CI pins `docs/quick-capture.md` and `docs/widgets-capture.md` (`.github/workflows/*-adapter.yml` 10), and `cmake/toolchains/` cites `docs/compatibility.md`, so the flat layout is load-bearing |

**Why the gap matters for this product**: the same document set is the contract that a user is told to read before
integrating, and the evidence record that a release authority reads before authorizing a tag. A policy that exists
only in prose means (a) a user cannot tell a final-state contract from a working note or a spike, (b) a claim can drift
from the candidate without anything failing, and (c) the statement rule in `docs/development-roadmap.md` (edit
limitations/compatibility only in the change that lands a capability) has no enforcement.

## Decision

1. **The three zones are physical, not notional.** `docs/guide/**` (end-user final shape), `docs/reference/**`
   (product final-state contracts), and the internal/release zone (`docs/` remaining, `docs/adr/`, `docs/releases/`,
   `docs/proposals/`, `docs/acceptance/`) are directories with the language rules already declared in
   `CONTRIBUTING.md`. A document that belongs to a zone moves into it; the zone's own `README.md` is its index.
2. **`docs/acceptance/` becomes the home of recorded evidence**, one directory per candidate, next to
   `docs/v1-physical-acceptance.md` as its runbook. Evidence copied out of an Issue is a documentation record, not a
   test, and never enters the test graph.
3. **Migration is staged and reference-preserving.** Every reference to a moved document is rewritten in the same
   change (documentation cross-links, workflow path filters, toolchain CMake comments, release-readiness token
   checks). No forwarding stubs and no duplicates: the layout authority forbids compatibility copies.
4. **The zones and the bilingual duty are machine-checked** by extending the release-readiness documentation gate, so
   a document that is in the wrong zone, a zone that disappears, or a missing declared mirror fails a deterministic
   test rather than a review opinion.
5. **The language rule for `reference/` is not silently changed by this migration.** `CONTRIBUTING.md` declares
   Chinese-primary with an English mirror for both user-facing zones, while the reference corpus is currently written
   in English. Moving the files does **not** satisfy that rule by itself, and translating the corpus is a body of work
   with a schedule consequence. Two honest shapes are available (see the consequence below); until the product side
   chooses, the migration lands the structure and the gate encodes the declared rule **only** for the documents that
   already satisfy it, with the rest listed as an explicit, tracked migration set rather than being pretended away.

## Consequences

- A user can tell a contract from a working note by its path, which is the whole point of the separation.
- `docs/README.md`, `CONTRIBUTING.md`, the workflow path filters and the release-readiness token checks must all move
  in the same change as the files; the gate is what keeps them honest afterwards.
- The reference corpus's language status must be decided rather than inherited: either the declared Chinese-primary +
  English-mirror rule is honoured (a translation body of work that has to be scheduled), or the rule is amended for
  `reference/` to "English canonical, optional Chinese mirror". **This ADR recommends the second only if the product
  side accepts that reference contracts are read by integrators primarily in English**; the first is the faithful
  reading of the existing policy.
- The V1 layout freeze (`docs/repository-layout.md` 27-31) is not violated: this is a documentation-internal
  reorganization inside a declared policy. No source module, build path or artifact location changes, so
  `build/remoteaccess`, `build/qml/HyRemote` and `build/plugins/platforms` stay exactly as they are.
