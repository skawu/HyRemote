#!/usr/bin/env python3
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MATRIX_PATH = ROOT / "tests" / "third_party" / "matrix.json"
SPEC_DIR = ROOT / "tests" / "third_party" / "specs"


def head(path: Path) -> str:
    return subprocess.run(["git", "rev-parse", "HEAD"], cwd=path, check=True, capture_output=True, text=True).stdout.strip()


def dump(path: Path, obj) -> None:
    path.write_text(json.dumps(obj, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    matrix = json.loads(MATRIX_PATH.read_text(encoding="utf-8"))
    for app in matrix["applications"]:
        new_head = head(ROOT / app["path"])
        old = app["tracking_commit"]
        app["tracking_commit"] = new_head
        spec_path = SPEC_DIR / f"{app['id']}.json"
        spec = json.loads(spec_path.read_text(encoding="utf-8"))
        spec["tracking_commit"] = new_head
        dump(spec_path, spec)
        print(f"{app['id']}: {old} -> {new_head}")
    dump(MATRIX_PATH, matrix)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
