#!/usr/bin/env python3
"""Verify that deployed ELF artifacts carry only relocatable runtime-path entries.

This is the shared deployed-ELF truth checker for the Linux deployment: the product stages several payloads
(the shared runtime, the Generic plugin, the QPA delegate and platform plugins, the QML module) and every one of
them must reach the deployment it was placed in rather than the machine that produced it.

The subject is the artifact's own DT_RPATH/DT_RUNPATH, read from the ELF, not what the loader happens to resolve on
this host: a payload can look correct here and still name the build tree or the SDK it was built against. An entry
is therefore accepted only when it is relocatable (relative or $ORIGIN-based) or a plain system directory, and any
absolute entry naming a build, source or SDK location fails the run.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
from pathlib import Path


# Standard loader directories. A deployed payload may keep relying on these, because they exist on every machine the
# deployment could land on; anything else absolute is a path from the machine that built it.
DEFAULT_SYSTEM_PREFIXES = ("/lib", "/lib64", "/usr/lib", "/usr/lib64", "/usr/local/lib")

ELF_MAGIC = b"\x7fELF"
RPATH_LINE = re.compile(r"\((?:RPATH|RUNPATH)\)")


def fail(message: str) -> None:
    raise RuntimeError(message)


def is_within(path: Path, prefix: Path) -> bool:
    try:
        path.relative_to(prefix)
        return True
    except ValueError:
        return False


def runtime_path_entries(artifact: Path) -> list[str]:
    """Return the artifact's own DT_RPATH/DT_RUNPATH entries, in file order."""
    result = subprocess.run(
        ["readelf", "--dynamic", "--wide", str(artifact)],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        fail(f"readelf failed for {artifact} with {result.returncode}:\n{result.stdout}{result.stderr}")

    entries: list[str] = []
    for line in result.stdout.splitlines():
        if not RPATH_LINE.search(line):
            continue
        start = line.find("[")
        end = line.rfind("]")
        if start == -1 or end == -1 or end < start:
            fail(f"could not read the runtime path entry of {artifact} from: {line}")
        raw = line[start + 1 : end]
        for entry in raw.split(":"):
            entries.append(entry)
    return entries


def is_elf(path: Path) -> bool:
    try:
        with path.open("rb") as stream:
            return stream.read(4) == ELF_MAGIC
    except OSError:
        return False


def collect_artifacts(args: argparse.Namespace, prefix: Path) -> list[Path]:
    selected: list[Path] = []
    for raw in args.artifact:
        artifact = raw.resolve()
        if not artifact.is_file():
            fail(f"deployed ELF artifact does not exist: {artifact}")
        if not is_within(artifact, prefix):
            fail(f"artifact is outside the deployment prefix: {artifact} (prefix {prefix})")
        selected.append(artifact)
    if args.scan:
        for root, _dirs, files in os.walk(prefix):
            for name in files:
                candidate = Path(root) / name
                if candidate.is_symlink() or not is_elf(candidate):
                    continue
                selected.append(candidate)
    if not selected:
        fail("no deployed ELF artifact was selected")
    return sorted(set(selected))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--artifact", action="append", default=[], type=Path)
    parser.add_argument("--scan", action="store_true", help="check every ELF file under the prefix")
    parser.add_argument("--system-prefix", action="append", default=[], type=Path)
    args = parser.parse_args()

    prefix = args.prefix.resolve()
    if not prefix.is_dir():
        fail(f"deployment prefix does not exist: {prefix}")
    system_prefixes = [p.resolve() for p in args.system_prefix] or [
        Path(p) for p in DEFAULT_SYSTEM_PREFIXES
    ]

    artifacts = collect_artifacts(args, prefix)

    checked = 0
    relocatable = 0
    system = 0
    for artifact in artifacts:
        entries = runtime_path_entries(artifact)
        checked += 1
        for entry in entries:
            if entry == "":
                fail(
                    f"deployed artifact {artifact} carries an empty runtime-path entry, which the loader reads as the "
                    "working directory"
                )
            if not entry.startswith("/"):
                relocatable += 1
                continue
            absolute = Path(entry)
            if any(is_within(absolute, allowed) for allowed in system_prefixes):
                system += 1
                continue
            fail(
                f"deployed artifact {artifact} names the absolute runtime path '{entry}', which belongs to the machine "
                f"that built it rather than to {prefix}"
            )
        print(f"PASS deployed runtime path: {artifact} -> {entries if entries else '[]'}")

    print(
        f"PASS: {checked} deployed ELF artifact(s) checked, {relocatable} relocatable entry(ies), "
        f"{system} standard system entry(ies), 0 build/source/SDK entry(ies)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
