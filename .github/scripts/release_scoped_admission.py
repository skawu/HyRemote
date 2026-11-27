#!/usr/bin/env python3
"""V0.3.5.0 one-off source admission; execute only from trusted develop.

This verifier never checks out or executes candidate source. The Actions
check-run is advisory until a separately sourced required check is enforced.
"""
from __future__ import annotations

import argparse
import copy
import json
import os
import sys
import urllib.error
import urllib.request

REPO = "skawu/HyRemote"
NUMBER = 876
BASE = "b0680d5df4f76d8b84b02a1b950030a65a89ddca"
HEAD = "f59719061ee09ffd7b9bd925b3bb156bfe335bab"
BRANCH = "feature/789-release-preparation"
RELEASE = "release/v0.3.5.0"
COMMITS = [
    "cf70d5574ed6055ac29b4951ddf5f664b7879bc2",
    "c2b2d331b9d177bba430c424edfaf1949c330dbc",
    "2fc19f05c080405c20c3ce0d08b1a00036ec62de",
    HEAD,
]
BLOBS = {
    "src/runtime/src/automatic/composite_target.cpp": "424029b453870b64714816017a3b7363353879c8",
    "src/runtime/src/quick/quick_image_frame_storage.hpp": "b5a58e921ae1857d1e4569383d18c0d6456aeced",
    "src/runtime/src/quick/quick_target.cpp": "b70cfb2c9cf260768111d60e4d52aa4bbd54f5b8",
    "src/runtime/src/transport/rfb_transport.cpp": "d6cfebb237fbd7c405b9e9da29233f4f6102260d",
    "src/runtime/tests/test_rfb_wire_robustness.cpp": "e51e172670109129d552cd676ca5f3eace098b1a",
}
# Distinct name avoids confusing an advisory GitHub Actions check with a
# future required release-scoped-admission check from an independent GitHub App.
CHECK_NAME = "release-scoped-admission-audit"


class Denied(ValueError):
    pass


def need(ok, why):
    if not ok:
        raise Denied(why)


def flat_tree(obj):
    need(obj.get("truncated") is False, "truncated Git tree")
    out = {}
    for entry in obj.get("tree", []):
        if entry.get("type") == "tree":
            continue
        path = entry.get("path")
        need(isinstance(path, str) and path not in out, "invalid/duplicate tree path")
        out[path] = (entry.get("sha"), entry.get("mode"), entry.get("type"))
    return out


def validate(pr, diff, before, after):
    need(pr.get("number") == NUMBER and pr.get("state") == "open", "wrong/closed PR")
    base, head = pr.get("base") or {}, pr.get("head") or {}
    need((base.get("repo") or {}).get("full_name") == REPO, "foreign base repo")
    need((head.get("repo") or {}).get("full_name") == REPO, "foreign head repo")
    need(base.get("ref") == RELEASE and base.get("sha") == BASE, "wrong release base")
    need(head.get("ref") == BRANCH and head.get("sha") == HEAD, "unreviewed head")
    need(diff.get("status") == "ahead" and diff.get("behind_by") == 0, "not ancestor")
    need(diff.get("ahead_by") == 4 and diff.get("total_commits") == 4, "unexpected history length")
    need([c.get("sha") for c in diff.get("commits", [])] == COMMITS, "unreviewed commit history")
    files = diff.get("files") or []
    need(len(files) == len(BLOBS) and {x.get("filename") for x in files} == set(BLOBS),
         "unexpected changed paths")
    for item in files:
        path = item["filename"]
        status = "added" if path.endswith("quick_image_frame_storage.hpp") else "modified"
        need(item.get("status") == status and item.get("sha") == BLOBS[path],
             "unreviewed changed-file status/blob")
    old, new = flat_tree(before), flat_tree(after)
    delta = {p for p in old.keys() | new.keys() if old.get(p) != new.get(p)}
    need(delta == set(BLOBS), "complete Git tree changed outside approved five files")
    for path, blob in BLOBS.items():
        need(new.get(path) == (blob, "100644", "blob"), "bad source blob/mode: " + path)
    need("src/runtime/src/quick/quick_image_frame_storage.hpp" not in old,
         "storage header already existed on frozen base")
    # Exact identity of ALL other Git objects includes VERSION, pending
    # bilingual release notes, workflow/policy, and security configuration.


def rest(method, path, token, body=None):
    url = "https://api.github.com/repos/" + REPO + "/" + path
    payload = json.dumps(body).encode("utf-8") if body is not None else None
    req = urllib.request.Request(url, method=method, data=payload, headers={
        "Authorization": "Bearer " + token,
        "Accept": "application/vnd.github+json",
        "Content-Type": "application/json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "HyRemote-release-admission-audit",
    })
    with urllib.request.urlopen(req, timeout=25) as response:
        return json.load(response)


def live(token):
    pr = rest("GET", "pulls/876", token)
    need(pr.get("base", {}).get("sha") == BASE and pr.get("head", {}).get("sha") == HEAD,
         "PR SHA moved")
    diff = rest("GET", "compare/" + BASE + "..." + HEAD, token)
    old_commit = rest("GET", "git/commits/" + BASE, token)
    new_commit = rest("GET", "git/commits/" + HEAD, token)
    old = rest("GET", "git/trees/" + old_commit["tree"]["sha"] + "?recursive=1", token)
    new = rest("GET", "git/trees/" + new_commit["tree"]["sha"] + "?recursive=1", token)
    validate(pr, diff, old, new)
    fresh = rest("GET", "pulls/876", token)
    need(fresh.get("base", {}).get("sha") == BASE and fresh.get("head", {}).get("sha") == HEAD,
         "PR changed during admission check")


def run_event(event_path):
    with open(event_path, encoding="utf-8") as file:
        event = json.load(file)
    run = event.get("workflow_run") or {}
    if run.get("event") != "pull_request" or run.get("name") != "Git Flow policy":
        print("SKIP non-PR Git Flow run")
        return
    associated = {p.get("number") for p in run.get("pull_requests", [])}
    if NUMBER not in associated:
        if (run.get("head_branch") != BRANCH or
                (run.get("head_repository") or {}).get("full_name") != REPO):
            print("SKIP unrelated PR")
            return
        # Some GitHub workflow_run payloads omit pull_requests. A branch
        # match only triggers revalidation of this one immutable PR/head.
    token = os.environ["GH_TOKEN"]
    verdict, summary = "success", "Reviewed source DAG and both full Git blob trees match."
    try:
        live(token)
    except (Denied, KeyError, ValueError, urllib.error.URLError) as error:
        verdict, summary = "failure", "Denied: " + type(error).__name__ + ": " + str(error)
    # The check run is placed on the exact PR head, not the default-branch
    # workflow_run commit; this does NOT make it a mandatory or distinct-App gate.
    check = rest("POST", "check-runs", token, {
        "name": CHECK_NAME, "head_sha": HEAD, "status": "completed",
        "conclusion": verdict,
        "external_id": "v035-876-" + BASE[:12] + "-" + HEAD[:12],
        "output": {
            "title": "V0.3.5.0 scoped source audit",
            "summary": summary + "\nAdvisory only; requires independent mandatory source before release.",
        },
    })
    print(verdict.upper(), "check run id", check.get("id"))
    need(verdict == "success", summary)


def self_test():
    pr = {"number": NUMBER, "state": "open",
          "base": {"ref": RELEASE, "sha": BASE, "repo": {"full_name": REPO}},
          "head": {"ref": BRANCH, "sha": HEAD, "repo": {"full_name": REPO}}}
    files = [{"filename": p, "status": "added" if p.endswith("quick_image_frame_storage.hpp")
              else "modified", "sha": v} for p, v in BLOBS.items()]
    diff = {"status": "ahead", "behind_by": 0, "ahead_by": 4, "total_commits": 4,
            "commits": [{"sha": s} for s in COMMITS], "files": files}
    baseline = {
        "CMakeLists.txt": ("unchanged-version", "100644", "blob"),
        "docs/releases/v0.3.5.0.md": ("unchanged-notes", "100644", "blob"),
        ".github/workflows/git-flow-policy.yml": ("unchanged-workflow", "100644", "blob"),
    }
    baseline.update({p: ("base-" + p, "100644", "blob") for p in BLOBS
                     if not p.endswith("quick_image_frame_storage.hpp")})
    candidate = dict(baseline)
    candidate.update({p: (sha, "100644", "blob") for p, sha in BLOBS.items()})
    def tree(m):
        return {"truncated": False, "tree": [
            {"path": p, "sha": s, "mode": mode, "type": kind}
            for p, (s, mode, kind) in m.items()]}
    old, new = tree(baseline), tree(candidate)
    validate(pr, diff, old, new)

    cases = []
    def mutate(name, index, modify):
        args = [copy.deepcopy(x) for x in (pr, diff, old, new)]
        modify(args[index])
        cases.append((name, args))
    mutate("wrong release", 0, lambda x: x["base"].update(ref="release/v0.3.6.0"))
    mutate("wrong base", 0, lambda x: x["base"].update(sha="moved"))
    mutate("wrong head", 0, lambda x: x["head"].update(sha="moved"))
    mutate("wrong branch", 0, lambda x: x["head"].update(ref="feature/789-other"))
    mutate("other repo", 0, lambda x: x["head"]["repo"].update(full_name="other/repo"))
    mutate("closed PR", 0, lambda x: x.update(state="closed"))
    mutate("wrong PR", 0, lambda x: x.update(number=999))
    mutate("not ancestor", 1, lambda x: x.update(status="diverged", behind_by=1))
    mutate("extra commit", 1, lambda x: x.update(ahead_by=5))
    mutate("wrong commit", 1, lambda x: x["commits"][0].update(sha="changed"))
    mutate("extra diff file", 1, lambda x: x["files"].append(
        {"filename": "src/new.cpp", "status": "added", "sha": "bad"}))
    mutate("wrong diff blob", 1, lambda x: x["files"][0].update(sha="bad"))
    mutate("missing tree entry", 3, lambda x: x["tree"].pop())
    mutate("modified notes", 3, lambda x: next(e for e in x["tree"]
           if e["path"] == "docs/releases/v0.3.5.0.md").update(sha="evil"))
    mutate("modified root version", 3, lambda x: next(e for e in x["tree"]
           if e["path"] == "CMakeLists.txt").update(sha="evil"))
    mutate("modified workflow", 3, lambda x: next(e for e in x["tree"]
           if e["path"] == ".github/workflows/git-flow-policy.yml").update(sha="evil"))
    mutate("unreviewed new script", 3, lambda x: x["tree"].append(
        {"path": ".github/scripts/release_correction.py", "sha": "evil",
         "mode": "100644", "type": "blob"}))
    mutate("bad mode", 3, lambda x: next(e for e in x["tree"]
           if e["path"] in BLOBS).update(mode="100755"))
    mutate("truncated tree", 3, lambda x: x.update(truncated=True))
    for name, args in cases:
        try:
            validate(*args)
        except Denied:
            continue
        raise AssertionError("Rejected test case incorrectly PASSED: " + name)
    print("PASS 1 positive +", len(cases), "adversarial negative cases")


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--self-test", action="store_true")
    group.add_argument("--workflow-run-event")
    args = parser.parse_args()
    if args.self_test:
        self_test()
    else:
        run_event(args.workflow_run_event)


if __name__ == "__main__":
    try:
        main()
    except (Denied, AssertionError, KeyError, ValueError, RuntimeError,
            urllib.error.URLError) as error:
        print("DENIED:", error, file=sys.stderr)
        raise SystemExit(1)
