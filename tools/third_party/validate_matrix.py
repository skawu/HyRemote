#!/usr/bin/env python3
import configparser
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MATRIX_PATH = ROOT / "tests" / "third_party" / "matrix.json"
SPEC_DIR = ROOT / "tests" / "third_party" / "specs"
GITMODULES = ROOT / ".gitmodules"
HEX40 = re.compile(r"^[0-9a-f]{40}$")


def die(message: str) -> None:
    print(f"third-party matrix error: {message}", file=sys.stderr)
    raise SystemExit(1)


def gitlink(path: str):
    proc = subprocess.run(
        ["git", "-C", str(ROOT), "ls-tree", "HEAD", "--", path],
        check=True, capture_output=True, text=True
    )
    line = proc.stdout.strip()
    if not line:
        die(f"missing gitlink for {path}")
    meta, listed_path = line.split("\t", 1)
    mode, obj_type, sha = meta.split()
    if listed_path != path or mode != "160000" or obj_type != "commit":
        die(f"{path} is not a submodule gitlink: {line}")
    return sha


def main() -> int:
    data = json.loads(MATRIX_PATH.read_text(encoding="utf-8"))
    if data.get("schema_version") != 1:
        die("unsupported schema_version")
    if data.get("v1_qpa_exact_qt") != "6.8.3":
        die("V1 QPA exact version must remain 6.8.3")

    lanes = {lane["id"]: lane for lane in data["qt_lanes"]}
    if lanes.get("qt68-ref", {}).get("version") != "6.8.3":
        die("qt68-ref must remain exact Qt 6.8.3")
    for lane in lanes.values():
        if lane.get("qpa") == "qualified-v1" and lane.get("version") != "6.8.3":
            die(f"only exact 6.8.3 may be V1 QPA-qualified, got {lane['id']}")

    cfg = configparser.ConfigParser()
    cfg.read(GITMODULES, encoding="utf-8")
    seen = set()
    for app in data["applications"]:
        app_id = app["id"]
        if app_id in seen:
            die(f"duplicate application id {app_id}")
        seen.add(app_id)
        commit = app["tracking_commit"]
        if not HEX40.match(commit):
            die(f"invalid tracking commit for {app_id}: {commit}")
        section = f'submodule "third-party/{app_id}"'
        if section not in cfg:
            die(f"missing .gitmodules section {section}")
        if cfg[section].get("path") != app["path"]:
            die(f"path mismatch for {app_id}")
        if cfg[section].get("url") != app["url"]:
            die(f"url mismatch for {app_id}")
        if cfg[section].get("branch") != app["branch"]:
            die(f"branch mismatch for {app_id}")
        if gitlink(app["path"]) != commit:
            die(f"gitlink/tracking_commit mismatch for {app_id}")
        for lane_id, ref in app["lane_refs"].items():
            if lane_id not in lanes:
                die(f"unknown lane {lane_id} in {app_id}")
            if ref not in (None, "tracking") and not HEX40.match(ref):
                die(f"lane ref for {app_id}/{lane_id} must be null, tracking or a 40-char SHA")

        spec_path = SPEC_DIR / f"{app_id}.json"
        if not spec_path.exists():
            die(f"missing spec {spec_path.relative_to(ROOT)}")
        spec = json.loads(spec_path.read_text(encoding="utf-8"))
        if spec.get("id") != app_id or spec.get("upstream") != app["url"] or spec.get("branch") != app["branch"]:
            die(f"spec identity mismatch for {app_id}")
        if spec.get("tracking_commit") != commit:
            die(f"spec tracking commit mismatch for {app_id}")

    expected = {"sqlitebrowser", "qbittorrent", "musescore", "shotcut", "obs-studio"}
    if seen != expected:
        die(f"application set changed without authority update: {sorted(seen)}")
    print(f"third-party matrix valid: {len(seen)} applications, {len(lanes)} Qt lanes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
