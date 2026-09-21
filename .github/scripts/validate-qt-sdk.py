#!/usr/bin/env python3
"""Validate that a Qt SDK prefix holds every artifact the reference lanes need.

Why this exists: the contract used to be expressed as bash predicates (``[[ -f ... ]]`` and ``compgen -G``) over a
prefix assembled from ``RUNNER_TEMP``. On the Windows runners ``RUNNER_TEMP`` is a native path such as ``D:\\a\\_temp``,
so those predicates were handed a string containing backslashes, which bash treats as ordinary characters rather than
separators; every check evaluated false and the whole contract collapsed into one generic "integrity validation
failed" line that named no artifact. Nothing in that failure was specific to a Qt file - the path representation was
wrong for the tool doing the checking.

The same capability contract is expressed here once, for both operating systems, over the real filesystem through
``pathlib``: it reports ``[PASS]`` or ``[MISSING]`` per artifact and exits non-zero if anything is missing. The two
platforms keep identical capability semantics; only the native platform plugin differs (``qwindows`` on Windows,
``qxcb`` on Linux).

Nothing here weakens the contract: the private Qt GUI header and the native platform plugin are still required
whenever QPA is selected, the QML package and module are still required whenever QML is selected, and a missing
artifact is still a failure rather than a warning.
"""

from __future__ import annotations

import argparse
import fnmatch
import os
import pathlib
import sys

PLATFORM_PLUGIN_PATTERNS = {
    "windows": ("qwindows*",),
    "linux": ("libqxcb*",),
}


def first_match(directory: pathlib.Path, patterns: tuple[str, ...]) -> pathlib.Path | None:
    """Return the first entry of ``directory`` matching any pattern, case-insensitively."""
    if not directory.is_dir():
        return None
    for entry in sorted(directory.iterdir()):
        for pattern in patterns:
            if fnmatch.fnmatch(entry.name.lower(), pattern.lower()):
                return entry
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate a Qt SDK prefix for the HyRemote reference lanes.")
    parser.add_argument("--prefix", required=True, help="Qt prefix as produced by the installer (native path is fine)")
    parser.add_argument("--qt-version", required=True, help="Exact Qt version, e.g. 6.8.3")
    parser.add_argument("--platform", required=True, choices=sorted(PLATFORM_PLUGIN_PATTERNS))
    parser.add_argument("--need-qml", default="false", help="true when the QML frontend is selected")
    parser.add_argument("--need-qpa", default="false", help="true when the QPA frontend is selected")
    parser.add_argument("--cmake-components", nargs="*", default=[],
                        help="Qt CMake package config file names, e.g. Qt6CoreConfig.cmake")
    parser.add_argument("--private-headers", nargs="*", default=[],
                        help="Qt private GUI headers required by the QPA frontend")
    args = parser.parse_args()

    prefix = pathlib.Path(args.prefix)

    print(f"QT_VALIDATOR=validate-qt-sdk.py")
    print(f"RUNNER_TEMP_RAW={os.environ.get('RUNNER_TEMP', '')}")
    print(f"QT_ROOT_VALIDATION_PATH={pathlib.Path(os.environ.get('RUNNER_TEMP', '.')) / 'Qt'}")
    print(f"QT_PREFIX_VALIDATION_PATH={prefix}")
    print(f"QT_PREFIX_RESOLVED={prefix.resolve() if prefix.exists() else '<does not exist>'}")
    print(f"QT_PLATFORM={args.platform} QT_VERSION={args.qt_version} "
          f"NEED_QML={args.need_qml} NEED_QPA={args.need_qpa}")

    if not prefix.is_dir():
        print(f"[MISSING] Qt prefix directory {prefix}")
        print("RESULT: 0 passed, 1 missing")
        return 1

    failures = 0
    passes = 0

    def report(label: str, path: pathlib.Path | None, expected: pathlib.Path) -> None:
        nonlocal failures, passes
        if path is not None:
            passes += 1
            print(f"[PASS] {label} -> {path}")
        else:
            failures += 1
            print(f"[MISSING] {label} -> expected {expected}")

    # The capability contract itself, unchanged: the five mandatory Qt CMake packages plus every package the caller
    # declares, then the two SDK tools, then the QML and QPA capabilities when they are selected.
    for component in args.cmake_components:
        stem = component[: -len("Config.cmake")] if component.endswith("Config.cmake") else component
        expected = prefix / "lib" / "cmake" / stem / component
        report(component, expected if expected.is_file() else None, expected)

    expected_qtroot = prefix / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake"
    report("Qt6Config.cmake", expected_qtroot if expected_qtroot.is_file() else None, expected_qtroot)

    for tool in ("qtpaths", "qmake"):
        directory = prefix / "bin"
        found = first_match(directory, (f"{tool}", f"{tool}.*"))
        report(f"bin/{tool}*", found, directory / f"{tool}*")

    if args.need_qml.lower() == "true":
        expected_qml_package = prefix / "lib" / "cmake" / "Qt6Qml" / "Qt6QmlConfig.cmake"
        report("Qt6QmlConfig.cmake", expected_qml_package if expected_qml_package.is_file() else None,
               expected_qml_package)
        expected_module = prefix / "qml" / "QtQml" / "qmldir"
        report("qml/QtQml/qmldir", expected_module if expected_module.is_file() else None, expected_module)

    if args.need_qpa.lower() == "true":
        for header in args.private_headers:
            expected_header = (prefix / "include" / "QtGui" / args.qt_version / "QtGui" / "private" / header)
            report(f"private/{header}", expected_header if expected_header.is_file() else None, expected_header)
        directory = prefix / "plugins" / "platforms"
        patterns = PLATFORM_PLUGIN_PATTERNS[args.platform]
        report(f"native platform plugin {patterns[0]}", first_match(directory, patterns),
               directory / patterns[0])

    print(f"RESULT: {passes} passed, {failures} missing")
    print("QT_SDK_VALIDATION=" + ("FAIL" if failures else "PASS"))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
