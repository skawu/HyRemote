import json, subprocess, urllib.request

REPO = "skawu/HyRemote"
token = subprocess.run(["gh", "auth", "token"], capture_output=True, text=True).stdout.strip()
H = {"Authorization": "Bearer %s" % token, "Accept": "application/vnd.github+json",
     "Content-Type": "application/json", "User-Agent": "hyremote-agent"}

BODY = """## Correction, and the hang narrows: the transport-stall scenario itself is sound

**First, a correction to my earlier comment.** I wrote that the delivery hang appeared "with the global allocation
counter disarmed". That was an inference from the trace, and it was **wrong**: the delivery loop in that version ran
*inside* the armed region (the counter was armed immediately before it), so an injected `bad_alloc` was in play. The
trace could not distinguish the two, and I should not have written the stronger claim.

**Re-measured without arming anything**, same scenario (`capacity = 1`, `ProducerThrottle`, `EnqueueGate` closed and
installed, one parking delivery verified with `waitForEntered`, then four deliveries):

```
parking frame delivered=1, entered=1
after deliver 0  delivered=1 framesRejectedOverflow=0 framesAccepted=2
after deliver 1  delivered=1 framesRejectedOverflow=1 framesAccepted=2
after deliver 2  delivered=1 framesRejectedOverflow=2 framesAccepted=2
after deliver 3  delivered=1 framesRejectedOverflow=3 framesAccepted=2
deliveries done; opening gate and stopping
stopped
```

So the scenario is **good**: the overflow branch is reached deterministically from the first extra delivery, the
mailbox stops accepting at capacity as the policy says, and nothing blocks. The case then runs to completion.

**Two candidates eliminated by reading, not by guessing**:

- it is not a lock held across the transport call (`runDispatcher` calls `transport->enqueueFrame` at
  `session.cpp:683` with no Core lock held; the lock follows at `:691`);
- it is not a blocking mailbox push: `detail::Mailbox::push` (`src/core/src/detail/mailbox.hpp`) is non-blocking by
  construction - it returns `Stored` / `StoredAfterDroppingOldest` / `RejectedOverflow` / `RejectedClosed`, and only
  `waitForWork()`, which the dispatch worker uses, ever waits on the condition variable.

**What that leaves**: the hang is **injection-induced** - the same deliveries complete when nothing is armed and did
not when a single allocation was made to fail - so an allocation failure somewhere on the delivery/dispatch path can
leave a delivery stuck. Which position, and therefore which mechanism, is not established yet; the sweep cannot say,
because the process simply stops making progress.

**Where this leaves this PR**: unchanged - the change and its evidence are as committed, and the instrumentation was
kept out of the tree (the test file is identical to the committed version). The follow-up is now sharper and cheaper:
bisect the armed position, or instrument the delivery call per step, to find the first position that stops making
progress.

**A related fact worth having while it is fresh**: with `capacity = 1` and `ProducerThrottle`, `framesAccepted` stops
at 2 - one frame owned by the dispatch worker (ADR-0003) plus one waiting - which matches the documented ownership
accounting exactly.
"""

req = urllib.request.Request("https://api.github.com/repos/%s/issues/191/comments" % REPO,
                             data=json.dumps({"body": BODY}).encode("utf-8"), headers=H, method="POST")
with urllib.request.urlopen(req, timeout=60) as r:
    got = json.loads(r.read().decode("utf-8"))
print("comment url:", got["html_url"])

p = subprocess.run(["git", "status", "--short"], cwd="f:/workspace/hyremote/hyremote-wt-146",
                   capture_output=True, text=True)
print("worktree:", p.stdout.strip() or "(clean)")
p = subprocess.run(["git", "diff", "--stat"], cwd="f:/workspace/hyremote/hyremote-wt-146",
                   capture_output=True, text=True)
print("diff:", p.stdout.strip() or "(none - test file matches the committed version)")
