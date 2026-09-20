import json, subprocess, urllib.request

REPO = "skawu/HyRemote"
WT = "f:/workspace/hyremote/hyremote-wt-146"
token = subprocess.run(["gh", "auth", "token"], capture_output=True, text=True).stdout.strip()
H = {"Authorization": "Bearer %s" % token, "Accept": "application/vnd.github+json",
     "Content-Type": "application/json", "User-Agent": "hyremote-agent"}


def run(*args):
    p = subprocess.run(list(args), cwd=WT, capture_output=True, text=True)
    return p.returncode, (p.stdout + p.stderr).strip()


print("branch:", run("git", "branch", "--show-current")[1])
run("git", "add", "src/core/src/session.cpp")
print(run("git", "status", "--short")[1])
rc, out = run("git", "diff", "--cached", "--stat")
print(out)

MSG1 = "fix(core): contain exceptions at the Core worker thread entry points (#164)"
MSG2 = ("Acceptance criterion 2 of #164. A worker thread is a std::thread entry point, so an exception escaping "
        "one terminates the process, which the criterion forbids for an avoidable internal allocation or "
        "formatting failure. runDispatcher guarded only the transport call (`session.cpp:682-688`) while its "
        "bookkeeping afterwards - which constructs a SessionError - was unprotected, and runScheduler had no "
        "guard at all.\n\n"
        "The guard sits at the launch site, so it covers the whole thread function: the startup handshake, every "
        "bookkeeping step and the transport call. A worker that fails reports a non-recoverable "
        "ComponentFailure, so the session faults rather than staying Running with a dead worker, and the thread "
        "function returns normally, which keeps the owning join well defined.\n\n"
        "Disclosed rather than claimed: this boundary is **not** exercised by a deterministic test yet. The case "
        "written to exercise it - a stalled transport with ProducerThrottle, where the injected failure is free "
        "to be consumed by any thread - instead reproduced the delivery hang this PR already tracks, and showed "
        "the hang is in the teardown rather than in a worker thread, so that case was not shipped. The suite is "
        "76/76 in the QML+QPA configuration and all ten gates pass; this is defensive hardening for criterion 2 "
        "and the follow-up to exercise it is recorded on the PR.")
rc, out = run("git", "commit", "-q", "-m", MSG1, "-m", MSG2)
print("commit rc=%d" % rc)
if rc != 0:
    print(out[-400:])
    raise SystemExit(1)
print(run("git", "log", "--oneline", "-3")[1])
rc, out = run("git", "push")
print("push rc=%d" % rc)
if rc != 0:
    print(out[-300:])
    raise SystemExit(1)

BODY = """## Worker-thread boundary added, and the hang is not what I thought

The bisect left one conclusion standing - the hang needs an injected failure on a thread other than the caller - so
the obvious next move was to guard the worker threads and see whether the hang goes away. It does **not**, and that
is worth recording precisely.

**Added in this commit**: `Impl::atWorkerBoundary` + `Impl::reportWorkerFailure` (`src/core/src/session.cpp`), applied
at the two `std::thread` launch sites. Worker threads are `std::thread` entry points, so an escaping exception
terminates the process - forbidden by acceptance criterion 2. `runDispatcher` guarded only
`transport->enqueueFrame` (`:682-688`) while its bookkeeping afterwards (`:691-698`, which constructs a
`SessionError`) was unprotected, and `runScheduler` had no guard at all. A failing worker now reports a
non-recoverable `ComponentFailure`, so the session faults instead of staying `Running` with a dead worker, and the
thread function returns normally so the owning join stays well defined.

**The hang survived that change**, so it is not an escaping worker exception. Re-running the stall scenario with the
injection free to be consumed by any thread, instrumented per step, position 0:

```
[WORKER-PATH] position 0: deliveries begin
[WORKER-PATH] position 0: deliveries done escaped=1
```

The deliveries **complete**, with one exception escaping into the caller. That escape comes from the harness, not
from the boundary: `FakeCaptureSource::deliverInternal` copies its `std::function` handler before invoking it
(`support/fakes.hpp`), and that copy allocates. The delivery therefore fails *before* the callback is entered, so the
session never sees the completion - and what hangs is the next thing that runs, which is `~RunningSession` calling
`stop()` during unwinding of the failed check. The `[WORKER-PATH] ... opening the stall and stopping` line never
prints, which places the blockage inside teardown rather than in the delivery loop.

**Two hypotheses eliminated, both by reading rather than guessing**:

- *an exception escaping a worker thread* - the boundary above removes that possibility, and the hang remains;
- *in-flight capture requests never draining* - `impl.inFlight` is reset during teardown (`session.cpp:674` and
  `:848`), so teardown does not wait for it.

**What that leaves, stated as the next step rather than a conclusion**: `stop()` blocks when a capture completion is
lost before it reaches the session. Finding what teardown waits on that such a completion would have satisfied is the
way in, and it is a product-level hazard worth its own attention, because a `stop()` that never returns means an
application that cannot shut down. It belongs with the lifecycle-bounds work in this issue rather than in this
commit.

**This commit is therefore defensive hardening, not a proven fix**, and the test case that would have exercised it
was deliberately left out of the tree rather than shipped as a hanging test. Suite 76/76 in the QML+QPA
configuration; ten gates pass. The test file is unchanged by this commit.
"""

req = urllib.request.Request("https://api.github.com/repos/%s/issues/191/comments" % REPO,
                             data=json.dumps({"body": BODY}).encode("utf-8"), headers=H, method="POST")
with urllib.request.urlopen(req, timeout=60) as r:
    got = json.loads(r.read().decode("utf-8"))
print("comment url:", got["html_url"])

req = urllib.request.Request("https://api.github.com/repos/%s/pulls/191" % REPO, headers=H)
with urllib.request.urlopen(req, timeout=60) as r:
    pr = json.loads(r.read().decode("utf-8"))
print("PR head:", pr["head"]["sha"][:8], "state:", pr["state"], "base:", pr["base"]["ref"])
