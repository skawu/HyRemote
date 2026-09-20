# HyRemote V1 examples

The V1 example system has two responsibilities:

1. **Learning examples** teach the supported HyRemote integration paths from beginner to advanced.
2. **Real-world verification examples** demonstrate low-intrusion integration with representative high-star Qt applications while keeping upstream source and branding pristine.

Qt Widgets and Qt Quick are the two UI technology families. QML is the primary declarative language for Qt Quick and is also HyRemote's declarative API surface. Therefore `03-quick-cpp` and `04-quick-qml` use the same Qt Quick/QML UI family but different HyRemote APIs.

## Learning path

```text
learning/
  01-widgets-basic/               # Qt Widgets + C++ API, minimal onboarding
  02-widgets-control/             # Qt Widgets + C++ API, input/reconnect/lifecycle
  03-quick-cpp/                   # Qt Quick/QML UI + C++ API
  04-quick-qml/                   # Qt Quick/QML UI + QML API
  05-qpa-existing-app/
    widgets-app/                  # ordinary Qt Widgets app + -platform hyremote
    quick-app/                    # ordinary Qt Quick app + -platform hyremote
  06-session-security/            # security profiles, sessions, bind policy
  07-production-showcase/         # polished product-level showcase
```

All HyRemote-authored GUI examples use the canonical project mark from the root `logo/` directory through the supported resource/deployment mechanism. No example owns a copied logo file.

The controlled learning examples carry the Qt compatibility responsibility:
- Qt 5.15 LTS where applicable;
- Qt 6.8 LTS where applicable;
- Qt 6.8.3 as the complete primary GA/reference candidate;
- QPA exact-patch/private-ABI qualification.

Do not create parallel `examples-qt5` / `examples-qt6` trees.

## Real-world verification

```text
realworld/
  qbittorrent/    # Qt Widgets representative
  musescore/      # Qt Quick/QML representative
```

These directories contain HyRemote-owned manifests, instructions and bounded integration harnesses only. They do not vendor the upstream projects.

Rules:
- upstream source remains pristine;
- upstream branding remains unchanged;
- QPA/low-intrusion integration is preferred;
- results are labelled **third-party verification example — not a HyRemote compatibility/support claim**;
- qBittorrent reuses the proven #137 path;
- MuseScore reuses #138 build knowledge where useful and receives a bounded Qt 6.8 verification lane.

Qt 5.15 support is proven by the controlled learning examples and compatibility tests, not by forcing current third-party applications onto historical Qt versions.

## Release evidence mapping

- E1 -> `01-widgets-basic` + `02-widgets-control`;
- E2 -> `03-quick-cpp`;
- E3 -> `04-quick-qml`;
- E4 -> both `05-qpa-existing-app` fixtures;
- E5 -> `06-session-security` / `07-production-showcase` as applicable;
- E6 -> `../tests/consumer-installed-sdk`, intentionally not duplicated here.

Examples are evidence and education surfaces, not alternate runtimes. They consume only the public HyRemote application surfaces appropriate to their integration mode.

Refs: #33 #41 #57 #109 #134 #137 #138 #176 #209.
