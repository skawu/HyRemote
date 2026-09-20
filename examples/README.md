# HyRemote V1 examples

The V1 example system has two responsibilities:

1. **Learning examples** teach the supported HyRemote integration paths from beginner to advanced.
2. **Real-world verification examples** demonstrate low-intrusion integration with representative high-star Qt applications while keeping upstream source and branding pristine.

Qt Widgets and Qt Quick are the two UI technology families. QML is the primary declarative language for Qt Quick and is also HyRemote's declarative API surface. Therefore `03-quick-cpp` and `04-quick-qml` use the same Qt Quick/QML UI family but different HyRemote APIs.

## Learning path

| Step | Directory | Target / executable | UI technology | HyRemote integration |
| --- | --- | --- | --- | --- |
| 01 | `learning/01-widgets-basic/` | `hyremote-example-widgets-basic` | Qt Widgets | C++ API, minimal onboarding |
| 02 | `learning/02-widgets-control/` | `hyremote-example-widgets-control` | Qt Widgets | C++ API, control/reconnect/lifecycle |
| 03 | `learning/03-quick-cpp/` | `hyremote-example-quick-cpp` | Qt Quick/QML | C++ API |
| 04 | `learning/04-quick-qml/` | `hyremote-example-quick-qml` | Qt Quick/QML | QML API |
| 05a | `learning/05-qpa-existing-app/widgets-app/` | `hyremote-example-qpa-existing-widgets` | Qt Widgets | QPA (`-platform hyremote`) |
| 05b | `learning/05-qpa-existing-app/quick-app/` | `hyremote-example-qpa-existing-quick` | Qt Quick/QML | QPA (`-platform hyremote`) |
| 06 | `learning/06-session-security/` | enabled after #170 | product API | Session/security advanced example |
| 07 | `learning/07-production-showcase/` | `hyremote-example-production-showcase` | Qt Quick/QML | QML API, product-level showcase |

The directory number communicates learning order; executable names communicate technology and remain independent of that ordering.

All HyRemote-authored GUI examples use the canonical project mark from the root `logo/` directory through `examples/common`. No example owns a copied logo file.

The controlled learning examples carry the formal compatibility responsibility:

- Qt 5.15 LTS where applicable;
- Qt 6.8 LTS where applicable;
- Qt 6.8.3 as the complete primary GA/reference candidate;
- exact-patch/private-ABI qualification for QPA.

Do not create parallel `examples-qt5` / `examples-qt6` trees. #57 owns the real Qt 5.15 adaptation and executable evidence where Qt/CMake/QML private/public APIs differ.

## Real-world verification

```text
realworld/
  qbittorrent/    # Qt Widgets representative
  musescore/      # Qt Quick/QML representative
```

These directories contain HyRemote-owned manifests, instructions and bounded integration harnesses only. They do not vendor upstream source.

The V1 representatives are deliberately different generations:

- **qBittorrent**: Qt 6.8.3 / Widgets / QPA, reusing the already proven #137 path;
- **MuseScore 4.3.2**: Qt 5.15 / Quick-QML / QPA, activated when #57's exact Qt 5.15 QPA lane exists.

This does not make those projects support promises and does not replace the controlled Qt 5.15 + Qt 6.8 compatibility matrix. It adds representative real-world pressure on both UI families and both supported Qt generations.

Rules:

- upstream source remains pristine;
- upstream branding remains unchanged;
- integration lives outside upstream source;
- QPA/private ABI matches the exact Qt patch used to build/run the application;
- results are labelled **third-party verification example — not a HyRemote compatibility/support claim**.

## Release evidence mapping

- E1 -> `01-widgets-basic` + `02-widgets-control`;
- E2 -> `03-quick-cpp`;
- E3 -> `04-quick-qml`;
- E4 -> both `05-qpa-existing-app` fixtures;
- E5 -> `06-session-security` / `07-production-showcase` as applicable;
- E6 -> `../tests/consumer-installed-sdk`, intentionally not duplicated here.

Examples are education and evidence surfaces, not alternate runtimes. They consume only the public HyRemote surface appropriate to their integration mode.

Refs: #33 #41 #57 #109 #134 #137 #138 #170 #176 #209.
