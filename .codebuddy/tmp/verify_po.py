"""Verify the product owner's governance claims against the actual repository and GitHub state."""

import json
import pathlib
import subprocess

REPO = "skawu/HyRemote"
WT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")


def gh(args):
    p = subprocess.run(["gh"] + args, capture_output=True, text=True, encoding="utf-8")
    if p.returncode != 0:
        print("!! gh rc=%d %s" % (p.returncode, p.stderr.strip()[:300]), flush=True)
    return p.stdout


print("=== the four claimed new issues ===", flush=True)
for n in (170, 174, 175, 176, 157, 156):
    try:
        i = json.loads(gh(["api", "repos/%s/issues/%d" % (REPO, n)]))
    except Exception:
        print("#%d: unreadable" % n, flush=True)
        continue
    labels = ",".join(l["name"] for l in i.get("labels", []))
    print("#%-4d [%s] %s" % (n, i["state"], i["title"]), flush=True)
    print("      labels: %s" % (labels or "(none)"), flush=True)

print("", flush=True)
print("=== last comment on #157 (the product owner says it requests the manifest sync) ===", flush=True)
c = json.loads(gh(["api", "repos/%s/issues/157/comments?per_page=100" % REPO]))
print("comments=%d" % len(c), flush=True)
if c:
    last = c[-1]
    print("id=%s len=%d" % (last["id"], len(last["body"])), flush=True)
    print(last["body"][:2000], flush=True)

print("", flush=True)
print("=== machine manifest as it stands on this branch ===", flush=True)
man = WT / ".github/release/v1-mandatory-issues.json"
if man.exists():
    data = json.loads(man.read_text(encoding="utf-8"))
    print("required_issue_numbers:", data.get("required_issue_numbers"), flush=True)
    print("embedded_deferred:", data.get("embedded_deferred_issue_numbers"), flush=True)
    print("scope_authority:", data.get("scope_authority"), "freeze:", data.get("candidate_freeze_authority"), flush=True)
    missing = [n for n in (170, 174, 175, 176) if n not in data.get("required_issue_numbers", [])]
    print("claimed-but-absent:", missing, flush=True)
else:
    print("manifest not found at", man, flush=True)
