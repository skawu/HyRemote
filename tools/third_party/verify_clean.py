#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MATRIX = json.loads((ROOT / "tests" / "third_party" / "matrix.json").read_text(encoding="utf-8"))
APPS = {app["id"]: app for app in MATRIX["applications"]}


def run(*args, cwd=ROOT):
    return subprocess.run(args, cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


def expected_gitlink(path: str) -> str:
    line = run("git", "ls-tree", "HEAD", "--", path)
    if not line:
        raise RuntimeError(f"missing gitlink: {path}")
    return line.split()[2]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apps", nargs="*", default=sorted(APPS))
    ns = parser.parse_args()
    failed = False
    for app_id in ns.apps:
        if app_id not in APPS:
            print(f"unknown app: {app_id}", file=sys.stderr)
            failed = True
            continue
        path = ROOT / APPS[app_id]["path"]
        if not path.exists():
            print(f"{app_id}: submodule not initialized: {path}", file=sys.stderr)
            failed = True
            continue
        head = run("git", "rev-parse", "HEAD", cwd=path)
        expected = expected_gitlink(APPS[app_id]["path"])
        status = run("git", "status", "--porcelain", "--untracked-files=all", cwd=path)
        if head != expected:
            print(f"{app_id}: HEAD {head} != gitlink {expected}", file=sys.stderr)
            failed = True
        if status:
            print(f"{app_id}: dirty upstream working tree:\n{status}", file=sys.stderr)
            failed = True
        if head == expected and not status:
            print(f"{app_id}: clean @ {head}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
