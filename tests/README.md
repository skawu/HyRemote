# `tests/` - tests and product verification

Nothing in this directory is a **unit test**: a unit test needs private implementation detail, so it lives with the
module it qualifies (`src/*/tests/`) and is never installed.

Everything here falls into one of two groups, and both express something a test *inside* the product cannot:

- **Cross-module integration tests**, added directly under `tests/`, validate modules against each other.
- **Verification of the delivered product** builds against the product the way a user does, from outside the internal
  target graph:

| Directory | What it is |
| --- | --- |
| `consumer-installed-sdk` | an independent CMake project consuming the installed SDK |
| `consumer-installed-qml` | the same, for the installed QML payload |
| `consumer-installed-qpa` | the same, for the installed QPA payload |
| `consumer-source` | an `add_subdirectory` consumer (must not inherit developer-only switches) |
| `product-e2e` | product end-to-end scripts |
| `public-api-contract` | the public API surface contract |
| `third_party` | the real-world open-source application matrix |
| `release-readiness` | the release gates |

No target under `tests/` may become a product dependency, and no product module may include or link one. Recorded
review or acceptance evidence is documentation rather than test code: it lives under `docs/acceptance/`.

Ownership of the layout overall is `docs/internal/repository-layout.md`.
