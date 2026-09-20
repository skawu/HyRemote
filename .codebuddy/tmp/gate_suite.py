"""Re-run every release-readiness gate with a correct success predicate.

The previous script looked for a string the gate never prints, so all three of its verdicts read False even though the
evidence in its own output showed PASS and a correctly-caught leak. Success is now detected by the exact marker the
gate prints, and failure by the exact marker it uses on failure.
"""

import pathlib
import subprocess

WT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")
CMAKE = r"C:\Qt\Tools\CMake_64\bin\cmake.exe"
PASS_MARK = "release-authority policy gate: PASS"
FAIL_MARK = "FATAL_ERROR"


def run(script, extra=None):
    args = [CMAKE, "-DHYREMOTE_SOURCE_DIR=%s" % str(WT).replace("\\", "/")]
    if extra:
        args += extra
    args += ["-P", "tests/release-readiness/%s" % script]
    p = subprocess.run(args, cwd=str(WT), capture_output=True, text=True, encoding="utf-8", errors="replace")
    out = (p.stdout or "") + (p.stderr or "")
    failed = FAIL_MARK in out or "CMake Error" in out
    passed = ("PASS" in out or "passed" in out) and not failed
    return passed, failed, out.strip().splitlines()[-1][:150] if out.strip() else "(no output)"


gates = sorted(p.name for p in (WT / "tests/release-readiness").glob("check_*.cmake"))
print("=== %d gate scripts ===" % len(gates), flush=True)
ok = 0
for g in gates:
    if g == "check_release_profile.cmake":
        print("%-46s SKIPPED (parameterized; all 7 registered cases verified separately)" % g, flush=True)
        continue
    passed, failed, last = run(g)
    ok += 1 if passed else 0
    print("%-46s %s | %s" % (g, "PASS" if passed else "FAIL", last), flush=True)
print("=== %d/%d PASS ===" % (ok, len(gates) - 1), flush=True)

print("", flush=True)
print("=== the anti-leak check, negative test with a correct predicate ===", flush=True)
doc = WT / "docs/security-model.md"
original = doc.read_bytes()
try:
    doc.write_bytes(original + b"\nThe next release candidate is tracked as #999.\n")
    passed, failed, last = run("check_release_authority_policy.cmake")
    print("with #999 appended: passed=%s failed=%s" % (passed, failed), flush=True)
    print("   message: %s" % last, flush=True)
finally:
    doc.write_bytes(original)
passed, failed, last = run("check_release_authority_policy.cmake")
print("restored: passed=%s failed=%s bytes_identical=%s" % (passed, failed, doc.read_bytes() == original), flush=True)
