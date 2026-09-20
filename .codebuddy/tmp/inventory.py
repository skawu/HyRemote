"""Full inventory of open PRs, worktrees and branches, so the convergence plan is based on measurement."""

import json
import pathlib
import subprocess

REPO = "skawu/HyRemote"
WT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")


def sh(args, cwd=None):
    p = subprocess.run(args, cwd=str(cwd) if cwd else None, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    return (p.stdout or "") + (p.stderr or "")


def gh_json(args):
    out = sh(["gh"] + args)
    try:
        return json.loads(out)
    except Exception:
        print("!! unreadable: %s" % out[:200], flush=True)
        return None


print("=== open pull requests ===", flush=True)
prs = gh_json(["pr", "list", "--repo", REPO, "--state", "open", "--limit", "40", "--json",
               "number,title,headRefName,baseRefName,mergeable,mergeStateStatus,updatedAt,isDraft"])
if prs:
    for pr in sorted(prs, key=lambda p: p["number"]):
        print("#%-4d %-8s -> %-38s %-10s %s" % (pr["number"], pr["headRefName"][:34], pr["baseRefName"],
                                                pr["mergeStateStatus"], pr["title"][:52]), flush=True)

print("", flush=True)
print("=== worktrees ===", flush=True)
print(sh(["git", "worktree", "list"], WT).strip(), flush=True)

print("", flush=True)
print("=== local branches (with last commit) ===", flush=True)
print(sh(["git", "for-each-ref", "--sort=-committerdate", "--format=%(refname:short) | %(committerdate:short) | %(subject)",
          "refs/heads/"], WT).strip(), flush=True)

print("", flush=True)
print("=== remote branches ===", flush=True)
print(sh(["git", "for-each-ref", "--sort=-committerdate", "--format=%(refname:short) | %(committerdate:short) | %(subject)",
          "refs/remotes/origin/"], WT).strip(), flush=True)

print("", flush=True)
print("=== which branches are already contained in the candidate line ===", flush=True)
base = "origin/feature/104-v1-ga-acceptance-matrix"
merged = sh(["git", "branch", "-a", "--merged", base], WT).splitlines()
for line in merged:
    line = line.strip().lstrip("* ").strip()
    if line and "->" not in line:
        print("  merged: %s" % line, flush=True)

print("", flush=True)
print("=== candidate line head ===", flush=True)
print(sh(["git", "log", "--oneline", "-1", base], WT).strip(), flush=True)
