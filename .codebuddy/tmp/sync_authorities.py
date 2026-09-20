"""Open the governance PR and write the mandatory-set delta back to the five authorities that repeat the set."""

import json
import pathlib
import subprocess

REPO = "skawu/HyRemote"
TMP = pathlib.Path(r"f:/workspace/hyremote/HyRemote/.codebuddy/tmp")
WT = pathlib.Path(r"f:/workspace/hyremote/hyremote-wt-106")


def gh(args):
    p = subprocess.run(["gh"] + args, capture_output=True, text=True, encoding="utf-8")
    if p.returncode != 0:
        print("!! gh rc=%d %s" % (p.returncode, p.stderr.strip()[:400]), flush=True)
    return p.stdout


pr_body = """## What this changes

The product-owner scope review of 2026-09-19 confirmed four further V1.0.0.0 blockers and required the single machine-readable manifest - and every authority that repeats it - to include them, with a regression check so a future V1-labelled blocker cannot silently exist outside the manifest.

**Mandatory set: 21 -> 25 entries**, adding:

- **#170** secure configuration and authenticated-session operations;
- **#174** listener address-family and reachability contract;
- **#175** maintained-viewer interoperability against the final secure/compressed profile;
- **#176** reproducible Windows/Linux SDK release assets with integrity evidence.

**A classification list, not a weakener.** The manifest also gains `classified_referenced_issue_numbers` covering the numbers that appear in the release authorities without being V1 work items (14, 74, 90, 91, 95, 106, 134, 147, 156, 166), each with its reason in `notes`. Classifying them is deliberate: it is what makes the next unclassified number visible.

**The gate now enforces both.** `tests/release-readiness/check_release_authority_policy.cmake` pins the classified list as well, and scans seven in-tree authority documents for issue references - `docs/release-candidate-checklist.md`, `docs/v1-ga-acceptance.md`, `docs/development-roadmap.md`, `docs/known-limitations.md`, `docs/compatibility.md`, `docs/v1-physical-acceptance.md`, `docs/security-model.md`. Any number that is neither mandatory, embedded-deferred nor classified fails the gate and is named in the failure.

## Verification, run locally on this branch

- all **ten** standalone release-readiness gates PASS, and the authority gate reports the 25-entry manifest
  (`manifest=9;...;165;170;174;175;176;`);
- **negative test**: appending a fabricated `#999` reference to `docs/security-model.md` makes the gate **fail** and name
  the offending number and its document; restoring the document makes it **pass** again, with byte-identical content.

## What this does not do

It does not touch the frozen three-mode / one-runtime architecture, and it does not make any of the four new blockers
pass - they are now **visible to the gate**, which is precisely what was missing. `#157` was closed earlier the same day
after its five criteria were verified as they then stood; the review's correction arrived afterwards, so that close was
premature, and `#157` has been reopened and this is the first part of the correction.

Refs #157 #170 #174 #175 #176 #33 #95 #107 #106 #156
"""

pr_body_file = TMP / "pr-governance.md"
pr_body_file.write_text(pr_body, encoding="utf-8")

print("--- creating PR ---", flush=True)
print(gh(["pr", "create", "--repo", REPO, "--base", "feature/104-v1-ga-acceptance-matrix",
          "--head", "governance/v1-mandatory-set-delta",
          "--title", "ci(v1): include #170/#174/#175/#176 in the mandatory set and block unclassified authority references (#157)",
          "--body-file", str(pr_body_file)]).strip()[:200], flush=True)

comment = """## V1 mandatory-set delta applied in the repository, so this authority and the machine gate agree again

The product-owner scope review of 2026-09-19 (recorded on #157) confirmed four further V1.0.0.0 blockers, and required the machine-readable manifest plus every authority that repeats it to include them, with a regression check so a future V1-labelled blocker cannot exist outside the manifest.

Applied now, on branch `governance/v1-mandatory-set-delta` (base: the V1 candidate line):

| Item | Effect |
| --- | --- |
| mandatory set | **21 -> 25** entries: adds **#170**, **#174**, **#175**, **#176** |
| manifest | gains `classified_referenced_issue_numbers` for the numbers that appear in the release authorities without being V1 work items (14, 74, 90, 91, 95, 106, 134, 147, 156, 166), each with a reason in `notes` |
| gate | `check_release_authority_policy.cmake` pins the classified list too, and scans seven in-tree authority documents for issue references; anything neither mandatory, embedded-deferred nor classified fails the gate and is named |

Verified locally: all ten standalone release-readiness gates PASS (the authority gate reports the 25-entry manifest), and a negative test that appends a fabricated `#999` reference to `docs/security-model.md` fails the gate and names the number, while restoring the document passes with byte-identical content.

Two honest notes: the frozen three-mode / one-runtime architecture is untouched, and none of the four new blockers has become easier by this change - they are now **visible to the machine gate**, which is what the review found missing. Also, #157 was closed earlier on 2026-09-19 after its five criteria were verified as they then stood; the review's correction arrived afterwards, so that close was premature, and #157 has been reopened.
"""

for issue in (33, 95, 107, 106, 156):
    payload = TMP / ("sync-%d.json" % issue)
    payload.write_text(json.dumps({"body": comment}, ensure_ascii=False), encoding="utf-8")
    out = gh(["api", "-X", "POST", "repos/%s/issues/%d/comments" % (REPO, issue), "--input", str(payload)])
    try:
        print("#%d -> %s" % (issue, json.loads(out)["html_url"]), flush=True)
    except Exception:
        print("#%d post unreadable: %s" % (issue, out[:150]), flush=True)
