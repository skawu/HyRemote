#!/usr/bin/env python3
"""Trusted-default-branch preflight for the ONE unpublished V0.3.5.0 #1047 repair.

Never checkout or execute release-PR content. This is ADVISORY until a
separately sourced REQUIRED check is enforced for protected release branches.
Run in the default-branch workflow_run job, not in the candidate's workflow.
"""
from __future__ import annotations

import argparse
import copy
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request

REPO = "skawu/HyRemote"
BASE_REF = "release/v0.3.5.0"
BASE_SHA = "b0680d5df4f76d8b84b02a1b950030a65a89ddca"
HEAD_REF = "feature/589-release-preparation"
TAG = "v0.3.5.0"
CHECK = "v035-trusted-watermark-audit"  # advisory, NOT independently required
BLOBS = {
    "src/runtime/src/detail/watermark_store.cpp":
        "5931fd66b739607393389b8b058046215f1f9772",
    "src/runtime/tests/test_watermark_policy.cpp":
        "54aefaac3c3eb451b88c4ff885bd8fe4cf9f9d4e",
    ".github/scripts/release_correction.py":
        "f9e159d1975cb044be26b3fdfbaf9f0b4927b61f",
    ".github/workflows/git-flow-policy.yml":
        "9e991b5644b591ee9d705470f033e9446e92fb84",
}


class Denied(ValueError):
    pass


def need(condition, message):
    if not condition:
        raise Denied(message)


def tree_map(obj):
    need(obj.get("truncated") is False, "truncated recursive Git tree")
    result = {}
    for entry in obj.get("tree", []):
        if entry.get("type") == "tree":
            continue
        p = entry.get("path")
        need(isinstance(p, str) and p not in result, "bad/duplicate tree path")
        result[p] = (entry.get("sha"), entry.get("mode"), entry.get("type"))
    return result


def validate(pr, diff, old_tree, new_tree, *, tag_exists):
    need(not tag_exists, "already published/tagged")
    need(pr.get("state") == "open", "not an open PR")
    base, head = pr.get("base") or {}, pr.get("head") or {}
    need((base.get("repo") or {}).get("full_name") == REPO, "foreign base repo")
    need((head.get("repo") or {}).get("full_name") == REPO, "foreign head repo")
    need(base.get("ref") == BASE_REF and base.get("sha") == BASE_SHA, "stale release base")
    need(head.get("ref") == HEAD_REF, "wrong feature branch")
    sha = head.get("sha")
    need(isinstance(sha, str) and len(sha) == 40, "bad candidate head SHA")
    need(diff.get("status") == "ahead" and diff.get("behind_by") == 0,
         "PR must be a descendant of exact frozen release base")
    need(diff.get("ahead_by") == 1 and diff.get("total_commits") == 1,
         "unexpected commit count")
    need([c.get("sha") for c in diff.get("commits", [])] == [sha], "unreviewed DAG")
    diff_files = diff.get("files") or []
    need(len(diff_files) == len(BLOBS), "wrong number of changed files")
    need({f.get("filename") for f in diff_files} == set(BLOBS), "unexpected changed file")
    for f in diff_files:
        need(f.get("status") == "modified"
             and f.get("sha") == BLOBS[f["filename"]], "unreviewed diff blob")
    old, new = tree_map(old_tree), tree_map(new_tree)
    delta = {p for p in old.keys() | new.keys() if old.get(p) != new.get(p)}
    need(delta == set(BLOBS), "unexpected complete Git tree delta")
    for p, sha in BLOBS.items():
        need(p in old and new.get(p) == (sha, "100644", "blob"),
             "wrong source/governance blob or file mode: " + p)
    # All other file modes, versions, notes, workflows, secrets and source remain
    # identical to the accepted b068 Git tree by complete-tree equality.


def api(method, path, token, body=None):
    url = "https://api.github.com/repos/" + REPO + "/" + path
    payload = json.dumps(body).encode("utf-8") if body is not None else None
    req = urllib.request.Request(url, data=payload, method=method, headers={
        "Authorization": "Bearer " + token,
        "Accept": "application/vnd.github+json",
        "Content-Type": "application/json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "HyRemote-trusted-v035-watermark-audit",
    })
    with urllib.request.urlopen(req, timeout=25) as response:
        return json.load(response)


def tag_exists(token):
    try:
        api("GET", "git/ref/tags/" + TAG, token)
        return True
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return False
        raise


def live(token):
    query = urllib.parse.urlencode({
        "state": "open", "base": BASE_REF, "head": "skawu:" + HEAD_REF,
        "per_page": 100,
    })
    prs = api("GET", "pulls?" + query, token)
    need(len(prs) == 1, "expected one matching release correction PR")
    pr = api("GET", "pulls/" + str(prs[0]["number"]), token)
    base = pr["base"]["sha"]
    head = pr["head"]["sha"]
    # No checkout or invocation of a candidate-supplied workflow/validator.
    diff = api("GET", "compare/" + base + "..." + head, token)
    base_commit = api("GET", "git/commits/" + base, token)
    head_commit = api("GET", "git/commits/" + head, token)
    old_tree = api("GET", "git/trees/" + base_commit["tree"]["sha"] + "?recursive=1", token)
    new_tree = api("GET", "git/trees/" + head_commit["tree"]["sha"] + "?recursive=1", token)
    validate(pr, diff, old_tree, new_tree, tag_exists=tag_exists(token))
    fresh = api("GET", "pulls/" + str(pr["number"]), token)
    need(fresh["base"]["sha"] == base and fresh["head"]["sha"] == head,
         "release correction PR changed during audit")
    need(not tag_exists(token), "version tag appeared during audit")
    return pr, head


def run_event(event_path):
    with open(event_path, encoding="utf-8") as stream:
        event = json.load(stream)
    run = event.get("workflow_run") or {}
    if run.get("event") != "pull_request" or run.get("name") != "Git Flow policy":
        print("SKIP non-PR Git Flow workflow")
        return
    # workflow_run on trusted default branch is a trigger, not an authority.
    if (run.get("head_branch") != HEAD_REF
            or (run.get("head_repository") or {}).get("full_name") != REPO):
        print("SKIP unrelated PR branch")
        return
    token = os.environ["GH_TOKEN"]
    # Fail closed: failed/preempted topology never earns a PASS admission.
    result, detail = "success", "Exact reviewed security and governance blobs, complete Git trees and untagged state match."
    try:
        need(run.get("conclusion") == "success", "Git Flow workflow did not pass")
        pr, sha = live(token)
    except (Denied, KeyError, ValueError, urllib.error.URLError) as error:
        result, detail = "failure", type(error).__name__ + ": " + str(error)
        query = urllib.parse.urlencode({
            "state": "open", "base": BASE_REF, "head": "skawu:" + HEAD_REF,
        })
        prs = api("GET", "pulls?" + query, token)
        if len(prs) != 1:
            raise Denied("no unique matching PR to report failure: " + detail)
        sha = prs[0]["head"]["sha"]
    api("POST", "check-runs", token, {
        "name": CHECK,
        "head_sha": sha,
        "status": "completed",
        "conclusion": result,
        "external_id": "v035-trusted-" + BASE_SHA[:12] + "-" + sha[:12],
        "output": {
            "title": "V0.3.5.0 exact source admission (ADVISORY)",
            "summary": detail + "\nNot a REQUIRED independent GitHub App check; cannot approve release.",
        },
    })
    print(result.upper(), CHECK, sha)
    need(result == "success", detail)


def self_test():
    pr = {"state": "open", "number": 1,
          "base": {"ref": BASE_REF, "sha": BASE_SHA, "repo": {"full_name": REPO}},
          "head": {"ref": HEAD_REF, "sha": "a" * 40, "repo": {"full_name": REPO}}}
    old = {"CMakeLists.txt": ("old", "100644", "blob"),
           "docs/releases/v0.3.5.0.md": ("notes", "100644", "blob")}
    old.update({p: ("old:" + p, "100644", "blob") for p in BLOBS})
    new = dict(old)
    new.update({p: (sha, "100644", "blob") for p, sha in BLOBS.items()})
    def tree(value):
        return {"truncated": False, "tree": [
            {"path": p, "sha": item[0], "mode": item[1], "type": item[2]}
            for p, item in value.items()]}
    diff = {"status": "ahead", "behind_by": 0, "ahead_by": 1, "total_commits": 1,
            "commits": [{"sha": "a" * 40}],
            "files": [{"filename": p, "sha": sha, "status": "modified"}
                      for p, sha in BLOBS.items()]}
    base_case = [pr, diff, tree(old), tree(new)]
    validate(*base_case, tag_exists=False)
    mutations = []
    def case(name, index, change, tag=False):
        c = copy.deepcopy(base_case)
        change(c[index])
        mutations.append((name, c, tag))
    case("already tagged", 0, lambda x: None, True)
    case("wrong branch", 0, lambda x: x["head"].update(ref="evil"))
    case("moved release base", 0, lambda x: x["base"].update(sha="b" * 40))
    case("wrong head repo", 0, lambda x: x["head"]["repo"].update(full_name="other/repo"))
    case("closed PR", 0, lambda x: x.update(state="closed"))
    case("not descendant", 1, lambda x: x.update(status="diverged", behind_by=1))
    case("extra commit", 1, lambda x: x.update(ahead_by=2, total_commits=2))
    case("bad commit SHA", 1, lambda x: x["commits"][0].update(sha="b" * 40))
    case("unreviewed diff file", 1, lambda x: x["files"].append(
        {"filename": "src/runtime/src/transport/rfb_transport.cpp", "sha": "x", "status": "added"}))
    case("unreviewed product blob", 1, lambda x: x["files"][0].update(sha="x"))
    case("extra product tree entry", 3, lambda x: x["tree"].append(
        {"path": "src/new.cpp", "sha": "evil", "mode": "100644", "type": "blob"}))
    case("edited notes", 3, lambda x: next(v for v in x["tree"] if v["path"].endswith("v0.3.5.0.md")).update(sha="bad"))
    case("edited VERSION", 3, lambda x: next(v for v in x["tree"] if v["path"] == "CMakeLists.txt").update(sha="bad"))
    case("candidate governance altered", 3, lambda x: next(v for v in x["tree"] if v["path"] == ".github/workflows/git-flow-policy.yml").update(sha="bad"))
    case("bad executable bit", 3, lambda x: next(v for v in x["tree"] if v["path"] in BLOBS).update(mode="100755"))
    case("truncated tree", 3, lambda x: x.update(truncated=True))
    case("missing regression test", 3, lambda x: x["tree"].remove(next(
        v for v in x["tree"] if v["path"] == "src/runtime/tests/test_watermark_policy.cpp")))
    for name, case_args, tagged in mutations:
        try:
            validate(*case_args, tag_exists=tagged)
        except Denied:
            continue
        raise AssertionError("negative unexpectedly passed: " + name)
    print("PASS trusted V035 audit: 1 positive +", len(mutations), "negative cases")


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
    except (Denied, AssertionError, KeyError, ValueError, urllib.error.URLError) as error:
        print("DENIED:", error, file=sys.stderr)
        raise SystemExit(1)
