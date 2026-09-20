# HyRemote V1.0.0.0 Example Strategy

This document records the V1 example information architecture owned by #41.

## Integration terminology

HyRemote has three application integration modes and one internal base:

```text
src/core/  -> internal base, not an application integration mode
src/cpp/   -> C++ API integration, HyRemote::RemoteAccess + the one shared runtime
src/qml/   -> QML API integration, import HyRemote over the same runtime
src/qpa/   -> QPA platform integration, qhyremote over the same runtime
```

Use the Qt/industry terms **C++**, **QML** and **QPA** in user-facing architecture and examples. Avoid using `embedded`, `declarative` or `transparent` as source-directory names; those words describe qualities, while C++/QML/QPA identify the actual integration technology. In particular, `embedded` is reserved for future embedded-system/platform discussion and must not mean the desktop C++ API.

Qt Widgets and Qt Quick are UI technology families. QML is the primary declarative language used by Qt Quick and is also the surface for HyRemote's QML API. Therefore Quick + C++ and Quick + QML are two HyRemote integration modes over the same Qt Quick UI family.

## Example topology

```text
examples/
  learning/
    01-widgets-basic/
    02-widgets-control/
    03-quick-cpp/
    04-quick-qml/
    05-qpa-existing-app/
      widgets-app/
      quick-app/
    06-session-security/
    07-production-showcase/

  realworld/
    qbittorrent/
    musescore/
```

The physical migration lands once with #209. Do not keep old and new example taxonomies in parallel.

## Self-authored learning path

### 01-widgets-basic
Qt Widgets UI + HyRemote C++ API. Smallest readable `HyRemote::RemoteAccess` start/stop example.

### 02-widgets-control
Qt Widgets UI + HyRemote C++ API. Adds remote-control policy, pointer/keyboard/text, reconnect, resize/DPR and lifecycle cleanup.

### 03-quick-cpp
Qt Quick UI authored in QML + HyRemote C++ API against `QQuickWindow`. It exists specifically to show that a QML-authored UI does not require the HyRemote QML API.

### 04-quick-qml
The same Qt Quick/QML UI family + HyRemote QML API via `import HyRemote` and `RemoteAccess { ... }`.

The README comparison is explicit:

```text
03 = Qt Quick/QML UI + HyRemote C++ API
04 = Qt Quick/QML UI + HyRemote QML API
```

### 05-qpa-existing-app
Two ordinary Qt-only fixtures, one Widgets and one Qt Quick/QML. Neither links HyRemote application APIs. Both demonstrate `-platform hyremote` and `hyremote_deploy(... QPA)`.

### 06-session-security
Simple Widgets UI + HyRemote C++ API for security profiles, IPv4 bind policy, authenticated sessions/events and targeted termination.

### 07-production-showcase
Polished Qt Quick/QML product demo using the HyRemote QML API over the shared runtime.

## Qt coverage

The controlled examples carry the complete V1 Qt compatibility responsibility:

- Qt 5.15 LTS: required where applicable;
- Qt 6.8 LTS: required where applicable;
- Qt 6.8.3: complete primary GA/reference candidate;
- QPA: exact Qt patch/private ABI qualification.

There is one source tree per logical example. Do not create `examples-qt5` / `examples-qt6` trees or version-specific public APIs.

## Real-world high-star verification

V1 keeps two representative projects:

- `qbittorrent/qBittorrent` -> Qt Widgets representative, pristine QPA integration, reuse #137;
- `musescore/MuseScore` -> Qt Quick/QML representative, pristine QPA-first integration, reuse #138 build material where useful.

These projects retain upstream source and branding. Every result is labelled `third-party verification example — not a HyRemote compatibility/support claim`.

Qt 5.15 support is not forced through old upstream project history; it is proven by the controlled learning examples and #57 qualification lanes.

## Branding ownership

Project branding has one repository-owned source at the repository root:

```text
logo/
  huayan-logo-single.png
  huayan-software-horizontal.png
```

Rules:

- `logo/huayan-logo-single.png` is the canonical application/window logo for HyRemote-authored GUI examples unless a later explicit branding decision replaces it;
- documentation and examples reference the root `logo/` assets rather than owning copies;
- no per-example copied logo files;
- packaging/deployment must allow self-authored examples to resolve the canonical logo without depending on the source tree;
- qBittorrent and MuseScore retain upstream branding and are never rewritten with HyRemote assets.

The current files under `docs/assets/logo/` are migration sources only. #209 performs one history-preserving move to root `logo/` and removes the stale documentation-owned copies.

## Release evidence mapping

- E1 -> 01/02 Widgets + C++ API;
- E2 -> 03 Qt Quick + C++ API;
- E3 -> 04 Qt Quick + QML API;
- E4 -> 05 QPA Widgets + Quick fixtures;
- E5 -> 06/07 operational/showcase;
- E6 -> installed SDK consumer under `tests/`, not duplicated under examples.

R1/R2 real-world verification complement but do not replace #109 controlled physical acceptance.

## Foundation rule

The source-directory terminology, example topology and logo ownership are part of the one-time #209 V1 foundation migration. After that migration, do not reopen the basic repository architecture merely to add a platform, Qt LTS family or product feature.

Refs: #33 #41 #57 #109 #134 #137 #138 #160 #165 #176 #209.
