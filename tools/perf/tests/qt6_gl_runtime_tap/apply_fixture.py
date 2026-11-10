#!/usr/bin/env python3
"""Patch only the Qt-only Generic Quick example for a source-locked CI smoke.

This deliberately modifies NO product/runtime source, only two user-app
fixture files in the checked-out test worktree. The source revision is always
the checked-out PR HEAD; fixture content and SHA256 are retained as evidence.
"""
from __future__ import annotations
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--worktree", type=Path, default=Path("."))
    p.add_argument("--manifest", type=Path, required=True)
    args = p.parse_args()
    root = args.worktree.resolve()
    script = Path(__file__).resolve()
    source = script.parent / "Main.qml"
    dest = root / "examples/learning/03-generic/quick-app/Main.qml"
    main_cpp = root / "examples/learning/03-generic/quick-app/main.cpp"
    current = subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    before = main_cpp.read_text(encoding="utf-8")
    exact = "    view.resize(460, 190);"
    if before.count(exact) != 1:
        raise RuntimeError("Original user app geometry seam absent")
    main_cpp.write_text(before.replace(exact, "    view.resize(640, 360);"),
                        encoding="utf-8")
    dest.write_bytes(source.read_bytes())
    names = set(subprocess.check_output(
        ["git", "-C", str(root), "diff", "--name-only"], text=True).splitlines())
    allowed = {
        "examples/learning/03-generic/quick-app/Main.qml",
        "examples/learning/03-generic/quick-app/main.cpp",
    }
    if names != allowed:
        raise RuntimeError(f"Unexpected tracked source modification: {sorted(names)}")
    obj = {
        "schema": "hyremote-qt6-gl-runtime-tap-fixture/1",
        "source_sha": current,
        "changed_tracked_paths": sorted(names),
        "fixture_qml_sha256": sha(dest),
        "fixture_main_sha256": sha(main_cpp),
        "user_application_links_no_hyremote_libraries": True,
        "source_stage_interval_ms": 120,
        "source_stage_colors": ["#e22634", "#27cf40", "#2c44e8"],
        "qt_only_test_application": True,
        "product_source_modified": False,
        "real_viewer_performance_qualified": False,
    }
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(obj, indent=2) + "\n", encoding="utf-8")
    print("QT6_GL_TEST_FIXTURE=PASS SOURCE_SHA=" + current)

if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print("QT6_GL_TEST_FIXTURE=FAIL " + str(exc), file=sys.stderr)
        raise
