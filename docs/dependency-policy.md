# Third-Party Dependency Policy

HyRemote is licensed under the **Apache License 2.0** and is intended for reuse in commercial and open-source embedded products. Dependencies therefore need to be technically useful, maintainable, and legally compatible with distribution of HyRemote under Apache-2.0.

## Required review for every new dependency

Record the following before adding a dependency:

1. upstream project and canonical repository;
2. selected release/tag/commit;
3. license and attribution obligations;
4. compatibility with HyRemote's Apache-2.0 distribution;
5. static/dynamic linking implications where relevant;
6. project activity and release history;
7. security/update mechanism;
8. required versus optional status;
9. supported platforms;
10. fallback/removal strategy;
11. whether an upstream contribution is preferable to maintaining a fork.

## License compatibility rule

A dependency must not silently impose licensing terms that contradict the intended Apache-2.0 distribution of HyRemote or force downstream applications to adopt an incompatible license merely by using HyRemote.

Before vendoring, statically incorporating, or making a dependency required, record its exact redistribution and attribution obligations. Copyleft, source-disclosure, commercial-only, or otherwise restrictive terms require explicit review and must not be introduced into the generic Core by default.

Third-party code remains under its own license. Required notices and attributions must be preserved in the form required by that dependency's license.

## Preferred dependency shape

Prefer libraries that:

- expose a stable C/C++ API, or can be isolated behind a small stable C ABI when there is a justified cross-language backend;
- use permissive licenses compatible with Apache-2.0 and broad embedded adoption;
- can be built reproducibly with normal toolchain/package mechanisms;
- allow optional features to be disabled;
- do not pull desktop-only dependencies into the core;
- have active upstream maintenance and reproducible releases.

A non-C++ toolchain must not become a project-wide requirement merely because one optional backend uses it. Its build/runtime boundary, supported platforms and fallback strategy must be explicit.

## Fork policy

Do not create a permanent HyRemote fork merely to avoid contributing a generally useful change upstream.

A temporary pinned patch may be maintained when necessary, but it must document:

- why upstream cannot currently be used unchanged;
- the exact patch;
- the upstream issue/PR if applicable;
- criteria for deleting the patch.

## VNC/RFB backend candidates

Canonical x86 transport evaluation: [`x86-vnc-transport-evaluation.md`](x86-vnc-transport-evaluation.md).

### NeatVNC

Candidate role: Linux/Embedded Linux-oriented VNC/RFB transport backend.

Why it remains valuable:

- embeddable server library rather than a full desktop stack;
- transport/protocol responsibility can remain outside HyRemote Core;
- frame buffers, damage, input callbacks and modern Linux graphics-oriented integrations;
- permissive ISC license;
- strong fit for later Linux GBM/DRM/low-copy work without becoming a Core dependency.

The accepted #4 evidence does **not** establish native Windows support, so NeatVNC alone cannot satisfy the frozen V0.0.1.0 Windows + Linux product gate.

### rustvncserver

Current disposition: **Conditional GO for bounded x86 C ABI/build feasibility only** (#34), not production dependency approval.

- pinned evaluation release: `v2.2.1`;
- upstream license: Apache-2.0;
- upstream-declared Windows x86_64 and Linux x86_64/ARM64 support;
- Rust/Tokio implementation;
- Rust-native public API; no exported upstream C ABI was found during evaluation, so HyRemote's spike owns a thin opaque C ABI shim.

Before production adoption, #27 must resolve safe bind/listener control, lifecycle/error propagation, bounded downstream backpressure, real viewer interoperability, input fidelity and the cost of carrying a Rust/Cargo backend in a C++/Qt project.

### LibVNCServer

Current disposition: **NO-GO as HyRemote's default linked production backend**.

LibVNCServer is mature and cross-platform, but upstream is GPL-2.0-or-later and explicitly states that linking makes the program derivative work under GPL. That conflicts with the frozen normal Apache-2.0 HyRemote distribution model unless project licensing policy is explicitly changed.

### Custom HyRemote RFB implementation

Current disposition: **NO-GO by default**.

Owning an RFB server stack is not HyRemote's product value. Reconsider only if bounded evidence shows that maintained permissive backends cannot satisfy the product contract.

No transport dependency is considered production-frozen until #27 acceptance is complete.

## Qt

HyRemote's stable core should target Qt public APIs where possible. Qt private/QPA APIs may be used only in optional, isolated compatibility adapters with explicit Qt-version support statements.

Qt itself is an external dependency and remains subject to the Qt license selected by the downstream build/deployment. HyRemote's Apache-2.0 license does not alter Qt's licensing obligations.

## Platform libraries

GBM, DRM, RKMPP, V4L2, VA-API, and similar platform/hardware libraries must remain optional backend dependencies. They must not become required dependencies of the generic core.
