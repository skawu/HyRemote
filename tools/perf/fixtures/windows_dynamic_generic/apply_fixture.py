#!/usr/bin/env python3
"""Patch *application fixtures only* in pinned temporary Git worktrees.

This is NOT product source code and must never be applied to develop or a
release branch. Same test-only file bytes on old/new Runtime revisions.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

APP = Path("examples/learning/03-generic/quick-app")
OLD_RESIZE = "view.resize(460, 190);"
NEW_RESIZE = "view.resize(1920, 1080);"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def patch(tree: Path, fixture: Path, manifest: Path):
    tree = tree.resolve()
    app = tree / APP
    original_main = (app / "main.cpp").read_bytes()
    # Normalize newline differences only for the fixture edit. Source SHA is
    # separately fixed and verified by the caller before this operation.
    original_text = original_main.decode("utf-8")
    assert original_text.count(OLD_RESIZE) == 1, (
        "historical app main.cpp no longer has the expected 460x190 viewport"
    )
    # A C++ QTimer, not the Qt logging subsystem, produces an ASCII stage
    # line on stdout after updating the QML property. std::endl flushes it so
    # an independent Windows Viewer pixel observer can correlate UTC clocks.
    import_line = "#include <QGuiApplication>"
    assert original_text.count(import_line) == 1
    altered = original_text.replace(
        import_line,
        import_line + "\n#include <QDateTime>\n#include <QQuickItem>",
    )
    show_line = "    view.show();"
    assert altered.count(show_line) == 1
    stage_driver = r'''
    view.show();

    int fixturePhase = 0;
    const auto publishFixturePhase = [&] {
        std::cout << "HYREMOTE_DYNAMIC_STAGE_JSON={\"phase\":" << fixturePhase
                  << ",\"utc_ms\":" << QDateTime::currentMSecsSinceEpoch()
                  << ",\"source\":\"QTimer_property_before_render\"}" << std::endl;
    };
    publishFixturePhase();
    QTimer fixtureStageTimer;
    QObject::connect(&fixtureStageTimer, &QTimer::timeout, &view, [&] {
        fixturePhase = 1 - fixturePhase;
        if (view.rootObject()) {
            view.rootObject()->setProperty("phase", fixturePhase);
            publishFixturePhase();
        }
    });
    fixtureStageTimer.start(1200);
'''.strip("\n")
    altered = altered.replace(show_line, stage_driver)
    adjusted = altered.replace(OLD_RESIZE, NEW_RESIZE).encode("utf-8")
    qml = fixture.read_bytes()
    assert b"phase" in qml and b"#173cdb" in qml and b"#e8263b" in qml
    assert b"1920" in qml and b"1080" in qml
    (app / "main.cpp").write_bytes(adjusted)
    (app / "Main.qml").write_bytes(qml)
    # Git audit in bytes: stderr CRLF conversion warnings MUST NOT become
    # filenames. No other tracked source may be changed by the diagnostic.
    audited = subprocess.run(
        ["git", "diff", "--name-only", "-z", "--no-ext-diff"],
        cwd=tree, capture_output=True, check=False,
    )
    assert audited.returncode == 0, audited.stderr.decode(errors="replace")[-500:]
    changed = set(p.replace("\\", "/") for p in
                  audited.stdout.decode("utf-8").split("\0") if p)
    expected = {str(APP / "main.cpp").replace("\\", "/"),
                str(APP / "Main.qml").replace("\\", "/")}
    assert changed == expected, f"unexpected product source changes: {changed}"
    info = {
        "schema": "hyremote-windows-dynamic-quick-fixture/1",
        "fixture_is_app_only_not_product_patch": True,
        "app_main_original_sha256": digest(original_main),
        "app_main_modified_sha256": digest(adjusted),
        "app_qml_sha256": digest(qml),
        "framebuffer": [1920, 1080],
        "stage_interval_ms": 1200,
        "viewer_paint_not_measured_by_fixture": True,
    }
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")
    print("DYNAMIC_REFERENCE_APP_PATCHED="+json.dumps(info, separators=(",", ":")))


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--worktree", required=True, type=Path)
    p.add_argument("--fixture", required=True, type=Path)
    p.add_argument("--manifest", required=True, type=Path)
    args = p.parse_args()
    patch(args.worktree, args.fixture, args.manifest)
