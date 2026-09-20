import json, subprocess, urllib.request

REPO = "skawu/HyRemote"
token = subprocess.run(["gh", "auth", "token"], capture_output=True, text=True).stdout.strip()
H = {"Authorization": "Bearer %s" % token, "Accept": "application/vnd.github+json",
     "Content-Type": "application/json", "User-Agent": "hyremote-agent"}

NOTE = """
---

### Mandatory-set sync (2026-09-19)

Recorded here so this authority's prose matches the machine authority.

The machine-readable V1 mandatory set, `.github/release/v1-mandatory-issues.json`, was updated by **PR #177**
(`3e8f20a`, "mandatory set delta and unclassified-reference gate", #157) to include **#170**, **#174**, **#175** and
**#176**, each carrying the note `"V1.0.0.0 blocker confirmed by the product-owner scope review of 2026-09-19."` That
same PR extended `tests/release-readiness/check_release_authority_policy.cmake` by 46 lines with the drift comparison
and the unclassified-reference gate, and `.github/workflows/git-flow-policy.yml` now reads `required_issue_numbers`
directly from that file, so a V1 blocker cannot be silently absent from the machine set.

State of the integration line, measured on `develop` rather than assumed: **PR #106 is merged** (`b760427`, merged
2026-09-19T03:20:28Z), the branch `feature/104-v1-ga-acceptance-matrix` no longer exists, and `cc1adca` is an
ancestor of `origin/develop` (`64140c8`). The V1 integration line is therefore **`develop`**; `main` is at
`c3f4991`.
"""

for number in (33, 95, 107, 156):
    req = urllib.request.Request("https://api.github.com/repos/%s/issues/%d" % (REPO, number), headers=H)
    with urllib.request.urlopen(req, timeout=60) as r:
        issue = json.loads(r.read().decode("utf-8"))
    body = issue["body"] or ""
    if "Mandatory-set sync (2026-09-19)" in body:
        print("#%d already synced - skipped" % number)
        continue
    new_body = body.rstrip() + "\n" + NOTE
    api_req = urllib.request.Request("https://api.github.com/repos/%s/issues/%d" % (REPO, number),
                                     data=json.dumps({"body": new_body}).encode("utf-8"), headers=H, method="PATCH")
    with urllib.request.urlopen(api_req, timeout=60) as r:
        got = json.loads(r.read().decode("utf-8"))
    print("#%d updated; body has the note: %s" % (number, "Mandatory-set sync (2026-09-19)" in got["body"]))
