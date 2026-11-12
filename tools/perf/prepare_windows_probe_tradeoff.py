#!/usr/bin/env python3
"""Fail-closed Windows Qt Quick dynamic fixture + SINGLE Runtime probe change.

Both variants start at the same committed candidate SHA. This script modifies
only the standalone Qt-only learning app fixture and, for the 100ms arm, one
C++ constexpr in ViewerCaptureDemand. It does NOT change Core, Qt integration,
RFB, single-flight or any other production source. Not a shipping candidate.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

CANDIDATE = "5095d0bc3bb07966c03dad77296f6ec04a631adc"
HEADER = Path("src/runtime/src/detail/viewer_capture_demand.hpp")
APP = {
    "examples/learning/03-generic/quick-app/main.cpp",
    "examples/learning/03-generic/quick-app/Main.qml",
}
EXISTING = "static constexpr auto kStaticProbeInterval = std::chrono::milliseconds(250);"
EXPERIMENT = "static constexpr auto kStaticProbeInterval = std::chrono::milliseconds(100);"


def sha256(blob: bytes) -> str:
    return hashlib.sha256(blob).hexdigest()


def git(tree, *args):
    p = subprocess.run(
        ["git", *args], cwd=tree, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        check=False
    )
    if p.returncode:
        raise RuntimeError(f"git {' '.join(args)} failed: {p.stderr.decode(errors='replace')[-1000:]}")
    return p.stdout


def modify_probe(tree: Path, interval: int):
    if interval not in (100, 250):
        raise ValueError("Only fixed 100ms and 250ms lanes exist")
    header = tree / HEADER
    initial = header.read_bytes()
    text = initial.decode("utf-8")
    if text.count(EXISTING) != 1:
        raise RuntimeError("Pinned source does not contain exactly one expected probe line")
    if interval == 100:
        result = text.replace(EXISTING, EXPERIMENT).encode("utf-8")
        header.write_bytes(result)
    else:
        result = initial
    return {
        "path": HEADER.as_posix(),
        "original_sha256": sha256(initial),
        "experiment_sha256": sha256(result),
        "old_line": EXISTING,
        "new_line": EXPERIMENT if interval == 100 else EXISTING,
    }


def prepare(args):
    tree = args.worktree.resolve()
    if git(tree, "rev-parse", "HEAD").decode("ascii").strip() != CANDIDATE:
        raise RuntimeError("Wrong source SHA: refusing to patch product source")
    # The fixture patch audits the two APP files itself BEFORE any Runtime
    # change. Only then permit a single expected constexpr line substitution.
    fixture_script = Path(__file__).with_name("fixtures") / "windows_dynamic_generic" / "apply_fixture.py"
    if not fixture_script.is_file():
        raise RuntimeError(f"Missing pinned app fixture script: {fixture_script}")
    fixture_manifest = args.manifest.parent / "fixture-identity.json"
    app = subprocess.run(
        [sys.executable, str(fixture_script),
         "--worktree", str(tree), "--fixture", str(args.fixture.resolve()),
         "--manifest", str(fixture_manifest)],
        check=False, capture_output=True, text=True, errors="replace",
    )
    if app.returncode:
        raise RuntimeError(f"Application fixture patch failed: {app.stdout[-1000:]} {app.stderr[-1000:]}")
    print(app.stdout.strip(), flush=True)
    audit = modify_probe(tree, args.probe_ms)

    # NUL-delimited tracked file list; CRLF warnings on stderr are NOT paths.
    tracked = {p.decode("utf-8").replace("\\", "/")
               for p in git(tree, "diff", "--name-only", "-z", "--no-ext-diff").split(b"\0") if p}
    expected = APP | ({HEADER.as_posix()} if args.probe_ms == 100 else set())
    if tracked != expected:
        raise RuntimeError(f"Not single-variable: tracked source delta={sorted(tracked)} expected={sorted(expected)}")
    untracked = git(tree, "ls-files", "--others", "--exclude-standard", "-z")
    if untracked:
        raise RuntimeError(f"Unexpected untracked files: {untracked[:600]!r}")
    if args.probe_ms == 100:
        numstat = git(tree, "diff", "--numstat", "--", HEADER.as_posix()).decode("utf-8")
        pieces = numstat.split("\t")
        if len(pieces) != 3 or pieces[:2] != ["1", "1"]:
            raise RuntimeError(f"Probe diff is not exactly one line changed: {numstat!r}")

    source_app = {path: sha256((tree / path).read_bytes()) for path in sorted(APP)}
    identity = {
        "schema": "hyremote-100-vs-250ms-source-pinned-probe/1",
        "experiment_only_not_a_releasable_product": True,
        "source_sha": CANDIDATE,
        "probe_constant_ms": args.probe_ms,
        "changed_product_source": [HEADER.as_posix()] if args.probe_ms == 100 else [],
        "source_change_scope": "ONE private constexpr only; also used for no-viewer-update-demand fallback",
        "capture_control_file": audit,
        "changed_tracked_files": sorted(tracked),
        "dynamic_app_sources_sha256": source_app,
        "dynamic_scene": "1920x1080; A blue to B red; every 1200ms; real Qt Quick full scene",
        "qt_version": "6.8.3",
        "kit": "msvc2022_64 Release",
        "viewer_cpu": "NOT_MEASURED",
        "viewer_visible_p95": "NOT_MEASURED",
        "real_windows_d3d11": "NOT_RUN_HOSTED",
        "independent_b": "NOT_RUN",
        "site_handoff": "BLOCKED",
    }
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(identity, indent=2, sort_keys=True) + "\n",
                             encoding="utf-8")
    print("PROBE_VARIANT_PREPARED=" + json.dumps({
        "source_sha": CANDIDATE,
        "probe_ms": args.probe_ms,
        "changed_files": sorted(tracked),
        "app_hashes": source_app,
    }, separators=(",", ":")), flush=True)


def self_test():
    assert EXISTING != EXPERIMENT
    assert EXISTING.replace("(250)", "(100)") == EXPERIMENT
    assert "kStaticProbeInterval" in EXISTING
    assert len(APP) == 2
    print("PASS probe substitution literal: one fixed constexpr only")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--probe-ms", type=int, choices=(100, 250))
    parser.add_argument("--worktree", type=Path)
    parser.add_argument("--fixture", type=Path)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if any(a is None for a in (args.probe_ms, args.worktree, args.fixture, args.manifest)):
        parser.error("--probe-ms, --worktree, --fixture, --manifest required")
    prepare(args)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as exc:
        print("PROBE_VARIANT=FAIL_OR_BLOCKED REASON=" + str(exc), file=sys.stderr)
        sys.exit(1)
