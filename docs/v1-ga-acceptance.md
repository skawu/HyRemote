# HyRemote V1.0.0.0 GA acceptance matrix

Status: **release-candidate gate definition; no GA acceptance claim while required evidence is blocked or unexecuted.**

Authority: #33. Composes, but does not replace, milestone authorities #30, #31, #32, #39 and #41.

## 1. Release candidate environment

The integrated GA workflow is `.github/workflows/v1-ga-acceptance.yml`.

Reference matrix:

| Dimension | Windows | Linux |
| --- | --- | --- |
| Architecture | x86_64 | x86_64 |
| OS runner | Windows Server 2022 | Ubuntu 24.04 |
| Qt | 6.8.3 exact for GA matrix | 6.8.3 exact for GA matrix |
| Native QPA delegate | qwindows | qxcb |
| Linux display used by hosted correctness tests | n/a | Xvfb |

Xvfb is correctness infrastructure only. It is not evidence that a physical local monitor and local input remain usable while a remote viewer is connected.

## 2. Automated integrated gate

One release-like source tree is configured with all V1 product paths enabled at once:

- Core;
- `HyRemote::RemoteAccess`;
- Widgets adapter;
- Quick adapter;
- declarative QML module;
- bounded RFB transport;
- Transparent QPA Proxy;
- tests;
- E1–E5 examples.

The workflow must build that single tree and then pass all deterministic CTest suites. A mode that only passes in a separately reduced build does not satisfy this integrated gate.

## 3. Transport correctness

The release candidate runs the maintained RFB product-fit against the production internal RFB transport. Required behavior includes:

- bounded bind/start failure;
- incomplete-handshake expiry;
- maintained standard viewer framebuffer/input interoperability;
- disconnect/reconnect;
- abrupt viewer disconnect balances held pointer/key state (#90);
- subsequent viewer begins from clean input state;
- quiescent stop releases the listener.

SecurityType None remains only the current correctness baseline. Passing this gate does not make the stream authenticated or encrypted.

## 4. Integration-mode product E2E

### Embedded C++

E1 `widgets-basic` and E2 `quick-basic` must prove through the public `HyRemote::RemoteAccess` facade:

- real framebuffer delivery;
- view-only default isolation;
- explicit remote-input path;
- pointer/keyboard/text behavior;
- viewer disconnect/reconnect;
- backend-neutral connected-client diagnostics;
- explicit stop/listener release.

### Declarative QML

E3 `qml-basic` must prove the same shared runtime through `import HyRemote`, including live `connectedClientCount` lifecycle. QML must not expose or construct backend/session/client objects.

### Transparent QPA

The QPA CTest/product chain must prove the exact Qt 6.8.3 qualified proxy behavior:

- native-delegate preservation;
- native semantics/interception qualification;
- shared `RemoteAccess` composition;
- one listener/session across supported multi-surface churn;
- QWidget dialog/menu/popup scope;
- multiple QQuickWindow scope;
- current production capture-family checks;
- safe startup policy and reconnect.

E4 remains an ordinary Qt application. Its executable may not acquire HyRemote application-link dependencies merely to use Transparent QPA.

### Remote support showcase

E5 must retain explicit local start/stop, view-only safe initial policy, explicit control enablement, true listener-versus-connected status, reconnect/session diagnostics and meaningful Widgets/text interaction.

## 5. Installed/source SDK gate

The exact SDK produced from the integrated tree is installed and then consumed cleanly by:

- `tests/public-api-contract` — V1 public C++ type/ownership/default contract;
- E6 `tests/consumer-installed-sdk` — installed CMake package / `HyRemote::RemoteAccess` consumer;
- `tests/consumer-source` — source/add_subdirectory consumption;
- `tests/consumer-installed-qml` — installed QML module and QML deployment path;
- `tests/consumer-installed-qpa` — Qt-only application deployed with `hyremote_deploy(... QPA)`, launched with `-platform hyremote` without relying on `QT_PLUGIN_PATH`.

The QPA consumer must keep exact Qt private-ABI qualification. The QML and QPA deployment paths extend the single HyRemote deployment contract; they do not create separate user-facing backend commands.

## 6. Public API freeze gate

Before GA, #101 / `docs/v1-api-stability.md` must be accepted. The stable application-facing model remains:

- Embedded C++: `HyRemote::RemoteAccess`;
- QML: `import HyRemote` / `RemoteAccess` over the same runtime;
- Transparent QPA: exact-version package/launch/deployment mode, not a generic private-QPA C++ API;
- transport/capture/input/Core composition types are not a second normal application integration path.

## 7. Manual / physical acceptance not replaced by CI

The integrated workflow is necessary but not sufficient for `v1.0.0.0`.

Mandatory physical/manual evidence from #32/#33 still includes, on the claimed Windows and Linux reference environments:

- a real local application window remains visibly rendered through the native delegate while remote viewing is active;
- local pointer/keyboard input continues to reach the application while remote input is enabled as claimed;
- remote input does not disable or replace the native local path;
- QPA is demonstrably not replacement-only qvnc behavior;
- reconnect does not require application restart where claimed.

Use the Local Developer Agent only for this genuine local-display/input capability gap. Do not use it to substitute for unavailable GitHub-hosted runners.

## 8. Infrastructure evidence rule

#74 runner-assignment failures (`steps=[]`, no runner) are infrastructure failures. They are neither code failures nor passing product evidence.

If either reference OS job does not actually execute, the GA workflow is **unexecuted** for that OS and cannot authorize a release branch or tag.

## 9. Release authorization checklist

`release/v1.0.0.0` may be cut from `develop` only when all are true:

- #30 accepted and integrated;
- #31 accepted and integrated;
- #32 accepted and integrated, including required physical evidence;
- #39 accepted and integrated;
- #41 accepted and integrated;
- #90 disconnect correctness accepted;
- #91 connected-client diagnostics accepted;
- #101 API freeze accepted;
- #104 integrated GA workflow has actually passed on both reference OSes;
- compatibility/known-limitations/security documentation matches the candidate;
- no mandatory acceptance item remains blocked, failed or merely inferred.

## 10. Git Flow / tag rule

Release sequence is strictly:

```text
feature/* -> develop
              |
              v
release/v1.0.0.0 -> main -> tag v1.0.0.0
              |
              +---- back-merge/reconcile release result -> develop
```

On `release/v1.0.0.0`, root `project(HyRemote VERSION ...)` must be exactly `1.0.0.0`. No new product capability is added on the release branch.

The tag is created only after the accepted release branch is merged to `main`, and points to that accepted main release commit. A tag is a release fact, not a progress marker.

Governance mode: `transitional-explicit` until #14 passes reusable ADS machine acceptance.
