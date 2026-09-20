import json, subprocess, urllib.request

REPO = "skawu/HyRemote"
WT = "f:/workspace/hyremote/hyremote-wt-106"
CAND = "origin/feature/104-v1-ga-acceptance-matrix"
token = subprocess.run(["gh", "auth", "token"], capture_output=True, text=True).stdout.strip()
H = {"Authorization": "Bearer %s" % token, "Accept": "application/vnd.github+json",
     "Content-Type": "application/json", "User-Agent": "hyremote-agent"}


def api(path, method="GET", payload=None):
    req = urllib.request.Request(
        "https://api.github.com/repos/%s/%s" % (REPO, path),
        data=json.dumps(payload).encode("utf-8") if payload is not None else None,
        headers=H, method=method)
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.loads(r.read().decode("utf-8"))


def git(*args):
    p = subprocess.run(["git"] + list(args), cwd=WT, capture_output=True, text=True)
    return p.returncode, p.stdout.strip()


print("=" * 20, "PR #146 (already-decided port work)", "=" * 20)
pr = api("pulls/146")
print("state            =", pr["state"])
print("title            =", pr["title"])
print("base             =", pr["base"]["ref"])
print("head             =", pr["head"]["ref"], pr["head"]["sha"][:8])
print("merged / mergedAt=", pr["merged"], pr.get("merged_at"))
print("mergeable_state  =", pr.get("mergeable_state"))
print("merge_commit     =", (pr.get("merge_commit_sha") or "")[:8])

git("fetch", "origin", "--prune", "--quiet")
for sha in ("4112ae6", "256c52c"):
    for ref in ("origin/develop", "origin/main", CAND, "origin/" + pr["head"]["ref"]):
        rc, _ = git("merge-base", "--is-ancestor", sha, ref)
        print("%s ancestor of %-48s = %s" % (sha, ref, rc == 0))

rc, mb = git("merge-base", CAND, pr["head"]["sha"])
print("merge-base(candidate, pr146) =", mb[:8] if rc == 0 else "n/a")
rc, files146 = git("show", "--name-only", "--format=", "4112ae6")
files146 = set(f for f in files146.splitlines() if f.strip())
rc, cand_changed = git("diff", "--name-only", mb, "cc1adca")
cand_changed = set(f for f in cand_changed.splitlines() if f.strip())
print("#146 files = %d, candidate-side changed files = %d" % (len(files146), len(cand_changed)))
overlap = sorted(files146 & cand_changed)
print("overlap (real conflict risk if replayed) = %d" % len(overlap))
for f in overlap:
    print("   ", f)

print("=" * 20, "closing #179 as duplicate", "=" * 20)
COMMENT = """Closing as a duplicate - the port design here was already decided, implemented and verified, and I did not check
before opening this.

**Where it already lives: PR #146.**
- `4112ae6` "fix(v1): make 5921 the product default port across all three integration modes" - **23 files / 37 lines**,
  covering exactly the file set this issue re-derived as its "impact surface" (`README.md`, the 8 documents, the 8 example
  files, the four test-assertion files, `src/remoteaccess/src/remote_access.cpp`,
  `integrations/qpa/hyremote_qpa_remote_controller.hpp`).
- `256c52c` "feat(v1): make the default listener port configurable at configure time" -
  `set(HYREMOTE_DEFAULT_PORT 5921 CACHE STRING "Default loopback listener port shared by all integration modes")`, with a
  range check that **fails configure** on an out-of-range value and `add_compile_definitions(HYREMOTE_DEFAULT_PORT=...)`
  publishing it to every target. That is the build-time criterion this issue had begun to "add".
- Verified by running, not by reading: `READY 5930` (C++), `READY 5931` (QML), `port: 5932` (QPA) with an explicit port,
  and `READY 5921` / `READY 5921` / `port: 5921` with no port argument, a viewer connecting in each case; 72/72 ctest,
  CI 7/7 on `4112ae6`.

**Two residual items, recorded here rather than lost.**

1. `examples/remote-support-showcase/README.md:35` still illustrates the CLI as
   `hyremote-remote-support-showcase --auto-start --remote-input --port 5901 --test-seconds 30`. That file is **not** in
   `4112ae6`'s 23-file list, so the stale `5901` survives. It is the E5 acceptance example, so a wrong port in its
   documentation harms acceptance repeatability.
2. Already-recorded follow-up: shipped examples pass their own explicit port default, so an example launched without
   `--port` does not inherit the configure-time `HYREMOTE_DEFAULT_PORT`; making the examples inherit it was noted as a
   deliberate small follow-up, not an oversight.

**Why I opened this anyway**, recorded so the mistake is visible rather than excused: I read only the checked-out
candidate tree. `git grep` searches the working tree, not all branches, so "5921 occurs nowhere" and "no
`HYREMOTE_*PORT*` option exists" were true of that checkout and false about the project - `git log --all -S` finds both
in minutes. The design was also sitting in `MEMORY.md`'s port sections the whole time."""
c = api("issues/179/comments", "POST", {"body": COMMENT})
print("comment id =", c["id"], c["html_url"])
api("issues/179", "PATCH", {"state": "closed", "state_reason": "not_planned"})
back = api("issues/179")
print("state =", back["state"], "| reason =", back.get("state_reason"))
