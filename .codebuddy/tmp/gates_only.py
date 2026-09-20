"""Run the standalone release-readiness gates against a given source directory.

Usage: gates_only.py <source-dir> [--with-profile]

Success is detected by each gate's own PASS/FAIL behaviour, never by a guessed string: a gate is counted as failing
when it reports a CMake error, which is what every one of these gates raises on a violation.
"""

import pathlib
import subprocess
import sys

SOURCE = pathlib.Path(sys.argv[1])
WITH_PROFILE = "--with-profile" in sys.argv
CMAKE = r"C:\Qt\Tools\CMake_64\bin\cmake.exe"


def run(script, extra=()):
    p = subprocess.run([CMAKE, "-DHYREMOTE_SOURCE_DIR=%s" % str(SOURCE).replace("\\", "/")] + list(extra)
                       + ["-P", "tests/release-readiness/%s" % script],
                       cwd=str(SOURCE), capture_output=True, text=True, encoding="utf-8", errors="replace")
    return (p.stdout or "") + (p.stderr or "")


print("=== gates against %s ===" % SOURCE, flush=True)
ok = fail = 0
for gate in sorted(p.name for p in (SOURCE / "tests/release-readiness").glob("check_*.cmake")):
    if gate == "check_release_profile.cmake":
        continue
    out = run(gate)
    bad = "FATAL_ERROR" in out or "CMake Error" in out
    ok += 0 if bad else 1
    fail += 1 if bad else 0
    print("%-46s %s" % (gate, "FAIL" if bad else "PASS"), flush=True)
    if bad:
        print("    %s" % out.strip().splitlines()[-1][:140], flush=True)
print("=== %d PASS / %d FAIL ===" % (ok, fail), flush=True)
