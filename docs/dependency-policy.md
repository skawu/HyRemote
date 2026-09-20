# Third-Party Dependency Policy

HyRemote is licensed under the **Apache License 2.0** and is intended for reuse in commercial and open-source Qt products, including future embedded deployments. Dependencies must be technically useful, maintainable, legally compatible and invisible to normal application integration unless the product capability genuinely requires them.

## V1 product/toolchain rule

HyRemote V1 has one C++ Common Runtime and four peer application integration technologies:

```text
C++ API --------\
QML API ---------> Common Runtime -> Core
Generic Plugin --/
QPA ------------/
```

Consequences:

- the normal product implementation is C++17/CMake/Qt;
- C++/QML/Generic/QPA are integration frontends, not separate backend toolchains;
- Generic uses public Qt APIs and preserves the application's native QPA;
- QPA alone may use exact-version Qt private/QPA APIs;
- ordinary Windows/Linux SDK consumers do **not** install Rust, Cargo, Go, Node, Python or another backend-specific toolchain to use HyRemote;
- experiments in another language remain optional/research work and must not alter the public API, deployment contract or required consumer toolchain.

Python and other tools used by repository CI/product-fit are test tooling, not Runtime dependencies.

### Repository test/CI-only tools

| Tool | Repository use | License |
| --- | --- | --- |
| `aqtinstall` | acquire exact Qt packages for CI qualification | MIT |
| `vncdotool==1.3.0` | maintained VNC client used by product-fit | MIT |
| Pillow | image/pixel checks used by product-fit | HPND |
| Ninja | CI build executor | Apache-2.0 |

Their own upstream licenses remain authoritative; using them in CI does not make them installed HyRemote SDK dependencies.

## No vendored product dependencies

A product dependency is used only when a build requests the capability it serves. The repository:

- does not carry dependency source as a git submodule;
- does not maintain `vendor/`, `extern/`, `deps/`, `subprojects/` or other product dependency source trees;
- does not acquire Qt or OpenSSL with `FetchContent`, `ExternalProject` or an equivalent hidden downloader;
- consumes dependencies from the environment or an explicitly selected prefix/toolchain.

`tests/third_party/` is a **verification harness lane**, not a vendoring lane. A harness may fetch a recorded upstream application revision into an ignored work/build directory. Upstream project sources or licence trees are not committed into HyRemote.

`tests/release-readiness/check_licensing_boundary.cmake` enforces these rules.

## Qt

HyRemote Core remains Qt-free. Common Runtime and integration frontends use Qt as follows:

- public Qt APIs are preferred everywhere;
- Generic Plugin must remain public-Qt only;
- QPA private APIs are allowed only under `src/integrations/qpa` and require exact private-ABI qualification;
- the current reference QPA line is Qt 6.8.3;
- public-API compatibility evidence does not broaden a QPA private-ABI claim.

Qt is consumed dynamically from the user's/CI Qt installation. The product build never fetches, builds or patches Qt and does not statically incorporate Qt as an implementation convenience.

A distributor of a deployed application tree remains responsible for the licence/copyright/source-availability material required by the exact Qt distribution being shipped. See `NOTICE.md`.

## Transport security / OpenSSL

Transport security is a **Common Runtime capability**, not a fifth integration mode.

The build setting is:

```text
HYREMOTE_WITH_TRANSPORT_SECURITY
```

It is OFF unless requested. When ON:

- OpenSSL Crypto and SSL must already be available from the environment or an explicit prefix such as `OPENSSL_ROOT_DIR`;
- configure fails closed if they cannot be found;
- HyRemote never silently downgrades an explicitly requested secure-capability build;
- OpenSSL stays a private Runtime implementation dependency and is not exposed as a downstream SDK target.

If a binary release redistributes OpenSSL runtime binaries, the release must carry the licence/notice material required for that exact version. If a platform deliberately uses an operating-system OpenSSL, that prerequisite must be documented instead of silently bundling a different copy.

## Required review for any new dependency

Before adding a dependency record:

1. upstream project and canonical repository;
2. selected release/tag/commit;
3. licence and attribution obligations;
4. compatibility with HyRemote's Apache-2.0 distribution;
5. static/dynamic linking implications;
6. project activity and release history;
7. security/update mechanism;
8. required versus optional status;
9. supported platforms;
10. fallback/removal strategy;
11. effect on all applicable C++/QML/Generic/QPA integration paths and deployment;
12. whether an upstream contribution is preferable to maintaining a fork.

A dependency must not silently impose terms that contradict the intended HyRemote distribution or force downstream applications to adopt an incompatible licence merely by using HyRemote.

## V1 RFB/VNC transport decision

The Windows/Linux correctness baseline is HyRemote's **bounded C++ RFB 3.8 transport** behind the private transport seam in Common Runtime.

This is an implementation choice, not an application-facing API. All four frontends share the same transport-neutral session/capture/input model, so a future accepted transport backend may replace or supplement the implementation without creating frontend-specific Runtime personalities.

The insecure compatibility profile is explicit. `Authenticated` uses legacy RFB VNC authentication for interoperability and does not imply transport encryption. `AuthenticatedEncrypted` must fail closed if the configured secure transport cannot be established.

## Historical / future candidates

### rustvncserver

Historical C-ABI/toolchain feasibility work remains architecture evidence only. Rust/Cargo is not an active V1 product dependency or required SDK toolchain. Reconsideration requires a measured benefit and preservation of the normal C++/Qt consumer contract.

### NeatVNC

NeatVNC remains a possible future Linux/Embedded-Linux transport/low-copy candidate. It must not become a required Core or generic desktop dependency solely to optimize one platform.

### LibVNCServer

LibVNCServer remains **NO-GO as the default linked HyRemote backend** for the Apache-2.0 product line unless licensing policy is deliberately changed.

## Fork policy

Do not create a permanent HyRemote fork merely to avoid contributing a generally useful change upstream. A temporary pinned patch must document why upstream cannot currently be used unchanged, the exact patch/upstream issue or PR, and deletion criteria.

## Platform libraries

GBM, DRM, RKMPP, V4L2, VA-API and similar hardware/platform libraries remain optional evidence-driven acceleration dependencies unless a product requirement explicitly promotes one.

They must not:

- enter the generic Core;
- change the four integration contracts;
- force QPA/Generic applications to link SoC-specific libraries;
- require ordinary x86 V1 users to understand platform acceleration details.

Platform optimization stays behind the Runtime boundary and is introduced only when measured evidence justifies it.
