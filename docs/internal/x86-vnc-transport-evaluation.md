# x86 Windows + Linux VNC/RFB Transport Evaluation — historical record

Status: **historical evaluation; superseded for V1 product selection by the bounded C++ RFB transport now integrated in the V1 candidate.**

This document records why several transport candidates were investigated. It is retained for engineering history and future backend work; it is **not** the current V1 backend-selection instruction.

Current product truth is in:

- `docs/dependency-policy.md`;
- `docs/architecture.md` / transport contracts;
- `docs/security.md`;
- the current C++ RFB implementation and product-fit gates.

## Current V1 decision

For V1 Windows x86_64 + Linux x86_64, HyRemote uses its bounded **C++ RFB 3.8 correctness transport** behind the private transport seam.

Reasons:

- one C++/Qt/CMake product toolchain;
- no Rust/Cargo prerequisite for normal SDK users;
- Windows and Linux share one correctness baseline;
- explicit loopback-capable listener policy;
- bounded Core→transport handoff and per-client state;
- standard-viewer interoperability/product-fit coverage;
- normalized input path including abrupt-disconnect balancing;
- backend implementation remains private, so future replacement does not change `HyRemote::RemoteAccess`, QML or QPA usage.

The current transport uses **SecurityType None**. This is a correctness/interoperability baseline, not an authenticated/encrypted Internet-facing server.

## Why the historical evaluation still matters

The earlier work established useful facts:

1. NeatVNC is a strong permissively licensed Linux/Embedded-Linux-oriented candidate, but the accepted evidence did not establish a native Windows path.
2. LibVNCServer is mature and cross-platform, but GPL linking obligations conflict with HyRemote's normal Apache-2.0 product distribution model.
3. rustvncserver v2.2.1 can technically be hidden behind a HyRemote-owned C ABI on Windows/Linux, but carrying Rust/Tokio/Cargo adds a second implementation toolchain and its then-current listener API did not satisfy HyRemote's safe bind-address contract.
4. A transport backend can remain replaceable behind HyRemote's transport abstraction without changing the application API.

Those findings remain valid historical architecture evidence even though the V1 selection changed.

## Historical candidate matrix

| Candidate | Historical finding | V1 disposition |
| --- | --- | --- |
| NeatVNC v1.0.1 / ISC | strong Linux/embedded fit; Windows not established by HyRemote evidence | future Linux/Embedded Linux backend candidate, not V1 x86 baseline |
| LibVNCServer / GPL-2.0-or-later | technically mature/cross-platform | not default linked backend under current Apache-2.0 product policy |
| rustvncserver v2.2.1 / Apache-2.0 | C ABI/toolchain feasibility demonstrated on Windows/Linux | historical spike only; not active V1 CI/runtime dependency |
| HyRemote bounded C++ RFB | initially treated as a last-resort custom path | **selected V1 correctness baseline after bounded implementation/evidence work** |

## Historical Rust FFI evidence

The #34 / PR #37 spike demonstrated that a thin opaque C ABI over rustvncserver was technically feasible.

Recorded run `34920869266` exercised the bridge on both reference desktop OS families and proved that a C++ process could load the Rust-produced dynamic library, resolve exported C symbols, update RGBA frame storage, and exercise listener lifecycle.

That result proved **FFI feasibility only**. It never established that Rust should become the product backend or that SDK consumers should install Rust/Cargo.

The Rust spike source was removed from the repository once this decision was recorded; it remains retrievable from git history at commit `3e6e191`. Its dedicated GitHub Actions workflows had already been retired from the active V1 CI surface. This document is the record that matters: the evaluation, its measurements and why the bounded C++ RFB transport was chosen instead.

## Historical rustvncserver blockers

At evaluation time, notable production-fit concerns included:

- Rust-native public API requiring a HyRemote-owned C ABI;
- a then-observed listener path binding `0.0.0.0:{port}` rather than exposing the explicit bind-address policy HyRemote requires;
- Tokio/runtime ownership and cancellation complexity;
- extra cross-build/toolchain burden for a C++/Qt project;
- need to prove bounded frame/client queues and input fidelity independently.

These concerns were part of the reason a second-language transport did not become the V1 default.

## NeatVNC future role

NeatVNC remains relevant to post-V1 Embedded Linux work, especially where GBM/DRM/low-copy graphics integration becomes a measured platform requirement.

If reconsidered, it must remain behind the same transport/application boundary. Embedded optimization must not force ordinary users to change from:

```cpp
HyRemote::RemoteAccess remote(&window);
remote.start();
```

or change the Transparent QPA launch/deployment contract.

## LibVNCServer licensing boundary

LibVNCServer remains unsuitable as HyRemote's default linked backend under the current Apache-2.0 product policy unless licensing policy is explicitly reconsidered. Technical maturity alone does not override downstream license obligations.

## Rules for any future transport replacement

A future backend is accepted only if it improves a measured product need while preserving:

- `HyRemote::RemoteAccess` as the normal C++ API;
- QML as a thin wrapper;
- QPA as a Qt-only application integration path;
- one `hyremote_deploy()` packaging contract;
- bounded nonblocking Core handoff;
- safe listener/input defaults;
- Windows/Linux or target-platform evidence for the claimed scope;
- no unexplained backend-specific toolchain for ordinary prebuilt-SDK consumers;
- compatible licensing and notice obligations.

Transport replaceability is an internal architecture capability, not a reason to expose backend selection complexity to application developers.
