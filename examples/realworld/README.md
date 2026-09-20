# Real-world verification examples

This directory complements the controlled `../learning/` examples with two representative, high-star Qt applications whose upstream sources remain pristine.

These are **third-party verification examples — not HyRemote compatibility/support claims**.

| Representative | UI family | Qt line used by the V1 verification | HyRemote integration | V1 role |
| --- | --- | --- | --- | --- |
| `qbittorrent/` | Qt Widgets | Qt 6.8.3 | QPA | prove low-intrusion integration into a mature Qt 6 Widgets application |
| `musescore/` | Qt Quick/QML | Qt 5.15 | QPA | prove low-intrusion integration into a mature Qt 5 Quick/QML application once #57's Qt 5.15 QPA lane exists |

The split is intentional. Controlled examples own the complete Qt 5.15 + Qt 6.8 product compatibility matrix. These two projects provide representative real-world pressure without forcing current upstream projects onto artificial Qt versions.

Rules:

- never vendor or patch third-party source in this repository;
- resolve the recorded upstream revision into an external/ignored work directory;
- keep upstream branding unchanged;
- HyRemote integration lives outside upstream source;
- QPA/private ABI must match the exact Qt patch used to build/run the application;
- record build, launch, framebuffer, input/reconnect and cleanliness evidence without turning the result into a named support promise.

Refs: #41 #57 #109 #134 #137 #138 #209.
