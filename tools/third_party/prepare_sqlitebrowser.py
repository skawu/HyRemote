#!/usr/bin/env python3
"""Prepare a disposable SQLiteBrowser source tree for a source build.

The tracked submodule must remain pristine. SQLiteBrowser's git checkout stores
translation .ts files and its resource file references the generated .qm files.
The upstream CMake translation rules intentionally emit those .qm files into the
source translation directory, so a throwaway materialized tree must generate them
before AUTORCC scans translations.qrc.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path
import xml.etree.ElementTree as ET


def fail(message: str) -> "NoReturn":
    raise SystemExit(message)


def find_lrelease(qt_prefix: Path) -> Path:
    candidates = [qt_prefix / "bin" / "lrelease", qt_prefix / "bin" / "lrelease.exe"]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    fail(f"Qt lrelease not found under {qt_prefix / 'bin'}")


def qrc_qm_paths(qrc: Path) -> list[Path]:
    root = ET.parse(qrc).getroot()
    result: list[Path] = []
    for node in root.iter("file"):
        if node.text and node.text.strip().endswith(".qm"):
            result.append((qrc.parent / node.text.strip()).resolve())
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--qt-prefix", required=True, type=Path)
    args = parser.parse_args()

    source = args.source.resolve()
    translations = source / "src" / "translations"
    qrc = translations / "translations.qrc"
    if not qrc.is_file():
        fail(f"SQLiteBrowser translations.qrc not found: {qrc}")

    ts_files = sorted(translations.glob("*.ts"))
    if not ts_files:
        fail(f"No SQLiteBrowser .ts files found in {translations}")

    lrelease = find_lrelease(args.qt_prefix.resolve())
    for ts in ts_files:
        qm = ts.with_suffix(".qm")
        subprocess.run([str(lrelease), str(ts), "-qm", str(qm)], check=True)

    referenced = qrc_qm_paths(qrc)
    missing = [path for path in referenced if not path.is_file()]
    if missing:
        print("translations.qrc still has missing .qm payloads:", file=sys.stderr)
        for path in missing:
            print(f"  {path}", file=sys.stderr)
        return 1

    print(
        f"prepared SQLiteBrowser disposable translations: "
        f"{len(ts_files)} .ts -> .qm; {len(referenced)} qrc references satisfied"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
