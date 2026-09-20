"""Prove the new anti-leak check works: pass, then fail on an unclassified reference, then pass again.

The document is restored from its own bytes (not via git) so nothing else in the working tree is touched.
"""

import pathlib
import subprocess

WT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")
GATE = "tests/release-readiness/check_release_authority_policy.cmake"
DOC = WT / "docs/security-model.md"
CMAKE = r"C:\Qt\Tools\CMake_64\bin\cmake.exe"


def run_gate():
    p = subprocess.run([CMAKE, "-DHYREMOTE_SOURCE_DIR=%s" % str(WT).replace("\\", "/"), "-P", GATE],
                       cwd=str(WT), capture_output=True, text=True, encoding="utf-8", errors="replace")
    out = (p.stdout or "") + (p.stderr or "")
    return ("authority policy passed" in out.lower()) or ("passed" in out.lower() and "FATAL" not in out), out


ok1, out1 = run_gate()
print("1) after the correction: passed=%s" % ok1, flush=True)
if not ok1:
    print(out1[-900:], flush=True)

original = DOC.read_bytes()
try:
    DOC.write_bytes(original + b"\nThe next release candidate is tracked as #999.\n")
    ok2, out2 = run_gate()
    print("2) with an unclassified reference (#999): passed=%s (expected False)" % ok2, flush=True)
    caught = "999" in out2 and "unclassified" in out2
    print("   the gate names the offending number and the reason: %s" % caught, flush=True)
    if not caught:
        print(out2[-700:], flush=True)
finally:
    DOC.write_bytes(original)

ok3, _ = run_gate()
restored = DOC.read_bytes() == original
print("3) after restoring the document: passed=%s, bytes identical=%s" % (ok3, restored), flush=True)
