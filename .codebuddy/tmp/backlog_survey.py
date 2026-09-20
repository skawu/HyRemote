"""Verify the hosted workflows on the new candidate head, and gather per-PR facts for the 8-PR backlog review.

Both halves are measurement only: the workflow half reads runs for the exact head, and the PR half asks GitHub for
each PR's own metadata plus whether git considers its branch already contained in the candidate line.
"""

import json
import pathlib
import subprocess

REPO = "skawu/HyRemote"
WT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")
CANDIDATE = "b23871c"  # head after merging #169, #177 and #178
BACKLOG = [136, 138, 146, 150, 152, 153, 155, 160]


def sh(args):
    p = subprocess.run(args, cwd=str(WT), capture_output=True, text=True, encoding="utf-8", errors="replace")
    return (p.stdout or "") + (p.stderr or "")


def gh_json(args):
    out = sh(["gh"] + args)
    try:
        return json.loads(out)
    except Exception:
        print("!! unreadable for %s: %s" % (" ".join(args[:4]), out[:160]), flush=True)
        return None


sh(["git", "fetch", "origin", "--prune", "--quiet"])

print("=== hosted runs on candidate head %s ===" % CANDIDATE, flush=True)
runs = gh_json(["api", "repos/%s/actions/runs?head_sha=%s&per_page=50" % (REPO, CANDIDATE)])
if runs:
    items = runs.get("workflow_runs", [])
    print("runs on that head: %d" % len(items), flush=True)
    for r in sorted(items, key=lambda x: x["name"]):
        print("  %-30s %s/%s" % (r["name"], r["status"], r["conclusion"]), flush=True)

print("", flush=True)
print("=== backlog PRs ===", flush=True)
for n in BACKLOG:
    pr = gh_json(["pr", "view", str(n), "--repo", REPO, "--json",
                  "number,title,headRefName,baseRefName,mergeable,mergeStateStatus,isDraft,additions,deletions,files,updatedAt"])
    if not pr:
        continue
    files = [f["path"] for f in pr.get("files", [])]
    head = pr["headRefName"]
    contained = "unknown"
    code = subprocess.run(["git", "merge-base", "--is-ancestor", "origin/%s" % head, "origin/feature/104-v1-ga-acceptance-matrix"],
                          cwd=str(WT), capture_output=True).returncode
    contained = "YES" if code == 0 else "no"
    print("#%-4d %-42s base=%-38s %+d/-%d contained=%s state=%s"
          % (n, head, pr["baseRefName"], pr["additions"], pr["deletions"], contained, pr["mergeStateStatus"]), flush=True)
    print("      %s" % pr["title"][:120], flush=True)
    print("      files: %s" % ", ".join(files[:6]), flush=True)
