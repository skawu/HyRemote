"""Repair docs/architecture.md: my text-mode round-trip rewrote the whole file's line endings.

The index edit is redone in byte mode against the checkout's own newline, after restoring the file from git.
Also reports the repository's stored line endings and autocrlf setting, since a wrong guess here is what
produced the noise in the first place.
"""

import pathlib
import subprocess

ROOT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")


def git(args, cwd=ROOT):
    return subprocess.run(["git"] + args, cwd=str(cwd), capture_output=True)


stored = git(["show", "HEAD:docs/security-model.md"]).stdout
autocrlf = subprocess.run(
    ["git", "config", "core.autocrlf"], capture_output=True, text=True
).stdout.strip()
print("stored docs/security-model.md contains CRLF:", b"\r\n" in stored)
print("core.autocrlf:", autocrlf or "(unset)")

# Restore the file my edit rewrote, then redo the edit byte-exactly.
subprocess.run(["git", "checkout", "--", "docs/architecture.md"], cwd=str(ROOT), check=True)
arch = ROOT / "docs/architecture.md"
raw = arch.read_bytes()
nl = b"\r\n" if b"\r\n" in raw[:4000] else b"\n"
marker = b"- [`ADR-0003 Threading, Scheduling and Backpressure`](adr/0003-threading-backpressure.md);"
assert raw.count(marker) == 1, "ADR-0003 index line not found exactly once"
addition = (
    nl
    + b"- [`ADR-0006 Authenticated and Encrypted Transport Design`](adr/0006-authenticated-transport-design.md);"
)
arch.write_bytes(raw.replace(marker, marker + addition, 1))
print("architecture.md newline reused:", nl)

out = subprocess.run(["git", "diff", "--stat"], cwd=str(ROOT), capture_output=True, text=True).stdout
print("--- diffstat after repair ---")
print(out.strip() or "(no tracked changes)")
