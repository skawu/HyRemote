# Third-Party Dependency Policy

HyRemote is licensed under the **Apache License 2.0** and is intended for reuse in commercial and open-source embedded products. Dependencies must be technically useful, maintainable, legally compatible, and must not make the normal application experience more complicated than the product itself requires.

## V1 toolchain/product rule

HyRemote V1 is **C++ first**:

- the normal application SDK is the shared C++ `HyRemote::RemoteAccess` library;
- QPA is a Qt platform MODULE using the same C++ runtime;
- QML is a thin wrapper over the same runtime;
- the normal Windows/Linux SDK consumer does **not** install Rust, Cargo, Go, Node, Python or another backend-specific toolchain to use HyRemote;
- implementation experiments may use other languages only behind an optional/research boundary and must not change the public application API, deployment contract or required consumer toolchain.

Python tools used by repository CI/product-fit are test tooling, not runtime dependencies.

### Repository test/CI-only tools

These tools are pinned/used by repository automation and acceptance harnesses. They are not installed as HyRemote runtime payloads and do not become downstream application dependencies merely by consuming the SDK.

| Tool | V1 repository use | License |
| --- | --- | --- |
| `aqtinstall` | fetch exact Qt packages for CI qualification | MIT |
| `vncdotool==1.3.0` | maintained VNC client used by product-fit | MIT |
| Pillow | image/pixel checks used by product-fit | HPND |
| Ninja | CI build executor | Apache-2.0 |

Their own upstream licenses remain authoritative; this table records why they do not alter HyRemote's shipped Apache-2.0 product surface.

## Optional build-time dependencies

A third-party dependency is used only when a build asks for the capability it serves. Nothing is acquired for a
build that does not request it; the project **never installs a library onto the host machine, and never carries one
as a submodule**, so the provider is always the environment the user already has.

For the authenticated/encrypted transport the single setting is `HYREMOTE_WITH_TRANSPORT_SECURITY` (OFF by default,
so a build that does not ask for the capability acquires nothing). An OpenSSL already present on the machine is
used, or one selected explicitly with `-DOPENSSL_ROOT_DIR=<prefix>`. There is no HyRemote provider selector and no
bundled OpenSSL build path. The provider/version actually used by an official release is recorded in the release
manifest rather than exposed as another product personality.

If `HYREMOTE_WITH_TRANSPORT_SECURITY=ON`, both OpenSSL Crypto and SSL are required at configure time. If they cannot
be found, configuration fails with an actionable diagnostic. HyRemote never converts an explicit secure-capability
request into a build that silently lacks that capability. A build that does not need transport security configures
with `HYREMOTE_WITH_TRANSPORT_SECURITY=OFF` and therefore has no OpenSSL requirement.

At run time, a dynamically linked provider must of course be deployable alongside the application according to the
platform loader rules. Official V1 binary SDK assets record and qualify one exact provider/version per platform and
carry any required redistribution/notices through the release packaging contract.

## Required review for every new dependency

Record before adding a dependency:

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
11. effect on normal C++/QPA/QML consumption and deployment;
12. whether an upstream contribution is preferable to maintaining a fork.

## License compatibility rule

A dependency must not silently impose licensing terms that contradict the intended Apache-2.0 distribution of HyRemote or force downstream applications to adopt an incompatible license merely by using HyRemote.

Before vendoring, statically incorporating, or making a dependency required, record its exact redistribution and attribution obligations. Copyleft, source-disclosure, commercial-only, or otherwise restrictive terms require explicit review and must not be introduced into the generic Core/default product path without a deliberate product decision.

Third-party code remains under its own license. Required notices and attributions must be preserved in the form required by that dependency's license.

## Preferred dependency shape

Prefer libraries that:

- expose a stable C/C++ API when they are part of the normal product build;
- use permissive licenses compatible with Apache-2.0 and broad embedded adoption;
- can be built reproducibly with the normal CMake/C++/Qt toolchain;
- allow optional features to be disabled;
- do not pull desktop-only dependencies into the Qt-free Core;
- have active upstream maintenance and reproducible releases.

A cross-language backend may be explored behind a small C ABI only when it solves a measured problem. It remains optional until its build/runtime/toolchain burden is explicitly accepted. It must never force ordinary prebuilt-SDK users to understand or install that backend's implementation toolchain.

## V1 RFB/VNC transport decision

The V1 Windows/Linux correctness baseline is HyRemote's **bounded C++ RFB 3.8 transport** behind the private transport seam.

This is now the product baseline because it provides the required cross-platform correctness path without adding a second toolchain to the SDK/application contract. It includes the bounded lifecycle/backpressure/protocol behavior used by the current product-fit gates, including disconnect/reconnect and held-input cleanup.

This decision does **not** make RFB an application-facing API. `HyRemote::RemoteAccess`, QML and QPA remain transport-neutral at their public boundary, so a future accepted transport backend can replace or supplement the internal implementation without changing normal application integration.

The current baseline uses **SecurityType None** unless an authenticated profile is configured. `Authenticated` offers only **RFB VNC authentication (security type 2)** and therefore authenticates the viewer but still does **not encrypt** the transport. `AuthenticatedEncrypted` remains unavailable until the later #143 TLS/VeNCrypt increment lands; no profile silently falls back to a weaker security type. This boundary is product/security policy, not a reason to leak backend configuration into the public API.

## Historical / future transport candidates

Historical evaluation remains useful architecture evidence, but these candidates are **not active V1 product dependencies or active CI product lines**.

### rustvncserver

Historical status: bounded C-ABI/toolchain feasibility was explored under the transport spike work. The Rust/Cargo workflows are no longer part of active V1 CI.

A future reconsideration would require a measured benefit large enough to justify carrying a second implementation toolchain while preserving all of the following:

- normal prebuilt SDK users install no Rust/Cargo tooling;
- C++/QML/QPA public APIs remain unchanged;
- deployment remains one `hyremote_deploy()` contract;
- Windows/Linux and future cross-build burden is explicitly qualified;
- lifecycle, backpressure, input and security behavior meet or improve the accepted product contract.

Historical experiments are recorded as documents under `docs/internal/`; their source is not kept in the tree, so nothing experimental can become a production dependency.

### NeatVNC

NeatVNC remains interesting for future Linux/Embedded Linux graphics/low-copy work because it is an embeddable permissively licensed server library. It is not the V1 Windows+Linux baseline and must not become a required dependency of the generic Core or normal C++ SDK merely to optimize one Linux platform.

A future Linux-specific backend may reuse the existing transport seam if measured embedded requirements justify it.

### LibVNCServer

Current disposition remains **NO-GO as the default linked HyRemote backend** for the Apache-2.0 product line because its GPL linking obligations conflict with the intended normal HyRemote distribution model unless licensing policy is explicitly changed.

## Fork policy

Do not create a permanent HyRemote fork merely to avoid contributing a generally useful change upstream.

A temporary pinned patch may be maintained when necessary, but it must document:

- why upstream cannot currently be used unchanged;
- the exact patch;
- the upstream issue/PR if applicable;
- criteria for deleting the patch.

## Qt

HyRemote's Core remains Qt-free. Product adapters use Qt public APIs wherever practical.

Qt private/QPA APIs are allowed only inside the isolated QPA package with an exact-version support statement and acceptance evidence. The V1 QPA package is qualified specifically for Qt 6.8.3; public C++/QML API similarity does not broaden that private-ABI claim.

Qt itself is an external dependency and remains subject to the Qt license selected by the downstream build/deployment. HyRemote's Apache-2.0 license does not alter Qt licensing obligations.

Two consequences are absolute rather than advisory, because they decide whether an Apache-2.0 product line can be distributed at all:

- **Qt is linked dynamically and consumed from the user's installation.** A static Qt, a patched Qt, or a Qt acquired by
  this build would change the relinking and source-availability story for every binary produced here, so the repository
  never fetches Qt and never builds it. The QPA payload is the one place coupled to Qt internals, through
  the exact private QPA interfaces of its qualified line; its own source is available under this project's licence,
  which is what keeps that coupling distributable.
- **Nothing third-party is vendored.** A dependency is consumed from the environment or from an explicitly selected
  prefix, and third-party applications used as verification examples are fetched at a recorded commit into an ignored
  build directory. `tests/release-readiness/check_licensing_boundary.cmake` fails the build on a submodule, a vendored
  dependency tree, an upstream source tree under the third-party lane, or any attempt to acquire Qt or OpenSSL with
  `FetchContent`/`ExternalProject`.

## Platform libraries

GBM, DRM, RKMPP, V4L2, VA-API and similar platform/hardware libraries remain optional post-V1 backend dependencies unless a real product blocker explicitly promotes one.

They must not:

- become dependencies of the generic Core;
- change the normal `HyRemote::RemoteAccess` application API;
- make QPA applications link backend-specific libraries;
- require ordinary x86 V1 users to understand platform acceleration details.

Optimization stays behind the product boundary; usability does not regress to expose an implementation technique.
