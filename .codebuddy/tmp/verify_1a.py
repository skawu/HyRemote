"""Verify slice 1a: the eleven registered profile cases behave as registered, and every standalone gate still passes.

Success is detected by the exact marker each gate prints, never by a guessed string.
"""

import pathlib
import subprocess

WT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")
CMAKE = r"C:\Qt\Tools\CMake_64\bin\cmake.exe"
SRC = str(WT).replace("\\", "/")

CASES = [
    ("develop-all-modes", "0.0.0", "ON", "ON", "ON", False),
    ("v001-cpp-only", "0.0.1.0", "OFF", "OFF", "OFF", False),
    ("v001-reject-qml", "0.0.1.0", "ON", "OFF", "OFF", True),
    ("v001-reject-qpa", "0.0.1.0", "OFF", "ON", "OFF", True),
    ("v001-reject-security", "0.0.1.0", "OFF", "OFF", "ON", True),
    ("v002-qml", "0.0.2.0", "ON", "OFF", "OFF", False),
    ("v002-reject-qpa", "0.0.2.0", "ON", "ON", "OFF", True),
    ("v002-reject-security", "0.0.2.0", "ON", "OFF", "ON", True),
    ("v003-all-modes", "0.0.3.0", "ON", "ON", "OFF", False),
    ("v003-reject-security", "0.0.3.0", "ON", "ON", "ON", True),
    ("v100-all-modes", "1.0.0.0", "ON", "ON", "ON", False),
]


def run(script, extra=()):
    p = subprocess.run([CMAKE, "-DHYREMOTE_SOURCE_DIR=%s" % SRC] + list(extra) + ["-P", "tests/release-readiness/%s" % script],
                       cwd=str(WT), capture_output=True, text=True, encoding="utf-8", errors="replace")
    out = (p.stdout or "") + (p.stderr or "")
    return out


print("=== release-profile cases ===", flush=True)
bad = 0
for name, version, qml, qpa, sec, expect_reject in CASES:
    out = run("check_release_profile.cmake", ["-DHYREMOTE_TEST_VERSION=%s" % version, "-DHYREMOTE_TEST_QML=%s" % qml,
                                              "-DHYREMOTE_TEST_QPA=%s" % qpa, "-DHYREMOTE_TEST_SECURITY=%s" % sec])
    passed = "release-profile check passed" in out
    rejected = "FATAL_ERROR" in out or "CMake Error" in out
    ok = (not passed and rejected) if expect_reject else passed
    if not ok:
        bad += 1
    print("%-22s v%-8s qml=%-3s qpa=%-3s sec=%-3s reject=%-5s -> %s"
          % (name, version, qml, qpa, sec, expect_reject, "OK" if ok else "MISMATCH"), flush=True)
print("mismatches: %d" % bad, flush=True)

print("", flush=True)
print("=== standalone gates ===", flush=True)
failures = 0
for gate in sorted(p.name for p in (WT / "tests/release-readiness").glob("check_*.cmake")):
    if gate in ("check_release_profile.cmake",):
        continue
    out = run(gate)
    failed = "FATAL_ERROR" in out or "CMake Error" in out
    if failed:
        failures += 1
    last = out.strip().splitlines()[-1][:130] if out.strip() else "(no output)"
    print("%-46s %s" % (gate, "PASS" if not failed else "FAIL"), flush=True)
    if failed:
        print("    %s" % last, flush=True)
print("standalone failures: %d" % failures, flush=True)
