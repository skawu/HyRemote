# `tests/` - tests only

This directory holds **tests**, and only tests.

- **Cross-module integration tests** live here, because they belong to no single module and validate the modules
  against each other.
- **Unit tests live with the code they test** (`src/*/tests/`): they need private implementation detail and are never
  installed. A unit test does not belong in this directory.

Everything that is *verification of the delivered product* rather than a test lives under `verification/`, because it
builds **against** the product the way a user would, which is not something a unit or integration test should do:

| Verification | What it is |
| --- | --- |
| `verification/consumer-installed-sdk` | an independent CMake project consuming the installed SDK |
| `verification/consumer-installed-qml` | the same, for the installed QML payload |
| `verification/consumer-installed-qpa` | the same, for the installed QPA payload |
| `verification/consumer-source` | an `add_subdirectory` consumer (must not inherit developer-only switches) |
| `verification/product-e2e` | product end-to-end scripts |
| `verification/public-api-contract` | the public API surface contract |
| `verification/third_party` | the real-world open-source application matrix (issue #134) |
| `verification/release-readiness` | the release gates |

Ownership of the layout overall is `docs/internal/repository-layout.md`.
