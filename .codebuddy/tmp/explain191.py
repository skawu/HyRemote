import json, subprocess, urllib.request

REPO = "skawu/HyRemote"
token = subprocess.run(["gh", "auth", "token"], capture_output=True, text=True).stdout.strip()
H = {"Authorization": "Bearer %s" % token, "Accept": "application/vnd.github+json",
     "Content-Type": "application/json", "User-Agent": "hyremote-agent"}

BODY = """## The hang is explained - and my last two comments were wrong about why

Reading `teardownRun` (`src/core/src/session.cpp:621-681`) end to end closes this, and the answer is not what I
said before. I am correcting it here rather than leaving a wrong explanation in place.

**What teardown actually does**, in order: publish the terminal state, `gate->closeAndDrain()` (`:655`),
`stopComponentQuietly(source)` (`:657`), `mailbox->close()` (`:660`), **join both workers** (`:663-666`),
`stopComponentQuietly(transport)` (`:669`), and only then, at `:676`, `impl.teardownStarted = false;`.

**The hang, explained**: in my case the failing assertion aborted before `stall.open()`, so the dispatch worker was
still parked inside the **test fake's** `EnqueueGate::enter()`. Teardown then reached `schedulerThread.join()` /
`dispatcherThread.join()` (`:663-666`), and that join cannot return while the worker is waiting for a gate only the
test could open. The missing `opening the stall and stopping` print was the clue I should have followed: it says the
gate was never opened, which makes the blockage a **test-harness deadlock**, not a Core defect. My earlier claims -
that the hang needed an injected failure on another thread, and that it was an escaping worker exception - were both
wrong, and the pinning experiment that "proved" the first one only ever changed how quickly the case reached its own
abort path.

**Two genuine hazards this exposed anyway, and they are the reason to keep working here**:

1. **The teardown claim leaks on any exception.** `teardownStarted` is released only at `:676`, and nothing between
   `:651` and `:676` is guarded - `gate->closeAndDrain()`, `mailbox->close()`, both `join()` calls (which can throw
   `std::system_error`) and the two `notify_all()`s. If any of them throws, the claim stays set, the state stays
   `Stopping`, and **every later `stop()` blocks forever** on the claim wait at `:570`
   (`cv.wait(lock, [&impl] { return !impl.teardownStarted; })`). `stop()` is supposed to be idempotent, so this is a
   real defect and it is fixable structurally - release the claim and publish the terminal state on every path,
   rather than only on the happy one.
2. **Teardown joins workers that may be blocked inside the transport.** The comment at `:679-680` states the
   contract: "No Core lock is held across the transport call: the hand-off is a bounded post/enqueue by contract".
   Teardown relies on that bound for its join to return. A transport that blocks violates the contract, and then
   `stop()` cannot return at all - which is a product-level outcome (an application that cannot shut down), and it
   belongs with the bounded-occupancy work in #158 rather than being solved by detaching a worker that owns Core
   state.

**Why no timeout is the answer to (2)**: bounding the join with a timeout means either the join is abandoned and the
thread later touches `impl` (undefined behaviour), or the process leaks a live thread into its own teardown. So the
honest fix is (1) structural, plus a contract-and-bound decision for the transport, which the owner has already been
asked for and has now directed: **bounded teardown, in #164, with the plan presented before it lands**.

**Consequences for this PR**: nothing changes in the committed code. The worker boundary stays (criterion 2 requires
it whether or not it addresses this hang), and the case that produced the hang stays out of the tree. The next
commit will carry the claim-release fix with a deterministic test - inject a failure inside a teardown step, then
require that a second `stop()` still returns.
"""

req = urllib.request.Request("https://api.github.com/repos/%s/issues/191/comments" % REPO,
                             data=json.dumps({"body": BODY}).encode("utf-8"), headers=H, method="POST")
with urllib.request.urlopen(req, timeout=60) as r:
    got = json.loads(r.read().decode("utf-8"))
print("comment url:", got["html_url"])
