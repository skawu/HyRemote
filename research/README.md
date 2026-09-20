# `research/` — non-product research evidence

## What this directory is for

`research/` holds the **spikes** that produced architecture evidence, plus the harnesses that made the measurements
reproducible. It is evidence for a decision, not product code:

- nothing here is part of the default build (`HYREMOTE_BUILD_SPIKES` is `OFF` by default, `cmake/HyRemoteProjectOptions.cmake`);
- **no CI workflow builds or runs any of it** (every workflow passes `-DHYREMOTE_BUILD_SPIKES=OFF` explicitly);
- no product or integration module may reference it - `tests/release-readiness/check_repository_layout.cmake` forbids
  `src/core`, `src/remoteaccess`, `integrations/qml/HyRemote` and `integrations/qpa` from doing so;
- nothing here is installed, packaged or covered by the V1 API stability contract
  (`docs/release-package-manifest.md`).

The authoritative statement of its role is `docs/repository-layout.md` ("`research/` contains spikes and historical
architecture evidence... not a V1 release dependency").

## What is in here, and where each decision landed

| Spike | Question it answered | Decision, recorded in | Status |
| --- | --- | --- | --- |
| `capture/` (SPIKE-01, #3) | Which public Qt capture API each target family can actually use, and what each one costs | `docs/capture-spike.md`; frozen into `docs/adr/0001-core-boundaries.md`, `docs/adr/0002-remoteframe-lifetime-timestamps.md`, `docs/adr/0003-threading-backpressure.md`; shipped as `docs/widgets-capture.md` and `docs/quick-capture.md` | **decided and implemented** (Widgets: `QWidget::render()` into caller-owned storage; Quick: `contentItem()->grabToImage()`) |
| `async-capture/` (SPIKE-02, #16) | Whether the public asynchronous path is sufficient before reaching for lower-level GL/PBO/RHI mechanisms | `docs/async-capture-spike.md`; frozen into ADR-0002/0003 and `docs/quick-capture.md` | **decided** (public path sufficient; bounded in-flight, drop-oldest, latest-frame-wins) |
| `vnc-transport-rust-ffi/` (#34/#38) | Whether `rustvncserver` could be consumed through HyRemote's own opaque C ABI | `docs/x86-vnc-transport-evaluation.md`, `docs/neatvnc-evaluation.md` | **not adopted for V1** (superseded by the bounded C++ RFB transport); retained as a historical record only |

## Is it required?

Not by the product, and not by the release. The evidence above is why: it is unbuilt, unreferenced by product code,
absent from CI and absent from the package, while every conclusion it produced is already written down in `docs/` and
the ADRs. The two capture spikes describe themselves as *throwaway, non-production code* that was *expected to be
deleted once the capture architecture decision is implemented for real* - and that decision has been implemented.

It is kept because it is the **audit trail**: a reviewer asking "on what measurement was `QWidget::render()` chosen" or
"why was the Rust transport dropped" can see the harness that produced the numbers and re-run it. Deleting it is
permissible, but it is not a local deletion: `CMakeLists.txt` (`:195-198`), `cmake/HyRemoteProjectOptions.cmake`,
`tests/release-readiness/check_repository_layout.cmake`, `check_release_documentation_layout.cmake`,
`check_consumer_simplicity.cmake`, `check_package_acquisition_isolation.cmake`, `tests/consumer-source/CMakeLists.txt`
and the seven workflows that pass `HYREMOTE_BUILD_SPIKES=OFF` all name it, and several `docs/` pages link to the
source paths. The trade is therefore "lighter tree" against "reproducible evidence for a past decision", and that is a
product-owner call rather than a cleanup.
