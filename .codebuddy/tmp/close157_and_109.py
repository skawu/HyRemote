"""Close #157 with its closing note, and record the held-input carrier ruling on #109. Both bodies are read back."""

import json
import pathlib
import subprocess

REPO = "skawu/HyRemote"
TMP = pathlib.Path(r"f:/workspace/hyremote/HyRemote/.codebuddy/tmp")


def gh(args):
    p = subprocess.run(["gh"] + args, capture_output=True, text=True, encoding="utf-8")
    if p.returncode != 0:
        print("!! gh rc=%d %s" % (p.returncode, p.stderr.strip()[:300]), flush=True)
    return p.stdout


def post(issue, body, label):
    payload = TMP / ("post-%s.json" % label)
    payload.write_text(json.dumps({"body": body}, ensure_ascii=False), encoding="utf-8")
    out = gh(["api", "-X", "POST", "repos/%s/issues/%d/comments" % (REPO, issue), "--input", str(payload)])
    try:
        print("#%d comment: %s" % (issue, json.loads(out)["html_url"]), flush=True)
    except Exception:
        print("#%d post unreadable: %s" % (issue, out[:150]), flush=True)
    items = json.loads(gh(["api", "repos/%s/issues/%d/comments?per_page=100" % (REPO, issue)]))
    last = items[-1]
    print("   verify: len=%d matches=%s" % (len(last["body"]), last["body"].strip() == body.strip()), flush=True)


close_body = """## Closing again: the mandatory-set delta and the regression check are both on `develop`

The correction this issue was reopened for is landed on the integration branch:

- `.github/release/v1-mandatory-issues.json` now includes **#170 #174 #175 #176** (mandatory set 21 -> 25 entries) plus a
  `classified_referenced_issue_numbers` list for the numbers that appear in the release authorities without being V1 work
  items;
- `tests/release-readiness/check_release_authority_policy.cmake` pins both lists **and** scans seven in-tree authority
  documents for issue references, so a V1-labelled blocker cannot exist outside the manifest;
- the prose authorities that repeat the set (#33 / #95 / #107 / #106 / #156) were updated in place;
- all of it is on **`develop`** (`b760427`, with `b23871c` and `3e8f20a` beneath it), and the ten standalone
  release-readiness gates were re-run after each merge.

The five acceptance criteria are satisfied against the current authorities. The earlier close was premature only because
this correction arrived afterwards; this one is not.

Refs #170 #174 #175 #176 #33 #95 #106 #107 #156
"""

held_body = """## Held-input cell for E2/E3: carrier ruled, and the deviation is recorded rather than hidden

**Product ruling (2026-09-19)**: the cell "explicit stop / declarative disable while a modifier is held" is carried by the
**showcase** payload (`examples/remote-support-showcase`), which has the control the other payloads lack, driven through
**UI Automation by control name** - the same path a mouse click on the checkbox takes.

**Why, measured rather than assumed**:

- `examples/quick-basic/main.cpp`'s acceptance helper **early-returns when `--remote-input` was given**
  (`if (acceptancePolicyTransitionDone || policyTransitionMs <= 0 || remoteInput) return;`), so in control mode it never runs;
- `examples/quick-basic/Main.qml` and `examples/qml-basic/Main.qml` only **display** the policy value
  (`remoteControlEnabled` / `runtimeRemoteInput`) and expose **no control that changes it**;
- therefore on E2/E3 that cell has no carrier in control mode at all, and no experiment on those payloads can satisfy it.

**How E2/E3 are recorded**: covered by a cross-payload carrier, with this deviation stated - the same disposition the E1
step-6 cell received. Their own transition evidence stands and is unaffected: E2 `5738161902` (same-process
`stop -> configure -> start`) and E3 `5738172391` (declarative `stop -> change remoteInputEnabled -> restart`). The stated
deviation is that **no modifier was held** at those two transitions.

The showcase run for this cell is next, using the same UIA entry whose `On -> Off` transition already produced
`REMOTE_INPUT disabled` followed by the payload's documented restart.
"""

print("=== #157 ===", flush=True)
post(157, close_body, "157-close")
print(gh(["issue", "close", "157", "--repo", REPO, "--reason", "completed"]).strip()[:120], flush=True)
state = json.loads(gh(["api", "repos/%s/issues/157" % REPO]))
print("verify #157 state=%s reason=%s" % (state["state"], state.get("state_reason")), flush=True)

print("", flush=True)
print("=== #109 ===", flush=True)
post(109, held_body, "109-held")
