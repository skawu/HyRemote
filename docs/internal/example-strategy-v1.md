# HyRemote V1.0.0.0 Example Strategy

This document records the V1 example information architecture owned by #41.

The final physical example-directory migration is intentionally deferred to the one-time #209 foundation/layout change so the repository does not churn paths twice.

## Learning path

```text
01 Widgets minimal
 -> 02 Widgets control
 -> 03 Quick minimal
 -> 04 QML minimal
 -> 05 QML control
 -> 06 QPA existing app
 -> 07 Session + Security
 -> 08 Production showcase
```

## Design goals

The example set must cover four dimensions at once:

1. beginner -> advanced;
2. Widgets -> Quick -> QML -> QPA;
3. Qt 5.15 LTS -> Qt 6.8 LTS using one product API/runtime model;
4. source integration -> declarative integration -> zero/minimal-source-change integration -> installed SDK deployment.

## Rules

- Examples use public HyRemote APIs only.
- No example assembles Core/Transport/Capture/Session internals directly.
- Qt 5.15 and Qt 6.8 source examples are single-source where practical; compatibility conditionals remain internal/bounded.
- QPA is exact-patch/private-ABI qualified.
- Every HyRemote-authored GUI example uses the canonical project logo/icon through the project-owned branding asset path; no divergent copied logo files.
- Third-party upstream applications keep their own branding and source trees pristine.
- qBittorrent is the bounded V1 representative third-party verification lane; it is evidence, not a product support claim.
- E1-E6 remain release-authority labels where already referenced, but user-facing navigation is progressive rather than E-number-first.

## Logical examples

### 01-widgets-minimal
Smallest readable `HyRemote::RemoteAccess` Widgets integration: construct, start, stop, loopback-safe view-only default, visible status, project branding.

### 02-widgets-control
Remote-control lifecycle: explicit input enablement, stop/configure/start, pointer/keyboard/text, reconnect, DPR/resize, held-input cleanup, local/remote coexistence.

### 03-quick-minimal
C++ `HyRemote::RemoteAccess` against a `QQuickWindow`, with dynamic scene content and the same runtime semantics as Widgets.

### 04-qml-minimal
Canonical declarative path using `import HyRemote` and `RemoteAccess { target: mainWindow; enabled: true }`.

### 05-qml-control
Advanced QML lifecycle/input/reconnect/text-focus example, still using the shared runtime.

### 06-qpa-existing-app
Ordinary Qt-only application deployed with `hyremote_deploy(... QPA)` and started with `-platform hyremote`; native local display/input remains intact.

### 07-session-security
V1 operational surface: security profiles, cert/key configuration, auth rejection, bounded sessions/events, targeted terminate, fail-closed invalid configuration and numeric IPv4 contract.

### 08-production-showcase
Polished evaluator/operator-oriented example combining already-supported V1 capabilities and serving as the primary visual product demo.

## Qt coverage

All eight logical examples are required on Qt 5.15 and Qt 6.8 unless a documented module/private-API limitation is explicitly escalated. Qt 6.8.3 remains the complete V1 reference candidate. QPA builds are exact-patch only.

## Release evidence mapping

- E1 -> 01/02 Widgets
- E2 -> 03 Quick
- E3 -> 04/05 QML
- E4 -> 06 QPA
- E5 -> 07/08 operational/showcase
- E6 -> installed SDK consumer fixture under tests; do not duplicate it under examples

## Third-party verification

V1 keeps one representative real-world lane: `qbittorrent/qBittorrent`.

Reuse the successful #137 implementation/evidence rather than merging the broad five-project #136 test matrix. Results must be labelled `third-party verification example — not a V1 compatibility/support claim`.

## Migration

The physical tree migration, README rewrite, CMake/test-path updates, branding hookup and CI lane updates must land as one bounded #209-compatible change. Do not create simultaneous old/new canonical trees.

Refs: #33 #41 #57 #109 #134 #137 #165 #176 #209.
