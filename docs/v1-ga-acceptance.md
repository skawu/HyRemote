# HyRemote V1.0.0.0 GA acceptance matrix

Status: **release-candidate gate definition; no GA acceptance claim while required evidence is blocked or unexecuted.**

Authority: #33. Composes, but does not replace, milestone authorities #30, #31, #32, #39 and #41.

## 1. Release candidate environment

The integrated GA workflow is `.github/workflows/v1-ga-acceptance.yml`.

| Dimension | Windows | Linux |
| --- | --- | --- |
| Architecture | x86_64 | x86_64 |
| OS runner | Windows Server 2022 | Ubuntu 24.04 |
| Qt | 6.8.3 exact for GA matrix | 6.8.3 exact for GA matrix |
| Native QPA delegate | qwindows | qxcb |
| Hosted display | native hosted Windows | Xvfb correctness scope |

Xvfb is correctness infrastructure only. It is not evidence that a physical local monitor and local input remain usable while a remote viewer is connected.

## 2. Frozen V1 artifact and usability gate

The release candidate must preserve this exact normal product shape:

- `hyremote-core` is **STATIC** and remains an internal composition layer;
- `HyRemote::RemoteAccess` / `HyRemoteRemoteAccess` is the one **SHARED** C++ product library;
- `qhyremote` is a Qt platform **MODULE** for the zero-source-change QPA path;
- the HyRemote QML module is a thin wrapper over the same shared `RemoteAccess` runtime.

`BUILD_SHARED_LIBS` must not switch the normal V1 application between static and shared product personalities.

The normal user contracts are deliberately small:

```cpp
HyRemote::RemoteAccess remote(&window);
remote.start();
```

or:

```text
MyApp -platform hyremote
```

A release candidate fails the usability gate if a normal application must assemble Core/Session/capture/input/transport/backend objects, manually select RFB internals, or manually copy private HyRemote runtime/plugin files.

## 3. Automated integrated gate

One release-like source tree is configured with all V1 product paths enabled at once:

- internal Core;
- shared `HyRemote::RemoteAccess`;
- Widgets adapter;
- Quick adapter;
- declarative QML module;
- bounded RFB transport;
- Transparent QPA Proxy;
- tests;
- E1–E5 examples;
- V1 release-readiness metadata.

The workflow must build that single tree and pass all deterministic CTest suites. A mode that only passes in a separately reduced build does not satisfy this gate.

The workflow must also assert the installed artifact classes themselves: static Core archive, shared RemoteAccess runtime and QPA platform module.

## 4. Transport and input-lifecycle correctness

The release candidate runs maintained RFB product-fit against the production internal RFB transport. Required behavior includes:

- bounded bind/start failure;
- incomplete-handshake expiry;
- maintained standard-viewer framebuffer/input interoperability;
- disconnect/reconnect;
- abrupt viewer disconnect balances held pointer/key state (#90);
- subsequent viewer begins from clean input state;
- quiescent stop releases the listener.

Explicit HyRemote runtime stop is also a terminal remote-input lifecycle boundary. After transport/Core callbacks are quiescent, the target input adapter must discard remote input accepted into its pending mailbox but not yet delivered to Qt, and balance any supported remote key/button state that was already delivered to the still-running local application. Repeated teardown must be idempotent and must not synthesize duplicate releases. This behavior is internal to the shared runtime and does not add an application-facing reset API.

The integrated deterministic suite must therefore retain evidence for all of the following:

- Widgets and Quick adapter shutdown balance delivered held state and drop pending undelivered input;
- `RemoteAccess::stop()` and runtime replacement/move teardown invoke the same terminal input cleanup exactly once;
- QPA composite child-surface pruning propagates terminal shutdown before a still-live detached child can retain remote state;
- QPA composite final shutdown is idempotent and cleans every remaining child adapter.

SecurityType None remains only the current correctness baseline. Passing this gate does not make the stream authenticated or encrypted.

## 5. Integration-mode product E2E

### Embedded C++

E1 `widgets-basic` and E2 `quick-basic` must prove through the public `HyRemote::RemoteAccess` facade:

- real framebuffer delivery;
- view-only default isolation;
- explicit remote-input path;
- pointer/keyboard/text behavior;
- viewer disconnect/reconnect;
- backend-neutral connected-client diagnostics;
- explicit stop/listener release;
- explicit stop/policy transition cannot leave delivered remote key/button state held and cannot inject queued remote input after stop.

### Declarative QML

E3 `qml-basic` must prove the same shared runtime through `import HyRemote`, including live `connectedClientCount` lifecycle. QML must not expose or construct backend/session/client objects. Disabling/stopping the QML wrapper uses the same terminal input-cleanup semantics as the C++ facade; QML does not own a second input stack.

### Transparent QPA

The QPA CTest/product chain must prove the exact Qt 6.8.3 qualified proxy behavior:

- native-delegate preservation;
- native semantics/interception qualification;
- shared `RemoteAccess` composition;
- one listener/session across supported multi-surface churn;
- QWidget dialog/menu/popup scope;
- multiple QQuickWindow scope;
- current production capture-family checks;
- safe startup policy and reconnect;
- a valid QPA configuration whose remote listener cannot bind remains a **remote-capability failure only**: the native delegate, native application window and Qt event loop remain operational rather than being torn down or replaced;
- child surfaces leaving the composed application canvas receive terminal child-input cleanup before their adapters are retired.

An invalid `hyremote-*` startup parameter is different: the QPA plugin must reject that invalid configuration before silently starting some other remote policy. The native-survival rule above applies to a valid transparent-QPA configuration whose remote runtime subsequently fails to start.

E4 remains an ordinary Qt application. Its executable may not acquire HyRemote application-link dependencies merely to use Transparent QPA.

### Remote support showcase

E5 must retain explicit local start/stop, view-only safe initial policy, explicit control enablement, true listener-versus-connected status, reconnect/session diagnostics and meaningful Widgets/text interaction.

The E1/E2/E3/E5 product-fit scripts are release inputs and must exist in the candidate tree. A workflow reference to a missing harness is a candidate integration failure, not infrastructure evidence.

## 6. Installed/source deployment gate

All public integration modes use the single package helper:

```cmake
hyremote_deploy(TARGET MyCppApp)
hyremote_deploy(TARGET MyQmlApp QML)
hyremote_deploy(TARGET ExistingQtApp QPA)
hyremote_deploy(TARGET ExistingQmlApp QML QPA)
```

The exact SDK produced from the integrated tree is consumed by:

- `tests/public-api-contract` — V1 public C++ type/ownership/default contract;
- E6 `tests/consumer-installed-sdk` — installed package / shared `HyRemote::RemoteAccess` consumer;
- `tests/consumer-source` — source/add_subdirectory consumer with the same public target/deployment call;
- `tests/consumer-installed-qml` — installed QML module and shared-runtime deployment path;
- `tests/consumer-installed-qpa` — Qt-only application deployed with `hyremote_deploy(... QPA)`.

Required deployment evidence:

- C++ installed and source consumers are installed into clean application prefixes;
- the deployed trees contain the shared `HyRemoteRemoteAccess` runtime but no separate Core runtime requirement;
- the deployed applications run without the original HyRemote SDK/build-tree runtime path;
- QML deployment carries the same shared facade automatically;
- requesting QML or QPA from an acquisition that does not actually contain the corresponding optional payload fails at configure time; stale metadata, a missing QML import/module directory, or a missing installed QPA module may not authorize a partial deployment;
- QPA deployment contains `qhyremote` plus the same shared facade while the executable itself remains Qt-only;
- the clean installed-QPA consumer compiles the real E4 application source rather than a duplicate look-alike fixture;
- QPA product-fit removes `QT_PLUGIN_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH`, `QT_QPA_PLATFORM` and SDK runtime-path assistance before launching the deployed application;
- Linux QPA relocation uses the controlled origin-relative/RPATH deployment rule rather than `LD_LIBRARY_PATH` as product configuration.

The QPA consumer must keep exact Qt private-ABI qualification. QML and QPA extend the same deployment contract; they do not create separate backend commands.

The installed candidate must preserve repository-owned `LICENSE` and `NOTICE.md` metadata.

## 7. Public API and release-readiness freeze gates

Before GA:

- #101 / `docs/v1-api-stability.md` must be accepted;
- #107 / `docs/releases/v1.0.0.0.md`, `NOTICE.md` and `docs/release-package-manifest.md` must be accepted as release-readiness metadata.

The stable application-facing model remains:

- Embedded C++: one shared `HyRemote::RemoteAccess` facade;
- QML: `import HyRemote` / `RemoteAccess` over the same runtime;
- Transparent QPA: exact-version package/launch/deployment mode, not a generic private-QPA C++ API;
- Core/Session/transport/capture/input composition types are not a second normal application path.

The target-input shutdown hook is an internal composition contract and is not an additional stable application API. Release-readiness must reject a candidate that removes the hook, reverses the required `Session::stop()` then input-shutdown ordering, drops Widgets/Quick/QPA propagation, or removes the deterministic tests that pin these semantics.

Release notes stay candidate/pending until the final release branch is accepted. Feature/develop must not set the root project version to `1.0.0.0`; that version change belongs on `release/v1.0.0.0`.

## 8. Manual / physical acceptance not replaced by CI

The integrated workflow is necessary but not sufficient for `v1.0.0.0`.

Physical/native evidence is tracked by #109. The repository execution runbook and evidence template is `docs/v1-physical-acceptance.md`; it versions the exact candidate/environment/cell record but **does not itself authorize or prove physical acceptance**. #109 remains the acceptance authority.

On the claimed Windows and Linux reference environments it must prove the applicable E1/E2/E3/E4 local-behavior cells, including:

- a real local application window remains visibly rendered through the native path while remote viewing is active;
- local pointer/keyboard/text input continues to reach the application while remote input is enabled as claimed;
- remote input does not disable or replace the native local path;
- QPA is demonstrably not replacement-only qvnc behavior;
- reconnect does not require application restart where claimed;
- during a control-enabled E1/E2 run, abrupt viewer disconnect while a supported key/button is held leaves the target clean and the next viewer starts clean;
- during a control-enabled E1/E2 run, explicit HyRemote stop/policy transition while a supported key/button is held returns the still-running local UI to neutral state and does not deliver pending remote input after stop;
- E3 proves the same stop-boundary behavior through the QML wrapper rather than inferring it solely from C++;
- E4 observes applicable child-surface/process teardown without stuck remote input or teardown crash/hang.

Use the Local Developer Agent or equivalent physical host only when #109 reaches its execution entry condition. Do not use it to substitute for unavailable GitHub-hosted runners. Evidence from one OS does not substitute for the other.

## 9. Infrastructure evidence rule

#74 runner-assignment failures (`steps=[]`/`steps=null`, no runner execution) are infrastructure failures. They are neither code failures nor passing product evidence.

If either reference OS job does not actually execute, the GA workflow is **unexecuted** for that OS and cannot authorize a release branch or tag.

## 10. Release authorization checklist

`release/v1.0.0.0` may be cut from `develop` only when all are true:

- #30 accepted and integrated;
- #31 accepted and integrated;
- #32 accepted and integrated, including required physical evidence;
- #39 accepted and integrated;
- #41 accepted and integrated;
- #90 disconnect correctness accepted;
- #91 connected-client diagnostics accepted;
- #101 API/artifact freeze accepted;
- #107 release-readiness metadata accepted;
- #109 required cross-mode physical/native evidence accepted, including explicit-stop/policy-transition input cleanup, with the exact-candidate record completed through `docs/v1-physical-acceptance.md`;
- #104 integrated GA workflow has actually passed on both reference OSes;
- clean deployed C++/source/QML/QPA consumers have passed the no-SDK-runtime-path gate;
- compatibility/known-limitations/security documentation matches the candidate;
- no mandatory acceptance item remains blocked, failed or merely inferred.

## 11. Git Flow / tag rule

Release sequence is strictly:

```text
feature/* -> develop
              |
              v
release/v1.0.0.0 -> main -> annotated tag v1.0.0.0
              |
              +---- back-merge/reconcile release result -> develop
```

On `release/v1.0.0.0`, root `project(HyRemote VERSION ...)` must be exactly `1.0.0.0`. No new product capability is added on the release branch.

The tag is created only after the accepted release branch is merged to `main`. It must be an annotated tag on the exact current accepted `main` release HEAD, and its peeled commit/version must match the release candidate. A tag is a release fact, not a progress marker.

Governance mode: `transitional-explicit` until #14 passes reusable ADS machine acceptance.
