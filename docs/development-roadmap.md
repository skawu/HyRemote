# HyRemote Development Roadmap

Status: execution roadmap toward **V1.0.0.0**.

This document orders product work by externally usable milestones. Internal modules are WBS under the milestone they enable; they are not product milestones by themselves.

## V0.0.1.0 — x86_64 Windows + Linux / Embedded C++ API

Product acceptance authority: #30.

Execution order:

1. **Core closure** — #21 / PR #22
   - close the remaining Round-3 lifecycle regression;
   - merge the stable transport-neutral Session / RemoteFrame / bounded dispatch contract.
2. **SDK/public product boundary** — #39
   - install/export package;
   - `find_package(HyRemote CONFIG REQUIRED)`;
   - product target `HyRemote::RemoteAccess`;
   - prebuilt SDK and source-consumption paths;
   - deployment helper and clean external consumer.
3. **Target adapters**
   - #6 Qt Widgets C++ path;
   - #28 Qt Quick C++ path.
4. **Remote input** — #29
   - normalized input schema;
   - Widgets/Quick target-thread delivery;
   - view-only/control policy.
5. **VNC/RFB transport** — #27
   - Windows + Linux production backend strategy;
   - safe bind/start/stop/reconnect;
   - bounded downstream behavior;
   - backend hidden by the SDK contract.
6. **Build/package/compatibility** — #11
   - clean Windows + Linux build/install/package evidence;
   - external consumer smoke;
   - supported Qt range recorded.
7. **Examples/docs required for V0.0.1.0** — #41 subset
   - `widgets-basic`;
   - `quick-basic`;
   - Windows/Linux getting started;
   - SDK/source consumption;
   - deployment/viewer/security/troubleshooting docs.
8. **V0.0.1.0 acceptance** — #30.

Performance/hardware work such as DMA-BUF, H.264, RKMPP or embedded EGLFS does not block this milestone unless functional evidence proves it is required.

## V0.0.2.0 — x86_64 Windows + Linux / Declarative QML API

Product acceptance authority: #31.

- expose `import HyRemote` over the same runtime/Core;
- target a compact `RemoteAccess { target: ... }` experience;
- parity with the accepted Qt Quick C++ path;
- add `qml-basic` plus full QML user guide;
- no duplicate capture/transport/input stack.

## V0.0.3.0 — x86_64 Windows + Linux / Transparent QPA Proxy

Product acceptance authority: #32.

- mandatory V1.0 integration mode;
- no/minimal application source changes;
- preserve native local display and input while adding remote view/control;
- implement a version-coupled native-QPA proxy/decorator, not a replacement-only `qvnc` clone;
- isolate all Qt private/QPA dependencies from Core and C++/QML public APIs;
- freeze exact supported Qt/OS combinations from evidence;
- provide `qpa-proxy-existing-app` proving local + remote coexistence.

## V1.0.0.0 — x86_64 GA

Product acceptance authority: #33.

All three modes are mandatory on Windows x86_64 and Linux x86_64:

1. Embedded C++ API;
2. Declarative QML API;
3. Transparent QPA Proxy.

GA additionally requires #39 SDK/product consumption completion and #41 richer examples/documentation completion.

## Post-GA

- `V1.x.0.0`: formal embedded platform-family expansion, Embedded Linux first;
- Rockchip and NXP i.MX remain the current high-priority families;
- OpenHarmony is a long-term embedded OS direction and does not block x86 GA or initial Embedded Linux platform work.

## Execution rule

- GitHub Issue / PR / machine evidence is canonical.
- Work that can be completed through repository/tooling access is performed directly.
- Local Agent assistance is limited to genuine local-only build/runtime/device evidence or local working-tree state that is unavailable through repository tooling.
- Examples and documentation are acceptance artifacts, not post-release polish.
