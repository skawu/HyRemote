#!/usr/bin/env python3
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MATRIX = json.loads((ROOT / "tests" / "third_party" / "matrix.json").read_text(encoding="utf-8"))


def remote_head(url: str, branch: str) -> str:
    out = subprocess.run(["git", "ls-remote", url, f"refs/heads/{branch}"], check=True, capture_output=True, text=True).stdout.strip()
    if not out:
        raise RuntimeError(f"cannot resolve {url} {branch}")
    return out.split()[0]


def main() -> int:
    rows = []
    for app in MATRIX["applications"]:
        remote = remote_head(app["url"], app["branch"])
        rows.append({"id": app["id"], "pinned": app["tracking_commit"], "remote": remote, "update": remote != app["tracking_commit"]})
    print(json.dumps(rows, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
