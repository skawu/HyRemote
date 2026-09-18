# Real-world Qt application test suite

This directory owns HyRemote's real-world application compatibility evidence. The applications under `apps/` are upstream git submodules and **must remain pristine upstream source trees**.

Passing a row in this suite is test evidence. It is not, by itself, a named compatibility/support claim.

## Why five applications

The suite deliberately covers different failure surfaces rather than collecting projects by star count alone:

| Application | Primary role | Tier |
| --- | --- | --- |
| DB Browser for SQLite | lightweight deterministic Widgets / Embedded C++ baseline | L1 core |
| qBittorrent | complex Widgets, menus/dialogs/text/secondary windows, primary pristine-QPA baseline | L1 core |
| MuseScore | complex Qt Quick/QML application | L2 real-world |
| Shotcut | mixed Widgets + Qt Quick/QML + multimedia/dynamic surfaces | L2 real-world |
| OBS Studio | graphics/video/Dock/native-surface stress and boundary classification | L3 heavy |

QGIS and Krita remain valid ad-hoc stress candidates but are not continuously tracked because their repository/dependency footprint is disproportionate for the permanent suite.

## Qt-version model

HyRemote has two different compatibility problems and the test matrix keeps them separate.

### Public C++ / QML API lanes

The product requires Qt 6.8 or newer. Public-API build/behavior probes therefore run on multiple Qt 6 lines:

- `qt68-ref`: **6.8.3 exact**, the frozen V1 reference and release evidence line;
- `qt68-lts-latest`: latest available Qt 6.8 commercial LTS patch (currently recorded as 6.8.9); run only where a licensed SDK is available;
- `qt610`: Qt 6.10.3 forward-compatibility line;
- `qt611`: Qt 6.11.2 current stable forward-compatibility line;
- `qt612-preview`: optional preview canary, never release-blocking.

Qt < 6.8 is outside the product minimum and is not turned into a compatibility promise merely for test breadth.

### QPA lanes

QPA uses Qt private ABI. A `qhyremote` built against one Qt patch/minor **must never be reused as evidence for another Qt version**.

For V1, only Qt **6.8.3 exact** is qualified. A future QPA lane for 6.8.9, 6.10.3, 6.11.2 or later must build `qhyremote` against that exact SDK/private headers and remains Experimental/Unverified until a compatibility authority explicitly promotes it. The multi-version public-API workflows intentionally keep QPA disabled outside 6.8.3.

## Upstream cleanliness contract

1. Never patch files inside `apps/*`.
2. Never generate build output, test data or HyRemote glue inside a submodule.
3. Embedded C++ / QML integration is applied only to a disposable materialized source tree under the runner/build directory.
4. Transparent QPA uses the pristine upstream application and the normal installed/deployed `-platform hyremote` path.
5. Every applicable job ends with `tools/third_party/verify_clean.py`.
6. A dirty submodule, a submodule HEAD different from the superproject gitlink, or a mismatched `.gitmodules` contract is a test-infrastructure failure.

## Reproducibility and upstream tracking

The superproject gitlink is the reproducible tested input. `.gitmodules` records the upstream tracking branch only so automation can discover newer commits.

A single upstream HEAD is **not** assumed to build with every Qt lane. `matrix.json` and each application spec may pin a different previously-qualified upstream ref for a Qt lane. The normal mechanism is:

1. keep the submodule checkout at the superproject gitlink;
2. fetch a required historical/tagged ref into the submodule object database when needed;
3. use `tools/third_party/materialize.py` to archive that ref into a disposable source directory;
4. apply HyRemote overlays only to that disposable directory;
5. leave the submodule checkout unchanged and clean.

Scheduled update automation may move tracking gitlinks only in a reviewable PR. It never auto-merges an upstream update.

## Execution tiers

### L1 — core / PR

SQLiteBrowser + qBittorrent, Windows x86_64 + Linux x86_64, Qt 6.8.3. These are the first third-party rows to become blocking once their actual build/view/input recipes are accepted.

### L2 — version matrix / RC

Public C++/QML product compile probes across 6.8.3, 6.10.3 and 6.11.2; then application scenarios for core apps plus MuseScore and Shotcut on version/app combinations explicitly marked applicable in the specs.

### L3 — heavy / nightly-weekly + RC

OBS plus Shotcut multimedia/graphics stress. Execution is required when enabled, but results are classification-aware: a failure in an already-promised Widgets/Quick surface is a HyRemote defect; foreign-native/OpenGL/video behavior outside the frozen product boundary remains an observation/limitation.

### Licensed LTS and preview lanes

Latest 6.8 commercial LTS runs only on licensed infrastructure; unavailable SDK means `SKIP`, not failure. Qt 6.12 preview is a canary only.

## Scenario envelope

Required application rows eventually cover: clean build/deploy, native local baseline, remote view-only, framebuffer changes, pointer, keyboard, text, menus/dialogs/secondary windows where present, resize/DPR where relevant, reconnect, abrupt disconnect while a supported input is held, explicit stop/policy transition, no late remote input after stop, local input/rendering coexistence, normal teardown and final submodule cleanliness.

## Current bootstrap state

The initial #134 implementation establishes submodules, version/application manifests, clean materialization, cross-version HyRemote build probes and update automation. Application-specific dependency/build recipes and physical viewer scenarios are added only after each upstream ref/Qt-lane pair is actually proven; the manifest uses `unresolved` rather than inventing compatibility.

The existing self-owned E1-E4 acceptance surface remains normative for V1 product semantics. This suite increases real-world confidence without silently certifying the five named applications.
