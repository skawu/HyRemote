"""Post the E3 declarative-transition evidence to #109 and verify the stored body."""

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


body = (TMP / "109-e3.md").read_text(encoding="utf-8")
payload = TMP / "109-e3.json"
payload.write_text(json.dumps({"body": body}, ensure_ascii=False), encoding="utf-8")

print("posting to #109 (len=%d)" % len(body), flush=True)
out = gh(["api", "-X", "POST", "repos/%s/issues/109/comments" % REPO, "--input", str(payload)])
try:
    print("posted:", json.loads(out)["html_url"], flush=True)
except Exception:
    print("post result unreadable:", out[:250], flush=True)

items = json.loads(gh(["api", "repos/%s/issues/109/comments?per_page=100" % REPO]))
last = items[-1]
print("verify: comments=%d last_id=%s len=%d matches_source=%s"
      % (len(items), last["id"], len(last["body"]), last["body"].strip() == body.strip()), flush=True)
