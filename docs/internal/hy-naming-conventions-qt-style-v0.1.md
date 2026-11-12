# Qt-style Hy naming conventions for HyRemote (v0.1, P0 audit)

Status: **PROPOSAL / READ-ONLY AUDIT**. This document is not frozen. Cloud Owner
technical review is required before any rename is executed.

| Field | Value |
| --- | --- |
| Issue | #969, phase P0 |
| Base | `develop` at `64cae8977e06b21c23c21a5787a38bfc6db1f2e0` |
| Execution owner | Local Codebuddy, read-only initial phase |
| `TEST_PLAN` | `READ_ONLY` |
| Edit scope | `docs/internal/` only |
| Excluded scopes | `src/`, `tests/`, `tools/test/`, `.github/`, `cmake/`, `examples/` |
| Not run | `build.cmd test`, CTest, any Qt or RealVNC performance matrix, any GUI window |
| Deliverables | this document, `hy-naming-inventory.csv`, `hy-naming-inventory.json`, `hy-naming-audit-summary.json` |

## 1. What this document is

It records a Qt-style `Hy` prefix convention and a whole-tree, read-only naming
inventory at one fixed SHA. It proposes `existing -> proposed` mappings, classifies
compatibility impact, lists exemptions, compares three migration boundaries and
sketches a phased plan with a minimal lint gate.

It is a prospective convention for **new** code. It does not modify
[`naming-conventions.md`](naming-conventions.md), which describes what the tree
already does, and it does not supersede it. The two are compatible: one is
descriptive, this one is a proposal. Neither changes
[`v1-api-stability.md`](../v1-api-stability.md),
[`repository-layout.md`](repository-layout.md) or `CONTRIBUTING.md`.

No class, file, namespace, CMake target, QML URI, plugin token, C symbol, CTest
identity or public API/ABI was changed. No rename was performed. The only files
created by this turn are under `docs/internal/`.

Authority order used here: current `develop` plus #969 and its latest Owner
comment (2026-11-12 10:21Z), then exact code, then the current-state naming
document, then historical proposal text. #967 performance work is complete and is
isolated from this audit.

## 2. Qt source evidence

Qt naming is taken from the exact-version source trees, not from articles or
forum posts. Paths below are version-pinned repository-relative paths. No local
absolute path is recorded.

| Evidence | Location | What it confirms |
| --- | --- | --- |
| source-confirmed | `qtbase@6.8.3:src/corelib/kernel/qobject.h`, `qobject.cpp`, `qobject_p.h` | `QObject` maps to `qobject.h`/`.cpp`, private class to `qobject_p.h` |
| source-confirmed | `qtbase@6.8.3:src/corelib/kernel/qobject_p_p.h` | a second, deeper private header uses `_p_p.h` and only where a real extra layer exists |
| source-confirmed | `qtbase@6.8.3:src/widgets/kernel/qwidget.h`, `qwidget.cpp`, `qwidget_p.h`, `qwidgetwindow_p.h`, `qwidgetrepaintmanager_p.h` | same shape holds for a second Qt class family |
| source-confirmed | `qtbase@6.8.3:src/corelib/global/qtclasshelpermacros.h:137`, `:158`, `:159` | `Q_DECLARE_PRIVATE`, `Q_D`, `Q_Q` are the pimpl access macros |
| source-confirmed | `qtdeclarative@6.8.3:src/quick/items/qquickwindow.h`, `qquickwindow.cpp`, `qquickwindow_p.h`, `qquickwindowattached_p.h`, `qquickwindowmodule_p.h`, `qquickwindowmodule_p_p.h` | `QQuickWindow` follows `QFoo` / `qfoo.h` / `qfoo.cpp` / `qfoo_p.h`; `_p_p.h` is rare |
| source-confirmed | `qtbase@v5.15.2:src/corelib/kernel/qobject.h`, `qobject.cpp`, `qobject_p.h`; `qtdeclarative@v5.15.2:src/quick/items/qquickwindow.h`, `qquickwindow.cpp`, `qquickwindow_p.h` | the same naming shape exists in the Qt 5.15.2 tag, so one convention can cover both qualified LTS families |

Qt 5.15.2 files were checked through the upstream `v5.15.2` tag, since the local
Qt 5 installation only carries headers. Qt 6.8.3 files were checked in the local
source tree.

Conclusion: Qt pairs `QFoo` with `qfoo.h`, `qfoo.cpp`, `qfoo_p.h` and
`QFooPrivate`, with `_p_p.h` reserved for a genuine second private layer.
HyRemote copies this **naming shape**. It does not copy Qt's ABI strategy: Qt
keeps binary compatibility with a d-pointer, while HyRemote's stability promise
is the one in `docs/v1-api-stability.md`. That document freezes the installed C++
surface for V1 and lists implementation classes as unstable.

HyRemote-side facts below are call-site-confirmed from the current tree. Each
proposal is marked as an inference where a product decision is still open.

## 3. Convention v0.1

| Dimension | Rule | Applies to | Frozen in P0 |
| --- | --- | --- | --- |
| New own public C++ type | `HyFoo`; one principal type per `hyfoo.h` / `hyfoo.cpp` pair | newly created public types | no rename of existing types |
| New own internal C++ type | `HyFoo` with `hyfoo.hpp` / `hyfoo.cpp` | newly created Core and Runtime types | no bulk rename |
| Private implementation | `HyFooPrivate` in `hyfoo_p.h`; `hyfoo_p_p.h` only for a genuine second layer | new pimpl code | yes for existing `detail/` headers |
| Private header exposure | `_p.h` headers are never installed, exported or included by an installed header | all private headers | yes |
| Qt-owned symbols | `QQuickWindow`, `QOpenGLContext`, `QRhi`, `QObject`, `QML_NAMED_ELEMENT`, `Q_PLUGIN_METADATA` and every other Qt type or macro stay as they are | all Qt references | yes, always |
| Namespace | `HyRemote::` for the public C++ surface, `hyremote::` for Core, `HyRemote::detail` / `::Runtime` / `::Qml` / `::Generic` / `::Qpa` for internal layers | new symbols | yes for existing namespaces |
| Namespace convergence | unifying `HyRemote` and `hyremote` needs a source and ABI cost analysis plus a product decision | open question | yes |
| Methods, members, enums | `lowerCamelCase()`, `m_member`, `enum class` with `PascalCase` enumerators | own C++ | matches `naming-conventions.md`, no mechanical change |
| Constants | `HYREMOTE_` for global macros and build options, `kCamelCase` for internal `constexpr` | own C++ and CMake | yes |
| Build contracts | `project(HyRemote ...)`, `hyremote-` targets, `HyRemote::Component` exported targets, `HYREMOTE_` options, `qhyremote` QPA module, `HyRemote/RemoteAccess.h` install path | CMake and package metadata | yes |
| QML | URI `HyRemote`, `QML_NAMED_ELEMENT(RemoteAccess)`, qmldir and plugin registration | declarative surface | yes, no unversioned migration |
| C API, RFB wire, JSON schema | keep published symbols, wire tokens and schema keys | protocol and test clients | yes |
| CTest and evidence identity | keep registered test names and historical evidence labels | tests and archived evidence | yes |
| Include guard | `#pragma once` for own headers; do not mix `#ifndef` guards; keep `Q_OBJECT` files inside an AUTOMOC target | all own headers | yes |
| Case | lower-case `hy*.h`, `hy*.hpp`, `hy*.cpp`; never add two paths that differ only in case | all files | yes |

Windows treats file names as case-insensitive and Linux treats them as
case-sensitive. A rename must never create a case-only pair. The inventory found
zero such collisions today.

## 4. Compatibility mapping

`proposed_name` and `proposed_file` in the inventory are **design targets only**.
They are not approved renames. The 12 required columns are
`current_file`, `current_type`, `namespace`, `layer`, `public_or_private`,
`target_name`, `ABI_impact`, `source_impact`, `QML_impact`, `build_impact`,
`test_impact`, `migration_phase`; six extra columns carry `kind`,
`proposed_name`, `proposed_file`, `name_file_match`, `exemption` and `rationale`.

| Existing | Proposed | Impact | Phase |
| --- | --- | --- | --- |
| `HyRemote::RemoteAccess`, `RemoteAccessState`, `RemoteAccessErrorCode`, `RemoteAccessError`, `RemoteSecurityProfile`, `RemoteWatermark`, `RemoteWatermarkPosition` in `src/integrations/cpp/include/HyRemote/RemoteAccess.h` | `HyRemoteAccess`, `HyRemoteAccessState`, ..., `hyremoteaccess.h` | installed SDK, export symbols, install path, installed consumers, examples and CTest identities | `human-decision` |
| nested `struct Impl` in that header | `HyRemoteAccessPrivate`, `hyremoteaccess_p.h` | private but declared inside an installed header; needs an installed-header review before any change | `P1` candidate |
| `hyremote::CaptureSource` in `src/core/include/hyremote/core/capture_source.hpp` | `HyCaptureSource`, `hycapturesource.hpp` | internal Core ABI across the static library and module tests | `P2` |
| `HyRemote::detail::AdaptiveQuickCaptureSource` in `src/runtime/src/quick/quick_adaptive_capture_source.hpp` | `HyAdaptiveQuickCaptureSource`, `hyadaptivequickcapturesource.hpp` | private Runtime detail, includes, CMake source lists and module tests | `P1`/`P2` |
| `HyRemote::Qml::QmlRemoteAccess` in `src/integrations/qml/QmlRemoteAccess.h` | `HyQmlRemoteAccess`, `hyqmlremoteaccess.h` | the C++ class can move, the QML registration name `RemoteAccess` cannot | `P2` |
| `quick_d3d11_staging_capture_source.hpp`, `quick_gl_pbo_capture_source.hpp`, `automatic_access_config.hpp`, `automatic_access_controller.*` | `hyd3d11stagingcapturesource.h`, `hyglpbocapturesource.h`, `hyaccessconfig.h`, `hyaccesscontroller.h` | file names and principal classes share words but differ in order or carry a module prefix | mapping fix inside `P1`/`P2` |
| `QQuickWindow`, `QObject`, `QRhi`, all 164 Qt types | unchanged | third-party ownership | `exempt-third-party` |
| `HyRemote` URI, `QML_NAMED_ELEMENT(RemoteAccess)` | unchanged | public QML contract | `human-decision` if ever changed |
| `hyremote-*` targets, `HyRemote::*` exports, `HYREMOTE_*` options, `qhyremote`, `-plugin hyremote`, `-platform hyremote`, `hyremote.json` | unchanged | downstream build and deploy contracts | `frozen-contract` |
| `hyremote_test_start`, `hyremote_test_stop`, other C symbols and RFB wire files | unchanged | C ABI, wire protocol and evidence identity | `frozen-contract` |
| 80 registered CTest names | unchanged | test and evidence identity, #970 reuse | `frozen-contract` |

`HyRemote::RemoteAccess` becoming `HyRemote::HyRemoteAccess` in place would break
compiled ABI, existing consumer source, export symbols and examples. The issue
text explicitly rules that out. A bridge type or a new incompatible major version
window is a separate product API decision.

## 5. Exemptions: items that stay unchanged

| Exemption | Count | Reason |
| --- | --- | --- |
| Qt-owned types | 164 | third-party ownership; never wrapped or recast as a fake Qt type |
| Qt-owned plugin macros | 2 | `Q_PLUGIN_METADATA` registration points |
| CMake targets and aliases | 105 | `find_package` and `target_link_libraries` contract |
| `HYREMOTE_` options and macros | 153 | build and downstream contract |
| Export macros | 4 | binary export contract |
| CTest identities | 80 | evidence identity; #970 reuses cases, it does not create one per renamed file |
| C API symbols | 8 | C ABI and consumer probes |
| RFB wire files | 7 | line protocol and historical evidence |
| Installed public header path | 1 | `HyRemote/RemoteAccess.h` |
| QML registrations | 4 | 1 product URI, 1 product element, 2 example/test URIs |
| Plugin tokens and manifests | 4 / 2 | documented zero-code activation |
| Private headers | 34 | currently `detail/` plus nested `Impl`, not `_p.h` |

The two example/test QML URIs (`HyRemoteExample`,
`HyRemoteInstalledConsumer`) are not product public contracts. They are listed in
the inventory for completeness and stay out of the public ABI list.

## 6. Inventory findings

Base is the fixed SHA above. `hy-naming-audit-summary.json` holds the full
machine-readable detail; `hy-naming-inventory.csv` and `.json` hold the rows.

| Metric | Value |
| --- | --- |
| tracked files / scanned files | 676 / 632 |
| inventory rows / columns | 1224 / 18 |
| `src/` C++, CMake, QML and JSON files covered | 183, zero uncovered |
| own C++ types / already `Hy`-prefixed | 227 / 2 |
| distance from the proposed `Hy` prefix | 99.12% |
| product `src/` C++ files / `hy`-named files | 97 / 0 |
| files with a principal class | 39 |
| legacy name-file matches among those | 31 (79.49%) |
| module-prefix mismatches | 8 |
| files without a single principal class | 58 |
| case-insensitive path collisions | 0 |
| private headers / with `_p` suffix | 34 / 0 |
| `#pragma once` headers / missing guards | 62 / 0 |
| `Q_OBJECT` or QML element files / AUTOMOC CMake files | 7 / 4 |
| installed public headers / private-include leaks | 1 / 0 |
| QML registrations / plugin tokens / export macros | 4 / 4 / 4 |
| C API symbols / RFB wire files / install rules | 8 / 7 / 13 |
| namespaces in `src/` | 14 |
| duplicate type names across namespace or layer | 6 |

The 2 existing `Hy`-prefixed types are `HyRemoteGenericPlugin` and
`HyRemotePlatformIntegrationPlugin`. The prefix is therefore not entirely new in
this tree, but it is not a convention yet.

The high violation rate measures distance from a **proposed** convention. It does
not mean the current tree is broken. `naming-conventions.md` already documents a
consistent scheme: `snake_case` internal files with `PascalCase` types, plus the
single installed PascalCase header. 31 of 39 files with a principal class follow
it exactly. The 8 mismatches are word order or module prefixes, listed in
`file_mapping.module_prefix_mismatch_examples`. The 58 files without a principal
class are policy headers, free-function implementations, detail helpers and
records with no single owning type.

Six names appear in more than one namespace or layer: `AccessInstance`, `Clock`,
`ErrorCode`, `QueuedInput`, `SecurityProfile`, `ViewerCaptureDemand`. Namespace
separation resolves these; ad-hoc renaming to avoid a clash is not the fix.

All 34 private headers use `detail/` or a nested `Impl`, and none uses `_p.h`.
The `_p.h` rule is new. Turning the existing 34 headers into `_p.h` files would
be a bulk rename and is out of P0 scope.

Rows by migration phase: `P1` 89, `P2` 301, `P3` 10, `human-decision` 31,
`frozen-contract` 627, `exempt-third-party` 166. The 31 `human-decision` rows are
the 7 installed public types, 14 namespaces, 4 QML registrations, 4 plugin
tokens and 2 plugin manifests. CMake options and targets are classified as
frozen contracts, not as open product decisions.

## 7. Three migration boundaries

Counts are inventory rows, not files. Risk and cost are qualitative.

| Boundary | Rows | ABI | Source | QML | Build | Test | Risk | Cost |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A: full breaking rename | 894 | breaks installed SDK, export symbols and C ABI | breaks every consumer and example | breaks URI and type registration | renames targets, options and plugin tokens | breaks 80 evidence identities | unacceptable inside V1.x; invalidates #962/#966 evidence | one giant diff, every component, a new incompatible major version window |
| B: public compatible, internal first | 400 | no public ABI change | internal includes and CMake source lists | no URI change | per-component CMake and MOC updates | existing CTest identities reused | low to medium; each batch needs an affected build | several small PRs, staged by component |
| C: new files only | 0 | none | none | none | none | none | near zero | only the convention document and a report-only lint gate |

Recommended executable default for the next task: **C now, B as the staged
plan**. Apply the convention to newly created files, prepare one small P1
demonstration limited to private internal classes that are not installed, not
QML-registered and not C ABI, and leave all public SDK, QML URI, plugin and
namespace questions as separate product decisions. This is a recommendation from
the local self-review. Cloud Owner technical review freezes the default.

## 8. Phased plan

- **P0 (this turn).** Fixed-SHA read-only inventory, this convention draft,
  compatibility mapping, exemptions, boundary comparison, self-review, issue
  archive and an independent docs PR. No product file changes.
- **P1 (needs a freeze and a separate task).** Enable the convention for new
  files and a report-only lint. Select one small group of private internal
  classes, handle includes, CMake source lists, MOC and associated tests, and
  submit one small PR. Use the #970 planner to pick affected CMake+MOC,
  Qt5/Qt6 builds and installed consumers from the real diff. Do not add a new
  CTest or a one-off script per renamed file.
- **P2 (after P1 is green).** Batch by component: Core, Runtime, Widgets/Quick,
  transport, QPA/QML. No giant PR. Public SDK renames require a separate
  compatibility design and review.
- **P3 (only when a rename actually lands).** Windows Qt 5.15.2 and 6.8.3
  Release plus public-only and experimental ON/OFF, Linux Qt6 native
  compile/ctest, installed C++/QML/QPA/Generic consumers, SDK DLL export symbols
  and object sizes, case-sensitive include checks. Historical #962/#966
  performance evidence must not be polluted by a naming change.

## 9. Minimal lint and test gate (draft, not registered)

Applies only to newly added or newly migrated files.

Checks: own type starts with `Hy`; file is `hy<lowercase>.h/.hpp/.cpp`; private
header uses `_p.h` and is never installed; Qt types are untouched; no case-only
path pair; `#pragma once`; every `Q_OBJECT` file belongs to an AUTOMOC target;
no installed header includes a `detail/` or `_p.h` header.

Mode: warning and report-only first. Promote to a hard gate only in a later
phase, after a green affected build. #970 decides which CMake+MOC, Qt5/Qt6 and
installed-consumer cases a rename actually needs; only public ABI, QML, plugin
or install-path changes escalate qualification.

P0 registers no CI step and no CTest. #972 owns `.github/` and #975 owns `tests/`
and `tools/test/`. `check_script_discipline.cmake` requires every tracked `.py`,
`.ps1`, `.cmd`, `.bat` or `.sh` to be declared in
`tests/release-readiness/script_inventory.txt`. That register sits in #975's
`tests/` scope, so this read-only turn adds no tracked executable script. The
generator source is archived as data inside `hy-naming-inventory.json` instead.

## 10. Script contract and reproduction

| Field | Value |
| --- | --- |
| Name | `docs/internal/scan_hy_naming_inventory.py` (P0 generator, not committed) |
| Purpose | read-only inventory, `existing -> proposed` mapping and machine checks for the P0 decision |
| Inputs | `--root`, `--base-sha`, `git ls-files`; no writes to product files, no paths outside the checkout |
| Outputs | `docs/internal/hy-naming-inventory.csv`, `.json`, `hy-naming-audit-summary.json` |
| Reuse check | 24 existing tracked Python scripts cover performance, release policy and consumer checks; none performs a naming inventory; `docs/check_ai_flavor.py` checks prose only |
| Registration | none in P0; `TEST_PLAN=READ_ONLY`, and a tracked `.py` would require editing the #975-owned register |
| Run count | one `--self-test` and one real scan |
| PASS | 12 required columns, zero uncovered `src/` files, self-test passes, no absolute local path in any output |

Self-test result: 30 positive and 5 negative checks passed. Scan result: 1224
rows, 183 covered `src/` files, zero uncovered, zero case collisions, no
absolute local path in the data fields.

The full generator source is stored in
`hy-naming-inventory.json` under `generator_source` (40786 characters). To
inspect it without creating a file:

```text
python -c "import json; print(json.load(open('docs/internal/hy-naming-inventory.json'))['generator_source'])"
```

A later task that wants a permanent tool must first agree the ownership with
#975, place the script in a planned tracked directory, add one line to
`tests/release-readiness/script_inventory.txt`, and only then re-run it with the
same fixed SHA plus `--self-test`. Do not put the extracted script in `tmp/`.

## 11. Self-review

Evidence levels are explicit: Qt naming claims are source-confirmed at exact
versions; HyRemote facts are call-site-confirmed from the fixed SHA; migration
costs and the recommended default are labelled inference and left for Cloud
Owner review.

Scope was checked before work: #967 is complete, #970/#975 owns `tools/test/`,
`tests/` and the affected-case planner, #972 owns `.github/`, and the latest #969
comment limits this turn to `docs/internal/` outputs with
`TEST_PLAN=READ_ONLY`. No overlapping open PR claims #969 P0.

API and ABI safety: every installed public type, QML registration, plugin token
and namespace is marked `human-decision`; every build, test, wire and C ABI
identity is marked `frozen-contract`; all Qt symbols are exempt. No public
surface was touched.

Data quality: the 12 required columns are present and non-empty in all rows, all
183 `src/` C++/CMake/QML/JSON files appear in the inventory, all paths are
repo-relative POSIX, and the generator has a deterministic positive and negative
self-test.

Known limitations: the inventory is regex-based static text, not a compiler or
AST. Nested, anonymous and template-heavy declarations can be under-counted.
Generated `moc` output, `qmldir` and `RemoteAccessExport.h` do not exist in git
and were not parsed. Qt 5.15.2 facts were checked through the upstream tag
because the local Qt 5 installation has headers only. No Qt5/Qt6 compile,
MOC run, install-tree check or consumer build was performed. Those are P3 work
and are labelled `NOT_RUN`, never PASS.

A governance finding worth carrying forward: adding any tracked executable
script requires an entry in the #975-owned script register. A future P1 task
that adds the naming lint must settle that registration in the same change.

## 12. Acceptance criteria for this P0 delivery

- Diff is limited to `docs/internal/`; no product file, public API, ABI, QML URI,
  CMake target or test identity changed.
- `TEST_PLAN=READ_ONLY` is published on #969 and no Qt build, CTest or RealVNC
  matrix was started.
- The inventory is machine-readable, reproducible from the archived generator
  source and tied to one fixed SHA.
- The three migration boundaries, exemptions and phased plan are recorded for
  review, with no rename executed.
- An independent documentation PR carries the deliverables.

Open decisions for Human / Cloud Owner: which boundary to freeze, whether the
public SDK ever changes, and whether the two namespaces converge. P0 does not
answer these and does not act on them.
