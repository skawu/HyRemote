import json, subprocess, urllib.request

REPO = "skawu/HyRemote"
WT = "f:/workspace/hyremote/hyremote-wt-146"
token = subprocess.run(["gh", "auth", "token"], capture_output=True, text=True).stdout.strip()
H = {"Authorization": "Bearer %s" % token, "Accept": "application/vnd.github+json",
     "Content-Type": "application/json", "User-Agent": "hyremote-agent"}


def run(*args):
    p = subprocess.run(list(args), cwd=WT, capture_output=True, text=True)
    return p.returncode, (p.stdout + p.stderr).strip()


rc, diff = run("git", "diff", "--stat")
rc2, status = run("git", "status", "--short")
print("diff:", diff or "(empty - test file matches the committed version)")
print("status:", status or "(clean)")
if diff.strip():
    print("ABORT: the tree does not match the committed state, so the comment below would describe a tree that"
          " no longer exists")
    raise SystemExit(1)

BODY = """## Hang bisected: it needs an injected failure on a thread other than the caller

Two controlled runs, same stall scenario (`capacity = 1`, `ProducerThrottle`, closed `EnqueueGate`, one parking
delivery verified with `waitForEntered(1)`), differing only in **which thread may consume the injection**:

**A. Injection armed per delivery on the calling thread** (four deliveries, one session): every delivery completes.
`framesRejectedOverflow` stays 0 for the positions consumed before the callback and the session stays usable - the
runs finish, nothing blocks.

**B. Injection restricted to the arming thread, loop shape that previously hung** (fresh session per position, gate
closed, four deliveries, then `open()` and `stop()`), eight positions:

```
position 0..7: arming for this thread only
position N:    deliveries done escaped=0 overflow=3 accepted=2
position N:    stopping / stopped
all positions completed
```

**So the hang requires an allocation failure on a thread other than the one driving the delivery.** With the
injection pinned to the caller, the shape that hung before completes all eight positions. That is a mechanism, not a
guess: it was the one variable changed between the hanging run and this one.

**Why that matters more than the hang itself**: the other threads here are Core's own workers, and a worker's thread
function has no top-level exception guard - `runDispatcher` (`session.cpp:655`) catches only around
`transport->enqueueFrame` (`:682-688`), while its bookkeeping afterwards (`:691-698`, which builds a `SessionError`)
is unprotected. An exception escaping a `std::thread` function terminates the process, which is exactly what #164's
acceptance criterion 2 forbids ("no path can terminate the process because of an avoidable internal
allocation/formatting exception"). The hang observed earlier is therefore a symptom of an unprotected worker path,
not of the callback boundary.

**Two concrete follow-ups, both cheap now**:

1. Ship the overflow case with the injection pinned to the calling thread (`std::atomic<std::thread::id>` compared in
   the replaced `operator new`), assert the boundary's own message appears, and revert the boundary as the
   two-direction probe. With the injection pinned this is deterministic and it finally makes the boundary proven
   rather than defensive - the gap this PR currently records as open.
2. Decide what a worker-thread exception should do. Criterion 2 says it may not terminate the process, so the
   worker needs a boundary of its own, and the failure it reports has to follow the same error model as the callback
   boundary.

**Tree state**: the instrumentation never entered the tree - `git diff` against the committed version is empty and the
working tree is clean, verified immediately before posting this.
"""

req = urllib.request.Request("https://api.github.com/repos/%s/issues/191/comments" % REPO,
                             data=json.dumps({"body": BODY}).encode("utf-8"), headers=H, method="POST")
with urllib.request.urlopen(req, timeout=60) as r:
    got = json.loads(r.read().decode("utf-8"))
print("comment url:", got["html_url"])
