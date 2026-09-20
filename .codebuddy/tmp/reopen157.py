"""Reopen #157 because the product-owner correction arrived after this agent closed it, then show the in-tree
prose surfaces the anti-leak regression check could key on."""

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


comment = """## Reopened: this issue was closed before the mandatory-set correction landed

I closed this as completed on 2026-09-19 after verifying the five acceptance criteria as they stood. The product-owner
cycle then added a further required correction (comment `5737767879`): the single machine-readable V1 mandatory manifest,
and every release authority that repeats it, must include **#170 #174 #175 #176**, plus a regression check so a future
V1-labelled blocking authority cannot silently exist outside the manifest.

Closing was therefore premature relative to that requirement, and the issue is reopened so the correction is applied
against it rather than left to drift. The earlier verification of the five criteria still stands; what changed is the
scope the criteria are measured against.

Order of work now being applied:

1. add #170/#174/#175/#176 to `.github/release/v1-mandatory-issues.json` and to the pinned expected list in
   `tests/release-readiness/check_release_authority_policy.cmake`, then re-run the release-readiness gates locally;
2. sync the prose authorities that repeat the set (#33/#95/#107/#106/#156) and the in-tree release documents;
3. add the regression check the review asks for, so a number asserted as a V1 blocker in the authorities cannot be
   absent from the manifest.
"""

open("./.codebuddy/tmp/reopen-comment.txt", "w", encoding="utf-8").write(comment)
payload = WT / ".codebuddy-tmp-reopen.json"
payload.write_text(json.dumps({"body": comment}, ensure_ascii=False), encoding="utf-8")

print("--- reopening #157 ---", flush=True)
print(gh(["issue", "reopen", "157", "--repo", REPO]).strip()[:200], flush=True)
print(gh(["api", "-X", "POST", "repos/%s/issues/157/comments" % REPO, "--input", str(payload)]).strip()[:120], flush=True)

state = json.loads(gh(["api", "repos/%s/issues/157" % REPO]))
print("verify #157 state=%s" % state["state"], flush=True)

print("", flush=True)
print("=== in-tree surfaces that repeat the mandatory set ===", flush=True)
for rel in ("docs/release-candidate-checklist.md", "docs/v1-ga-acceptance.md"):
    p = WT / rel
    if not p.exists():
        print("%s: MISSING" % rel, flush=True)
        continue
    text = p.read_text(encoding="utf-8", errors="replace")
    hits = [l.strip() for l in text.splitlines() if "#" in l and any(ch.isdigit() for ch in l) and "mandatory" in l.lower()]
    print("--- %s : %d lines mentioning mandatory+numbers ---" % (rel, len(hits)), flush=True)
    for h in hits[:12]:
        print("   " + h[:150], flush=True)
    if not hits:
        numbered = [l.strip() for l in text.splitlines() if l.count("#1") + l.count("#2") > 1]
        print("   (no 'mandatory' line with numbers; lines with several issue numbers: %d)" % len(numbered), flush=True)
        for h in numbered[:8]:
            print("   " + h[:150], flush=True)
