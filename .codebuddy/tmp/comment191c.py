import json, subprocess, urllib.request

REPO = "skawu/HyRemote"
token = subprocess.run(["gh", "auth", "token"], capture_output=True, text=True).stdout.strip()
H = {"Authorization": "Bearer %s" % token, "Accept": "application/vnd.github+json",
     "Content-Type": "application/json", "User-Agent": "hyremote-agent"}

BODY = """## Rebased onto the merged candidate line, and the boundary proof is not achievable this way

**Rebase**: `develop` moved to `23b5edd` when **#187 (#159)** merged, and this branch touched the same file, so the
branch is now rebuilt on top of it - commits `129b152` (callback boundary) and `b63f6ed` (worker boundary). Full
suite **76/76** in the QML+QPA configuration, all **ten** gates pass, and the boundary test itself is 3/3.

**The proof attempt failed, and it is not shipped.** I tried to turn the callback boundary from defensive into
proven by injecting on the one path that allocates inside `onFrameReady` - the `ProducerThrottle` overflow branch,
whose diagnostic is far longer than the small-string buffer - with the injection **pinned to the delivering thread**
via an `atomic<std::thread::id>` compared in the replaced `operator new`, and swept **0..31**. Across every position
the boundary never reported: no swept allocation position lands inside `onFrameReady` on that path, so the injected
failure either lands in the harness's own pre-callback allocations or never reaches the callback's allocation at all.

Rather than weaken the assertion into something that passes without proving anything, the case was dropped: the
branch carries the **same test file as before**, and this PR's body keeps stating that the boundary is defensive
rather than proven. That gap is real and it is recorded here instead of being papered over.

**What would actually close it**, for whoever picks this up: a deterministic seam *inside* the callback - for example
a statistics or fault-reporting hook that a test can make fail - or an internal allocation the test can force on that
path. Until then, the honest statement is the one in the body.

**State**: branch rebased and pushed; suite and gates green; the test file unchanged by this rebase.
"""

req = urllib.request.Request("https://api.github.com/repos/%s/issues/191/comments" % REPO,
                             data=json.dumps({"body": BODY}).encode("utf-8"), headers=H, method="POST")
with urllib.request.urlopen(req, timeout=60) as r:
    got = json.loads(r.read().decode("utf-8"))
print("comment url:", got["html_url"])

req = urllib.request.Request("https://api.github.com/repos/%s/pulls/191" % REPO, headers=H)
with urllib.request.urlopen(req, timeout=60) as r:
    pr = json.loads(r.read().decode("utf-8"))
print("PR head:", pr["head"]["sha"][:8], "base:", pr["base"]["ref"], "state:", pr["state"],
      "mergeable:", pr["mergeable"])
