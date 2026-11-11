#!/usr/bin/env python3
"""Patch only the Qt-only test App's QML in ephemeral CI, never product Runtime.

The center marker toggles 1 Hz while corners retain ordinary app background.
Thus real VNC screenshots can prove more than a static bootstrap frame, without
requiring the application to link or import any HyRemote code.
"""
from pathlib import Path
import subprocess
import sys

QML = Path("examples/learning/03-generic/quick-app/Main.qml")
MARKER = """\n    // R4 test-only: independent Qt Quick source scene animation, no HyRemote API.
    Rectangle {
        id: dynamicProbe
        x: 205
        y: 112
        width: 80
        height: 56
        property int phase: 0
        color: phase === 0 ? "#df2534" : "#25d242"
        Timer {
            interval: 1000
            running: true
            repeat: true
            onTriggered: {
                dynamicProbe.phase = 1 - dynamicProbe.phase
                console.log("HYREMOTE_R4_QML_SOURCE_PHASE=" + dynamicProbe.phase)
                console.log("HYREMOTE_R4_QML_SCREEN_DPR=" + Screen.devicePixelRatio)
            }
        }
    }\n"""

def main():
    source = QML.read_text(encoding="utf-8")
    if "id: dynamicProbe" in source:
        raise RuntimeError("Source QML already patched; refuse non-identical fixture")
    if not source.startswith("// Plain Qt Quick UI") or not source.rstrip().endswith("}"):
        raise RuntimeError("Unexpected base learning example: fail source lock")
    # Pure Qt attached Screen property; confirms requested test scale
    # actually reaches the QQuickWindow rather than trusting the environment.
    source = source.replace("import QtQuick\\n", "import QtQuick\\nimport QtQuick.Window\\n", 1)
    target = source.rstrip()
    target = target[:-1] + MARKER + "}\n"
    QML.write_text(target, encoding="utf-8")
    changes = subprocess.check_output(
        ["git", "diff", "--name-only"], text=True).splitlines()
    if changes != [QML.as_posix()]:
        raise RuntimeError(f"Fixture touched non-QML product paths: {changes}")
    print("R4_QT_ONLY_DYNAMIC_FIXTURE=PASS")
    print("R4_FIXTURE_FILES=" + ";".join(changes))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print("R4_QT_ONLY_DYNAMIC_FIXTURE=BLOCKED: " + str(exc), file=sys.stderr)
        sys.exit(1)
