#!/usr/bin/env python3
import argparse
import json
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MATRIX = json.loads((ROOT / "tests" / "third_party" / "matrix.json").read_text(encoding="utf-8"))
APPS = {app["id"]: app for app in MATRIX["applications"]}


def git(*args, cwd):
    return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


def main() -> int:
    parser = argparse.ArgumentParser(description="Materialize an upstream ref without modifying its submodule working tree")
    parser.add_argument("--app", required=True, choices=sorted(APPS))
    parser.add_argument("--lane", required=True)
    parser.add_argument("--dest", required=True, type=Path)
    ns = parser.parse_args()

    app = APPS[ns.app]
    if ns.lane not in app["lane_refs"]:
        parser.error(f"unknown lane {ns.lane}")
    configured = app["lane_refs"][ns.lane]
    if configured is None:
        parser.error(f"{ns.app}/{ns.lane} has no qualified/candidate upstream ref yet")
    ref = app["tracking_commit"] if configured == "tracking" else configured
    source = ROOT / app["path"]
    if not source.exists():
        parser.error(f"submodule not initialized: {source}")
    if git("status", "--porcelain", "--untracked-files=all", cwd=source):
        parser.error(f"refusing to materialize from dirty submodule: {source}")
    git("cat-file", "-e", f"{ref}^{{commit}}", cwd=source)

    dest = ns.dest.resolve()
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    with tempfile.TemporaryDirectory(prefix="hyremote-third-party-") as tmp:
        archive = Path(tmp) / "source.tar"
        subprocess.run(["git", "-C", str(source), "archive", "--format=tar", "-o", str(archive), ref], check=True)
        with tarfile.open(archive, "r") as tf:
            tf.extractall(dest, filter="data")
    print(f"materialized {ns.app}/{ns.lane} {ref} -> {dest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
