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

- expose a stable C/C++ API;
- use permissive licenses compatible with Apache-2.0 and broad embedded adoption;
- can be built with CMake/Meson/toolchain files in cross-compilation environments;
- allow optional features to be disabled;
- do not pull desktop-only dependencies into the core;
- have active upstream maintenance and reproducible releases.

## Fork policy

Do not create a permanent HyRemote fork merely to avoid contributing a generally useful change upstream.

A temporary pinned patch may be maintained when necessary, but it must document:

- why upstream cannot currently be used unchanged;
- the exact patch;
- the upstream issue/PR if applicable;
- criteria for deleting the patch.

## Initial candidates

### NeatVNC

Candidate role: first VNC/RFB transport backend.

Why it is being evaluated:

- embeddable server library rather than a full desktop stack;
- transport/protocol responsibility can remain outside HyRemote core;
- existing support for frame buffers, damage, input callbacks, and modern Linux graphics-oriented integrations;
- permissive ISC license, compatible with HyRemote's Apache-2.0 project distribution when its attribution obligations are preserved.

No dependency is considered frozen until its dedicated evaluation Issue is accepted.

## Qt

HyRemote's stable core should target Qt public APIs where possible. Qt private/QPA APIs may be used only in optional, isolated compatibility adapters with explicit Qt-version support statements.

Qt itself is an external dependency and remains subject to the Qt license selected by the downstream build/deployment. HyRemote's Apache-2.0 license does not alter Qt's licensing obligations.

## Platform libraries

GBM, DRM, RKMPP, V4L2, VA-API, and similar platform/hardware libraries must remain optional backend dependencies. They must not become required dependencies of the generic core.
