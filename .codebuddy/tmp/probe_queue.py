"""Probe every PR in the queue for the two facts that decide whether it can land: mergeability and check state.

Neither may be assumed - the earlier "the queue is ready" claim was made without measuring either.
"""

import json
import pathlib
import subprocess

REPO = "skawu/HyRemote"
WT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")
QUEUE = [155, 150, 153, 146, 152, 160]


def sh(args, cwd=None):
    p = subprocess.run(args, cwd=str(cwd) if cwd else None, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    return (p.stdout or "") + (p.stderr or "")


def gh_json(args):
    out = sh(["gh"] + args)
    try:
        return json.loads(out)
    except Exception:
        print("!! unreadable: %s" % out[:160], flush=True)
        return None


sh(["git", "fetch", "origin", "--prune", "--quiet"], WT)

for n in QUEUE:
    pr = gh_json(["pr", "view", str(n), "--repo", REPO, "--json",
                  "number,title,headRefName,baseRefName,mergeable,mergeStateStatus,isDraft"])
    if not pr:
        continue
    checks_raw = sh(["gh", "pr", "checks", str(n), "--repo", REPO])
    lines = [l for l in checks_raw.splitlines() if l.strip()]
    states = {}
    for l in lines:
        parts = l.split("\t")
        if len(parts) >= 2:
            states[parts[1]] = states.get(parts[1], 0) + 1
    # Does the branch rebase cleanly today? Test on a scratch merge without touching any worktree.
    head = pr["headRefName"]
    merge_test = sh(["git", "merge-tree", "--write-tree", "origin/develop", "origin/%s" % head], WT)
    conflict = "CONFLICT" in merge_test or "conflict" in merge_test.lower()
    print("#%-4d base=%-9s mergeable=%-9s state=%-9s checks=%s | rebase-conflicts=%s"
          % (n, pr["baseRefName"], pr["mergeable"], pr["mergeStateStatus"],
             states if states else "(none run)", conflict), flush=True)
    print("      %s" % pr["title"][:110], flush=True)
