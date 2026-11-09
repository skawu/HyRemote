#!/usr/bin/env python3
"""Windows INTERACTIVE-DESKTOP source-pinned Qt 6.8.3 D3D11 overlap A/B/A.

Research-only. Builds exactly the two historical #919 source revisions with the
identical temporary diagnostic patch. Never modifies either product scheduler.
This is an actual Qt Quick grab workload, NOT a RealVNC end-to-end field test.
"""
from __future__ import annotations

import argparse
import ctypes
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone

BEFORE = "b1699ddab5b83a73d06dfc5d479dcda2cd6d8e74"
AFTER = "5095d0bc3bb07966c03dad77296f6ec04a631adc"
EXPECTED_QT = ("6.8.3", "msvc2022_64")


def run(argv, *, cwd=None, env=None, timeout=180, logfile=None):
    cmd = [str(p) for p in argv]
    print("RUN:", cmd[0], " ".join(cmd[1:4]), flush=True)
    started = time.monotonic()
    child = subprocess.Popen(
        cmd, cwd=cwd, env=env, text=True, encoding="utf-8", errors="replace",
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
    )
    try:
        while True:
            remaining = timeout - (time.monotonic() - started)
            if remaining <= 0:
                # Never leave a silent detached MSVC/CMake descendant behind.
                subprocess.run(["taskkill.exe", "/PID", str(child.pid), "/T", "/F"],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                               check=False)
                child.kill() if child.poll() is None else None
                output, _ = child.communicate(timeout=30)
                if logfile is not None:
                    Path(logfile).write_text(output, encoding="utf-8")
                raise RuntimeError(f"{Path(cmd[0]).name}: timed out after {timeout}s; "
                                   f"see {logfile or 'captured output'}")
            try:
                output, _ = child.communicate(timeout=min(5.0, remaining))
                break
            except subprocess.TimeoutExpired:
                print(f"PROGRESS command={Path(cmd[0]).name} "
                      f"elapsed_s={time.monotonic()-started:.0f}", flush=True)
    finally:
        if child.poll() is None:
            subprocess.run(["taskkill.exe", "/PID", str(child.pid), "/T", "/F"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           check=False)
    if logfile is not None:
        Path(logfile).write_text(output, encoding="utf-8")
    if child.returncode != 0:
        tail = "\n".join(output.splitlines()[-18:])
        raise RuntimeError(
            f"{Path(cmd[0]).name} exited {child.returncode}; "
            f"see {logfile or 'captured output'}\n{tail}"
        )
    return output.strip()


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def git(*args, cwd=None):
    return run(["git", *args], cwd=cwd, timeout=120)


def tracked_diagnostic_changes(worktree: Path):
    """Audit Git path bytes, never parse stderr CRLF warnings as filenames.

    -z also avoids quoting Windows/non-ASCII paths. Diagnostics from Git
    remain visible as stderr but do not become supposed modified paths.
    """
    cmd = ["git", "diff", "--name-only", "-z", "--no-ext-diff"]
    result = subprocess.run(cmd, cwd=worktree, capture_output=True, check=False)
    require(result.returncode == 0,
            f"Git source-patch audit failed (exit {result.returncode}): "
            f"{result.stderr.decode('utf-8', errors='replace')[-1000:]}")
    paths = result.stdout.decode("utf-8", errors="strict").split("\0")
    return set(x.replace("\\", "/") for x in paths if x)


def untracked_diagnostic_changes(worktree: Path):
    result = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard", "-z"],
        cwd=worktree, capture_output=True, check=False,
    )
    require(result.returncode == 0,
            f"Git untracked-source audit failed (exit {result.returncode}): "
            f"{result.stderr.decode('utf-8', errors='replace')[-1000:]}")
    return set(x.replace("\\", "/") for x in
               result.stdout.decode("utf-8", errors="strict").split("\0") if x)


def environment_snapshot():
    user32 = ctypes.windll.user32
    width, height = user32.GetSystemMetrics(0), user32.GetSystemMetrics(1)
    remote = bool(user32.GetSystemMetrics(0x1000))  # SM_REMOTESESSION
    require(width >= 1920 and height >= 1080,
            f"Physical desktop >=1920x1080 required; found {width}x{height}")
    require(not remote, "Remote Desktop session is not accepted as local physical D3D11 evidence")
    # This checks the Windows session/desktop, not whether the adapter uses WARP.
    process_id = os.getpid()
    session = ctypes.c_uint()
    require(bool(ctypes.windll.kernel32.ProcessIdToSessionId(process_id, ctypes.byref(session))),
            "Could not determine current Windows interactive session")
    require(session.value != 0, "Session 0 non-interactive service cannot qualify this test")
    gpu = []
    try:
        result = subprocess.run(
            ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
             "Get-CimInstance Win32_VideoController | "
             "Select-Object Name,DriverVersion | ConvertTo-Json -Compress"],
            text=True, encoding="utf-8", errors="replace", capture_output=True,
            timeout=30, check=False,
        )
        if result.returncode == 0 and result.stdout.strip():
            parsed = json.loads(result.stdout)
            gpu = parsed if isinstance(parsed, list) else [parsed]
    except (OSError, ValueError, subprocess.TimeoutExpired):
        pass
    return {
        "platform": platform.platform(),
        "local_session": int(session.value),
        "remote_session": remote,
        "display_pixels": [width, height],
        "gpu_adapters_reported_by_windows": gpu,
        "physical_gpu_vs_warp_verified": False,
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "warning": "GPU adapter identification and actual Viewer paint require independent review",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qt-prefix", required=True, type=Path,
                        help=r"Exact native Kit, e.g. C:\Qt\6.8.3\msvc2022_64")
    parser.add_argument("--out", required=True, type=Path,
                        help="Evidence directory, outside both historical worktrees")
    args = parser.parse_args()
    require(sys.platform == "win32", "This runner requires real Windows")
    repo = Path(git("rev-parse", "--show-toplevel")).resolve()
    require((repo / "tools/perf/prepare_quick_overlap_probe.py").is_file(),
            "Run from checked-out #921 diagnostic branch")
    require((repo / "src/runtime/tests/test_quick_overlap_profile.cpp").is_file(),
            "No #921 overlap workload")
    qt = args.qt_prefix.resolve()
    require(qt.is_dir() and EXPECTED_QT[0] in str(qt)
            and qt.name.lower() == EXPECTED_QT[1],
            "Wrong Qt Kit; expected EXACT Qt 6.8.3 MSVC2022_64")
    require((qt / "bin" / "Qt6Quick.dll").is_file(), "Qt6Quick.dll missing in Qt Kit")
    for utility in ("git", "cmake", "ninja", "cl"):
        require(shutil.which(utility) is not None,
                f"Missing {utility} on PATH; activate MSVC2022 x64 developer environment")
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    require(not str(out).casefold().startswith(str(qt).casefold()),
            "Evidence directory cannot be located inside the Qt Kit")
    host = environment_snapshot()
    host["qt_prefix_version_and_arch"] = "6.8.3/msvc2022_64"
    host["source_before"] = BEFORE
    host["source_after"] = AFTER
    host["comparison_scope"] = "single-flight public Qt Quick grab only"
    (out / "windows-host-context.json").write_text(
        json.dumps(host, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    # Each invocation gets a fresh uniquely owned throwaway worktree parent.
    # Original branch and genuine source history remain unchanged.
    with tempfile.TemporaryDirectory(prefix="hyremote-win-overlap-") as temp:
        root = Path(temp)
        trees = {}
        try:
            for label, sha in (("before", BEFORE), ("after", AFTER)):
                tree = root / label
                print(f"PREPARE {label}: {sha}", flush=True)
                git("worktree", "add", "--detach", str(tree), sha, cwd=repo)
                trees[label] = tree
                require(git("rev-parse", "HEAD", cwd=tree) == sha,
                        f"{label}: exact pinned source mismatch")
                shutil.copy2(repo / "src/runtime/tests/test_quick_overlap_profile.cpp",
                             tree / "src/runtime/tests/test_quick_overlap_profile.cpp")
                run([sys.executable, str(repo / "tools/perf/prepare_quick_overlap_probe.py"),
                     str(tree)], cwd=repo)
                changed = tracked_diagnostic_changes(tree)
                expect = {
                    "src/runtime/src/detail/perf_probe.hpp",
                    "src/runtime/src/quick/quick_target.cpp",
                    "src/runtime/tests/CMakeLists.txt",
                }
                require(changed == expect, f"{label}: unexpected source patch {changed}")
                untracked = untracked_diagnostic_changes(tree)
                require(untracked == {"src/runtime/tests/test_quick_overlap_profile.cpp"},
                        f"{label}: unexpected untracked files {untracked}")
                require(
                    (tree / "src/runtime/tests/test_quick_overlap_profile.cpp").is_file(),
                    f"{label}: missing same source workload",
                )
                # Only build diagnostics against both exact historical SHAs.
                # Full unrelated historical CTest suites are qualified separately;
                # running them here would confound a Qt Quick A/B/A capture probe
                # with independent release-readiness / RFB watermarks and MSVC
                # nested-PDB test harness issues. build.cmd remains the authority.
                run(
                    ["cmd.exe", "/d", "/c", "build.cmd", "build", "--mode=runtime",
                     f"--qt-prefix={qt}", "--build-type=Release",
                     "--build-dir=build-overlap", "--tests", "--no-examples",
                     "--clean", "-j4",
                     "--cmake=HYREMOTE_BUILD_QUICK_ADAPTER=ON",
                     "--cmake=HYREMOTE_ENABLE_CPU_PROBE=ON"],
                    cwd=tree, timeout=2400, logfile=out / f"{label}-build.log",
                )

            for mode in ("static", "transition"):
                for variant in ("before_a1", "after", "before_a2"):
                    label = "after" if variant == "after" else "before"
                    tree = trees[label]
                    binaries = list(
                        (tree / "build-overlap" / "runtime" / "tests").rglob(
                            "hyremote-quick-overlap-profile.exe"
                        )
                    )
                    require(len(binaries) == 1,
                            f"{label}: expected exactly one benchmark exe, got {len(binaries)}")
                    env = dict(os.environ)
                    env.pop("QT_QUICK_BACKEND", None)
                    env["PATH"] = str(qt / "bin") + os.pathsep + env.get("PATH", "")
                    env["QT_QPA_PLATFORM"] = "windows"
                    env["QSG_RHI_BACKEND"] = "d3d11"
                    env["HYREMOTE_QUICK_OVERLAP_BACKEND"] = "d3d11"
                    env["HYREMOTE_QUICK_OVERLAP_MODE"] = mode
                    env["GITHUB_SHA"] = AFTER if label == "after" else BEFORE
                    env["HYREMOTE_QUICK_OVERLAP_FILE"] = str(
                        out / f"d3d11-{mode}-{variant}.json"
                    )
                    env["HYREMOTE_CPU_PROBE_FILE"] = str(
                        out / f"d3d11-{mode}-{variant}.probe.jsonl"
                    )
                    print(f"PROFILE d3d11/{mode}/{variant}", flush=True)
                    run([str(binaries[0])], cwd=tree, env=env, timeout=180,
                        logfile=out / f"d3d11-{mode}-{variant}.log")
                    require(Path(env["HYREMOTE_QUICK_OVERLAP_FILE"]).is_file()
                            and Path(env["HYREMOTE_CPU_PROBE_FILE"]).is_file(),
                            f"{mode}/{variant}: incomplete raw evidence")
            run([sys.executable, str(repo / "tools/perf/assess_quick_overlap.py"),
                 "--dir", str(out), "--backends", "d3d11"],
                cwd=repo, timeout=60, logfile=out / "assessment.log")
            print(f"WINDOWS_D3D11_A_B_A=MEASURED\nEVIDENCE={out}", flush=True)
            print((out / "assessment.log").read_text(encoding="utf-8"), flush=True)
        finally:
            for label, tree in reversed(list(trees.items())):
                if tree.exists():
                    subprocess.run(["git", "worktree", "remove", "--force", str(tree)],
                                   cwd=repo, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL, check=False)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.TimeoutExpired) as exc:
        print("WINDOWS_D3D11_A_B_A=BLOCKED_OR_FAILED", file=sys.stderr)
        print(f"REASON={exc}", file=sys.stderr)
        sys.exit(1)
