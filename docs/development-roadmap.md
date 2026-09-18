# HyRemote Development Roadmap

Status: execution roadmap toward **V1.0.0.0**.

Product milestones are externally usable release gates; internal implementation slices are not milestones by themselves. Current V1 implementation converges through Draft PR #106 -> `develop` as the single active product line.

## Frozen application model

HyRemote V1 keeps the normal user surface deliberately small:

- **Embedded C++:** one shared `HyRemote::RemoteAccess` library;
- **Declarative QML:** thin `import HyRemote` wrapper over the same runtime;
- **Transparent QPA:** package-owned `qhyremote` platform MODULE, with the application remaining Qt-only.

Core, capture, input, transport, RFB and QPA interception/composition remain implementation detail. The bounded C++ RFB 3.8 implementation is the V1 Windows/Linux correctness transport baseline; historical Rust/NeatVNC work is research/future-backend input, not a competing V1 product route.

## V0.0.1.0 — Windows + Linux x86_64 / Embedded C++

Authority: #30. Final tag: `v0.0.1.0` only after acceptance.

Candidate capability already integrated in #106:

- shared `HyRemote::RemoteAccess` facade;
- Widgets + Quick target adapters behind the same API;
- explicit start/stop;
- loopback and view-only defaults;
- bounded C++ RFB remote view/reconnect;
- normalized pointer/button/wheel/key/text input;
- abrupt viewer-disconnect held-input cleanup;
- backend-neutral connected-client count;
- installed SDK and source/add_subdirectory consumption;
- one `hyremote_deploy(TARGET MyApp)` path;
- E1 Widgets + E2 Quick examples and user guides.

Remaining acceptance is evidence, not another backend-selection phase:

- actual Windows x86_64 product matrix execution;
- actual Linux x86_64 product matrix execution;
- clean installed/source deployment evidence;
- required local-visible/local-input evidence under #109;
- final #30 review.

A `release/v0.0.1.0` build is version-profiled to Embedded C++ only; later QML/QPA source existing on develop does not make those modes part of this release.

## V0.0.2.0 — Windows + Linux x86_64 / Declarative QML

Authority: #31. Final tag: `v0.0.2.0` only after acceptance.

Candidate capability already integrated in #106:

- `import HyRemote` over the same shared runtime;
- compact `RemoteAccess { target: ...; enabled: true }` syntax;
- QML startup deferred to `componentComplete()` so declaration order does not become lifecycle glue;
- state/error/client-count properties without backend types;
- remote view/input/reconnect parity with the Quick C++ path;
- installed QML-module deployment through `hyremote_deploy(... QML)`;
- E3 `qml-basic` and complete QML guide.

Remaining:

- actual Windows/Linux QML/reference execution;
- clean installed-QML deployment evidence;
- applicable physical local-behavior evidence under #109;
- final #31 review.

`release/v0.0.2.0` permits C++ + QML and still rejects QPA as a released product mode.

## V0.0.3.0 — Windows + Linux x86_64 / Transparent QPA Proxy

Authority: #32. Final tag: `v0.0.3.0` only after acceptance.

Candidate capability already integrated in #106:

- exact Qt 6.8.3 private-ABI proxy package;
- default native delegate: `qwindows` on Windows, `qxcb` on Linux;
- shortest normal launch: `MyApp -platform hyremote`;
- application remains Qt-only at link/source layer;
- one persistent shared `RemoteAccess` session across supported application-surface churn;
- QWidget/Quick multi-surface composition/input routing;
- startup view-only/control policy;
- package-owned qhyremote payload resolved by `hyremote_deploy(... QPA)`;
- no installed `HyRemote::QpaPlatform` application target;
- E4 existing-app example and exact capture/support classification.

Remaining:

- actual Windows/Linux hosted/reference execution after #74;
- **defining physical native local-display/local-input + remote coexistence evidence** under #109;
- final #32 review.

This milestone proves HyRemote QPA is additive to the native platform path rather than replacement-only qvnc behavior.

## V1.0.0.0 — Windows + Linux x86_64 GA

Authority: #33. Final tag: `v1.0.0.0` only after acceptance.

GA composes all three accepted predecessor product modes plus:

- #39 minimal SDK/public-consumption contract;
- #41 complete E1-E6 examples/docs program;
- public API/artifact freeze;
- release package manifest, LICENSE/NOTICE and security boundary;
- exact compatibility/known-limitations matrix;
- integrated all-modes Windows/Linux acceptance;
- clean deployed C++/source/QML/QPA consumers;
- #109 physical/native evidence;
- strict Git Flow release/tag/backmerge sequence.

The integrated GA workflow runs all three modes together on the `0.0.0` convergence line and on `release/v1.0.0.0`. It does not run as an all-modes matrix on the three pre-GA release profiles.

## Release sequence

Each milestone follows the same release discipline:

```text
feature/* -> develop
             |
             v
release/vX.Y.Z.W -> main -> annotated vX.Y.Z.W
             |
             +---- backmerge/vX.Y.Z.W -> develop (restore 0.0.0)
```

The tag is created only after the accepted release branch is merged to `main`, and must point to the exact accepted current `main` HEAD.

No current milestone is taggable while mandatory Windows/Linux evidence is unexecuted. #74 jobs with no runner/steps do not count as pass or code failure.

## Post-GA

- `V1.x.0.0`: formal embedded platform-family expansion, Embedded Linux first;
- Rockchip and NXP i.MX remain high-priority platform families;
- **security** - authenticated and encrypted transport behind the stable application model - is a **V1.0.0.0
  requirement, not a `V1.x` track**: `[SEC-01]` #143 is x86 work, so it is sequenced inside V1
  (design -> authentication -> encryption -> per-client authorization/audit);
- **the RFB encoding strategy and damage-aware incremental delivery are V1.0.0.0 requirements too**: `[BW-01]` #144
  and damage-aware delivery (#9) are x86 work and belong to V1;
- only the genuinely embedded parts stay post-GA: low-copy buffer ownership (#17) and hardware encoding with the
  embedded target (#10);
- OpenHarmony remains longer-term and does not block x86 GA.

These expansions must preserve the same application-facing model rather than exposing platform/backend mechanics to users.

**Statement rule for both tracks.** `docs/known-limitations.md` and `docs/compatibility.md` are edited only in the change
that lands a capability, never in advance and never to imply progress. Until then the V1 statements stand exactly as
written, and nothing in these tracks is a V1 blocker or may widen the V1 GA declaration.

## Execution rule

- GitHub Issue / PR / machine evidence is canonical.
- Repository-accessible work is performed directly.
- Local Agent/physical hosts are used only for genuine local-only runtime/device evidence.
- Local Agent is not used to substitute for broken hosted CI/account infrastructure.
- Examples/documentation/deployment/security/release metadata are acceptance artifacts, not post-release polish.
