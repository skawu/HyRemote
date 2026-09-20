"""Delete only branches provably contained in the candidate line, and report what remains.

The safety rule is the whole point: a branch is deleted only when git itself says it is merged into the candidate
line. Anything with unmerged commits is listed for the operator, never removed.
"""

import pathlib
import subprocess

WT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")
BASE = "origin/feature/104-v1-ga-acceptance-matrix"
PROTECTED = {"feature/104-v1-ga-acceptance-matrix", "main", "develop", "HEAD"}


def sh(args):
    p = subprocess.run(["git"] + args, cwd=str(WT), capture_output=True, text=True, encoding="utf-8", errors="replace")
    return (p.stdout or "") + (p.stderr or "")


merged = [b.strip().lstrip("* ").strip() for b in sh(["branch", "--merged", BASE, "--format=%(refname:short)"]).splitlines()]
merged = [b for b in merged if b and b not in PROTECTED]
unmerged = [b.strip().lstrip("* ").strip() for b in sh(["branch", "--no-merged", BASE, "--format=%(refname:short)"]).splitlines()]
unmerged = [b for b in unmerged if b and b not in PROTECTED and b not in {"origin/HEAD"}]

print("=== provably contained in %s: %d branches ===" % (BASE, len(merged)), flush=True)
for b in merged:
    print("   " + b, flush=True)

deleted, refused = [], []
for b in merged:
    out = sh(["branch", "-d", b])
    if "Deleted branch" in out:
        deleted.append(b)
    else:
        refused.append((b, out.strip().splitlines()[0][:110] if out.strip() else ""))

print("", flush=True)
print("deleted: %d" % len(deleted), flush=True)
if refused:
    print("refused (checked out in another worktree, or not fully merged): %d" % len(refused), flush=True)
    for b, why in refused:
        print("   %-46s %s" % (b, why), flush=True)

print("", flush=True)
print("=== NOT contained in the candidate line: %d branches, all left untouched ===" % len(unmerged), flush=True)
for b in unmerged:
    ahead = sh(["rev-list", "--count", "%s..%s" % (BASE, b)]).strip()
    print("   %-52s ahead=%s" % (b, ahead), flush=True)

print("", flush=True)
print("=== local branch count now: %s ===" % sh(["rev-list", "--count", "--all", "--branches"]).strip(), flush=True)
print("=== branches: %d ===" % len([l for l in sh(["for-each-ref", "--format=%(refname:short)", "refs/heads/"]).splitlines() if l.strip()]), flush=True)
