#!/usr/bin/env python3
"""Verify deployed HyRemote/Qt ELF dependencies resolve from the deployment prefix."""

from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path


# The public shared facade uses `libHyRemote...`; the declarative backing library is the internal
# `libhyremote-qml...` payload. Both are product-owned runtime dependencies and must resolve from the
# deployed prefix just like Qt6 libraries. Core remains static and therefore has no runtime entry.
PRODUCT_PREFIXES = ("libHyRemote", "libhyremote-qml", "libQt6")


def fail(message: str) -> None:
    raise RuntimeError(message)


def is_within(path: Path, prefix: Path) -> bool:
    try:
        path.relative_to(prefix)
        return True
    except ValueError:
        return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--artifact", required=True, action="append", type=Path)
    args = parser.parse_args()

    prefix = args.prefix.resolve()
    if not prefix.is_dir():
        fail(f"deployment prefix does not exist: {prefix}")

    env = os.environ.copy()
    env.pop("LD_LIBRARY_PATH", None)

    checked = 0
    for raw_artifact in args.artifact:
        artifact = raw_artifact.resolve()
        if not artifact.is_file():
            fail(f"deployed ELF artifact does not exist: {artifact}")
        if not is_within(artifact, prefix):
            fail(f"artifact is outside deployment prefix: {artifact} (prefix {prefix})")

        result = subprocess.run(
            ["ldd", str(artifact)],
            check=False,
            capture_output=True,
            text=True,
            env=env,
        )
        output = result.stdout + result.stderr
        if result.returncode != 0:
            fail(f"ldd failed for {artifact} with {result.returncode}:\n{output}")

        artifact_checks = 0
        for raw_line in output.splitlines():
            line = raw_line.strip()
            if "=>" not in line:
                continue
            soname_text, resolved_text = line.split("=>", 1)
            soname = soname_text.strip()
            if not soname.startswith(PRODUCT_PREFIXES):
                continue

            resolved_token = resolved_text.strip().split(maxsplit=1)[0]
            if resolved_token == "not":
                fail(f"dependency is unresolved for {artifact}: {line}")
            resolved = Path(resolved_token).resolve()
            if not is_within(resolved, prefix):
                fail(
                    f"deployed dependency escaped prefix for {artifact}: "
                    f"{soname} => {resolved} (prefix {prefix})"
                )
            artifact_checks += 1
            checked += 1

        # Every V1 product/deployment artifact passed here must depend on at least one Qt/HyRemote
        # shared library. Zero matches usually means the wrong file or an unexpectedly static shape.
        if artifact_checks == 0:
            fail(f"no HyRemote/Qt shared dependency was observed for {artifact}:\n{output}")

        print(f"PASS dependency origin: {artifact}")
        print(output, end="" if output.endswith("\n") else "\n")

    if checked == 0:
        fail("no HyRemote/Qt deployed dependency was checked")

    print(f"PASS: {checked} HyRemote/Qt dependency resolutions stayed within {prefix}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
