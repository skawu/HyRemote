# MuseScore real-world verification

**Role:** Qt Quick/QML / Qt 5.15 representative.

MuseScore's current releases have moved beyond the V1 Qt 6.8 reference line, so the V1 representative intentionally uses the last Qt-5-era MuseScore Studio 4.3 line rather than loading a Qt 6.8 QPA plugin into a Qt 6.9/6.10 application.

## Frozen upstream input

- repository: `https://github.com/musescore/MuseScore.git`
- release tag: `v4.3.2`
- release commit: `22b46f27e224cd38a0100be44409e648575d1931`
- UI family: Qt Quick/QML at product scale
- Qt family: **5.15**
- exact Qt 5.15 patch: frozen by #57 before executable QPA qualification
- integration: pristine application + externally deployed `-platform hyremote`

The release commit is the upstream v4.3.2 release revision. This lane deliberately waits for HyRemote's real Qt 5.15 QPA adapter/packaging from #57; it must not use a fabricated compatibility shim or mix private QPA ABIs merely to turn the check green.

## Contract

MuseScore source and branding remain untouched. HyRemote-owned harness code may fetch/build/materialize the pinned source in ignored external build space, deploy the exact-patch QPA payload, launch it and probe the running application, but may not patch upstream source as part of the claimed verification.

Minimum final evidence after #57 lands:

1. resolve and record the exact Qt 5.15 patch used for both MuseScore and `qhyremote`;
2. build/materialize the pinned MuseScore source with its required recursive dependencies;
3. verify the upstream checkout is clean before integration;
4. deploy the matching HyRemote runtime/QPA payload externally;
5. launch with `-platform hyremote` while preserving normal local UI behavior;
6. validate representative Quick/QML framebuffer, resize, reconnect and applicable input behavior;
7. verify upstream cleanliness after the run.

If a real Qt 5.15 QPA private-API blocker requires changing the one-runtime/native-delegate architecture, escalate under #57 as a Human product decision rather than weakening this example.

**This is a third-party verification example, not a HyRemote support claim for MuseScore.**

Refs: #41 #57 #109 #134 #138 #176 #209.
