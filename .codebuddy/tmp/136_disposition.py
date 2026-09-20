"""Record the non-V1 disposition of the #134 qualification track on #136 and verify the stored body."""

import json
import pathlib
import subprocess

REPO = "skawu/HyRemote"
TMP = pathlib.Path(r"f:/workspace/hyremote/HyRemote/.codebuddy/tmp")


def gh(args):
    p = subprocess.run(["gh"] + args, capture_output=True, text=True, encoding="utf-8")
    if p.returncode != 0:
        print("!! gh rc=%d %s" % (p.returncode, p.stderr.strip()[:400]), flush=True)
    return p.stdout


body = """## Disposition (product ruling, 2026-09-19): this stays a non-V1 track, deliberately left open and unmerged

The V1 mandatory set does not include #134 or this matrix PR. `.github/release/v1-mandatory-issues.json` classifies #134
as a qualification-infrastructure draft track that is **explicitly not a V1 support claim**, and the product side has
ruled that V1.0.0.0 stops taking new scope.

Concretely:

- **#136 and #138 stay open and are not merged into the V1 candidate line.** They are outside the release gate, and
  nothing in the V1 declaration depends on them.
- **They must not become a second mandatory release train.** If a later authority wants the upstream application matrix to
  gate a release, that needs an explicit scope ruling and a manifest change - not a CI workflow that starts running on its
  own.
- The work is **not discarded**: `tests/third_party/matrix.json`, the six workflow definitions and the L2 build attempts
  remain the starting point for the post-V1 qualification track.
- #138 targets this PR's branch, so the two move together or not at all.

Recorded here so the next reader does not have to reconstruct why a PR that adds only workflows is deliberately unmerged
during a convergence phase.
"""

payload = TMP / "136-disposition.json"
payload.write_text(json.dumps({"body": body}, ensure_ascii=False), encoding="utf-8")
out = gh(["api", "-X", "POST", "repos/%s/issues/136/comments" % REPO, "--input", str(payload)])
try:
    print("posted:", json.loads(out)["html_url"], flush=True)
except Exception:
    print("post unreadable:", out[:200], flush=True)

items = json.loads(gh(["api", "repos/%s/issues/136/comments?per_page=100" % REPO]))
last = items[-1]
print("verify: comments=%d last_id=%s len=%d matches=%s"
      % (len(items), last["id"], len(last["body"]), last["body"].strip() == body.strip()), flush=True)
