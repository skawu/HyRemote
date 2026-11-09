#!/usr/bin/env python3
"""Prepare a REAL Windows Generic Qt Quick product pair with identical app code.

Windows product integration follow-up for #922, NOT a physical performance test.
Baseline is #789's recorded site-release source, not a reconstructed site binary.
Candidate is a pinned develop revision. Both build via canonical build.cmd.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
from datetime import datetime, timezone

from run_quick_overlap_windows import git, require, run, environment_snapshot

BASELINE = "6a4b7b373ae1dce1f161771d5caab1cdb51a45d0"
CANDIDATE = "5095d0bc3bb07966c03dad77296f6ec04a631adc"
APP = "hyremote-learning-03-quick.exe"
APP_SOURCES = (
    "examples/learning/03-generic/quick-app/main.cpp",
    "examples/learning/03-generic/quick-app/Main.qml",
    "examples/learning/03-generic/quick-app/CMakeLists.txt",
)


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qt-prefix", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    require(sys.platform == "win32", "Real Windows / MSVC2022 x64 required")
    qt = args.qt_prefix.resolve()
    require(qt.name.lower() == "msvc2022_64"
            and qt.parent.name == "6.8.3"
            and (qt / "bin" / "Qt6Quick.dll").is_file(),
            "Wrong Qt 6.8.3 MSVC2022_64 Kit")
    for program in ("git", "cmake", "ninja", "cl"):
        require(shutil.which(program) is not None,
                f"Missing {program}; activate MSVC2022 x64 developer shell")
    repo = Path(git("rev-parse", "--show-toplevel")).resolve()
    out = args.out.resolve()
    require(not out.exists() or not any(out.iterdir()),
            "Evidence/packages destination must be a fresh empty directory")
    require(out != repo and repo not in out.parents
            and out != qt and qt not in out.parents,
            "Build artifact directory must be outside repo and Qt kit")
    out.mkdir(parents=True, exist_ok=True)
    hardware = environment_snapshot()
    (out / "windows-host-context.json").write_text(
        json.dumps(hardware, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    packages = []
    with tempfile.TemporaryDirectory(prefix="hyremote-qt-generic-product-pair-") as temp:
        worktrees = []
        try:
            for label, sha in (("site-source-baseline", BASELINE),
                               ("refactor-candidate", CANDIDATE)):
                tree = Path(temp) / label
                git("worktree", "add", "--detach", str(tree), sha, cwd=repo)
                worktrees.append(tree)
                require(git("rev-parse", "HEAD", cwd=tree) == sha,
                        f"{label}: wrong source revision")
                app_hashes = {name: digest(tree / name) for name in APP_SOURCES}
                install = tree / "build-reference-pair" / "install"
                print(f"BUILD_PRODUCT_PAIR={label} SOURCE_SHA={sha}", flush=True)
                run(
                    ["cmd.exe", "/d", "/c", "build.cmd", "install",
                     "--integrations=generic", "--examples",
                     f"--qt-prefix={qt}", "--build-type=Release",
                     "--build-dir=build-reference-pair", "--no-tests",
                     "--clean", "-j4"],
                    cwd=tree, timeout=2700, logfile=out / f"{label}-build.log",
                )
                exe = install / "bin" / APP
                require(exe.is_file(),
                        f"{label}: installed Generic Qt Quick application absent")
                generic_dlls = [
                    f for f in install.rglob("*.dll")
                    if "generic" in tuple(part.lower() for part in f.parts)
                    and "hyremote" in f.name.lower()
                ]
                require(generic_dlls,
                        f"{label}: deployed Generic plugin absent; plain Qt app is NOT an acceptable replacement")
                dest = out / label / "install"
                shutil.copytree(install, dest)
                installed_exe = dest / "bin" / APP
                require(installed_exe.is_file(), f"{label}: copied product missing")
                record = {
                    "label": label,
                    "source_sha": sha,
                    "source_origin": ("historical #789 site-reported source, NOT verified same binary"
                                      if label == "site-source-baseline"
                                      else "current pinned refactored develop candidate"),
                    "kit": "Qt 6.8.3 MSVC2022_64",
                    "build": "Release / canonical build.cmd install --integrations=generic --examples",
                    "source_app_hash_sha256": app_hashes,
                    "installed_executable": f"bin/{APP}",
                    "installed_executable_sha256": digest(installed_exe),
                    "generic_plugin_paths_relative_to_install":
                        [f.relative_to(install).as_posix() for f in generic_dlls],
                    "product_package": f"{label}/install",
                    "sample_is_not_site_business_application": True,
                    "real_viewer_test": "NOT_RUN",
                    "runtime_cpu": "NOT_MEASURED",
                }
                packages.append(record)
                (out / label / "manifest.json").write_text(
                    json.dumps(record, ensure_ascii=False, indent=2) + "\n",
                    encoding="utf-8",
                )
        finally:
            for tree in reversed(worktrees):
                if tree.exists():
                    import subprocess
                    subprocess.run(
                        ["git", "worktree", "remove", "--force", str(tree)],
                        cwd=repo, check=False,
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                    )

    require(packages[0]["source_app_hash_sha256"]
            == packages[1]["source_app_hash_sha256"],
            "The baseline/candidate Qt Quick application sources diverged; comparison INVALID")
    summary = {
        "schema": "hyremote-windows-generic-product-pair/1",
        "build_identity": packages,
        "source_app_identical": True,
        "integration": "Generic Qt Quick (not site deployment)",
        "installed_executable_name": APP,
        "tested_windows_desktop": False,
        "tested_real_viewer": False,
        "build_finished_utc": datetime.now(timezone.utc).isoformat(),
        "network_security": "Connect only inside isolated trusted lab; view-only preview, no internet exposure",
    }
    (out / "product-pair-identity.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("WINDOWS_GENERIC_PRODUCT_PAIR=BUILT_NOT_TESTED", flush=True)
    print("SOURCE_APP_IDENTICAL=YES", flush=True)
    print("SITE_HANDOFF=NOT_AUTHORIZED", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as exc:
        print(f"WINDOWS_GENERIC_PRODUCT_PAIR=BLOCKED_OR_FAILED REASON={exc}",
              file=sys.stderr, flush=True)
        sys.exit(1)
