# Third-Party Dependency Policy

HyRemote is intended for reuse in commercial and open-source embedded products. Dependencies therefore need to be technically useful, maintainable, and license-compatible with a permissive project distribution model.

## Required review for every new dependency

Record the following before adding a dependency:

1. upstream project and canonical repository;
2. selected release/tag/commit;
3. license and attribution obligations;
4. static/dynamic linking implications where relevant;
5. project activity and release history;
6. security/update mechanism;
7. required versus optional status;
8. supported platforms;
9. fallback/removal strategy;
10. whether an upstream contribution is preferable to maintaining a fork.

## Preferred dependency shape

Prefer libraries that:

- expose a stable C/C++ API;
- use permissive licenses suitable for broad embedded adoption;
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
- permissive ISC license.

No dependency is considered frozen until its dedicated evaluation Issue is accepted.

## Qt

HyRemote's stable core should target Qt public APIs where possible. Qt private/QPA APIs may be used only in optional, isolated compatibility adapters with explicit Qt-version support statements.

## Platform libraries

GBM, DRM, RKMPP, V4L2, VA-API, and similar platform/hardware libraries must remain optional backend dependencies. They must not become required dependencies of the generic core.
