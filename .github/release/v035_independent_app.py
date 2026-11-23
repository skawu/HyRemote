#!/usr/bin/env python3
"""Standalone V0.3.5.0 GitHub App required-check issuer.

Deploy this file as a PINNED, independently managed artifact, not from any
release pull request's checkout. A GitHub Actions check is NOT equivalent.
Network TLS and webhook ingress are provided by an external managed host.
"""
from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import time
from threading import Lock, Thread
import urllib.error
import urllib.parse
import urllib.request

REPO = "skawu/HyRemote"
BASE_SHA = "b0680d5df4f76d8b84b02a1b950030a65a89ddca"
HEAD_SHA = "681b51b1defe486cfa5069b2a785485f17fb2bd2"
BASE_REF = "release/v0.3.5.0"
HEAD_REF = "feature/589-release-preparation"
RELEASE_TAG = "v0.3.5.0"
CONTEXT = "v035-independent-source-admission"
EXPECTED = {
    "src/runtime/src/detail/watermark_store.cpp": "5931fd66b739607393389b8b058046215f1f9772",
    "src/runtime/tests/test_watermark_policy.cpp": "54aefaac3c3eb451b88c4ff885bd8fe4cf9f9d4e",
    ".github/scripts/release_correction.py": "f9e159d1975cb044be26b3fdfbaf9f0b4927b61f",
    ".github/workflows/git-flow-policy.yml": "9e991b5644b591ee9d705470f033e9446e92fb84",
}
ALLOWED_ACTIONS = frozenset({"opened", "reopened", "synchronize", "ready_for_review", "edited"})
MAX_WEBHOOK_BYTES = 262144
# Single-issuer deployment serializes PR success and tag-revocation checks.
ISSUANCE_LOCK = Lock()


class Denied(ValueError):
    """An explicit fail-closed policy denial."""


def need(condition, detail):
    if not condition:
        raise Denied(detail)


def verify_signature(body, signature, secret):
    if not isinstance(signature, str) or not signature.startswith("sha256="):
        return False
    expected = "sha256=" + hmac.new(secret.encode("utf-8"), body, hashlib.sha256).hexdigest()
    return hmac.compare_digest(signature, expected)


def flatten_tree(data):
    need(data.get("truncated") is False, "truncated Git tree")
    entries = {}
    for item in data.get("tree", []):
        if item.get("type") == "tree":
            continue
        path = item.get("path")
        need(isinstance(path, str) and path and path not in entries,
             "duplicate or invalid Git tree path")
        entries[path] = (item.get("sha"), item.get("mode"), item.get("type"))
    need(bool(entries), "empty Git tree")
    return entries


def validate(pr, comparison, base_commit, head_commit, old_tree, new_tree, *, tag_exists):
    need(pr.get("state") == "open", "PR not open")
    base, head = pr.get("base") or {}, pr.get("head") or {}
    need(base.get("ref") == BASE_REF and base.get("sha") == BASE_SHA,
         "wrong/stale protected release base")
    need(head.get("ref") == HEAD_REF and head.get("sha") == HEAD_SHA,
         "unexpected candidate branch or commit")
    need((base.get("repo") or {}).get("full_name") == REPO, "foreign base repository")
    need((head.get("repo") or {}).get("full_name") == REPO, "foreign candidate repository")
    need(not tag_exists, "release tag already exists")
    need(base_commit.get("sha") == BASE_SHA and head_commit.get("sha") == HEAD_SHA,
         "incorrect commit identity")
    need([p.get("sha") for p in head_commit.get("parents", [])] == [BASE_SHA],
         "candidate is not direct child of frozen release")
    need(comparison.get("status") == "ahead" and comparison.get("behind_by") == 0
         and comparison.get("ahead_by") == 1 and comparison.get("total_commits") == 1,
         "unexpected ancestry/commit count")
    need([c.get("sha") for c in comparison.get("commits", [])] == [HEAD_SHA],
         "unreviewed commit history")
    files = comparison.get("files") or []
    need(len(files) == len(EXPECTED) and {f.get("filename") for f in files} == set(EXPECTED),
         "changed file set differs from frozen four-path allowlist")
    for f in files:
        need(f.get("status") == "modified" and f.get("sha") == EXPECTED[f["filename"]],
             "changed file differs from reviewed blob")
    old, new = flatten_tree(old_tree), flatten_tree(new_tree)
    delta = {path for path in old.keys() | new.keys() if old.get(path) != new.get(path)}
    need(delta == set(EXPECTED), "complete Git object tree changed outside four paths")
    for path, sha in EXPECTED.items():
        need(path in old and new.get(path) == (sha, "100644", "blob"),
             "unreviewed blob, mode or type: " + path)
    # Complete equality of ALL remaining entries enforces unchanged CMake VERSION,
    # release notes, sources, scripts, manifests and permissions.


def ruleset_payload(app_id):
    need(isinstance(app_id, int) and app_id > 0, "real installed GitHub App ID required")
    return {
        "name": "V0.3.5.0 independent source admission",
        "target": "branch", "enforcement": "active",
        "bypass_actors": [],
        "conditions": {"ref_name": {
            "include": ["refs/heads/release/v0.3.5.0"], "exclude": []}},
        "rules": [{"type": "required_status_checks", "parameters": {
            "strict_required_status_checks_policy": True,
            "do_not_enforce_on_create": False,
            "required_status_checks": [
                {"context": CONTEXT, "integration_id": app_id}],
        }}],
    }


def gh(method, path, token, payload=None):
    need(path.startswith("/"), "invalid API path")
    request = urllib.request.Request(
        "https://api.github.com" + path,
        method=method,
        data=json.dumps(payload).encode("utf-8") if payload is not None else None,
        headers={
            "Authorization": "Bearer " + token,
            "Accept": "application/vnd.github+json",
            "Content-Type": "application/json",
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "HyRemote-v035-independent-admission",
        },
    )
    with urllib.request.urlopen(request, timeout=20) as response:
        return json.load(response)


def _state_file(suffix):
    # Must be an operator-controlled PERSISTENT writable volume, never a
    # candidate checkout, ephemeral container layer, or GitHub Actions artifact.
    root = os.environ.get("APP_STATE_DIR")
    need(bool(root), "durable APP_STATE_DIR is required for required checks")
    directory = Path(root)
    need(directory.is_dir(), "APP_STATE_DIR must be an existing mounted directory")
    return directory / ("v035-" + HEAD_SHA[:16] + suffix)


def _durable_create(path):
    try:
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        return
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(("revoked " + HEAD_SHA + "\\n").encode("ascii"))
            stream.flush()
            os.fsync(stream.fileno())
        parent_fd = os.open(str(path.parent), os.O_RDONLY)
        try:
            os.fsync(parent_fd)
        finally:
            os.close(parent_fd)
    except Exception:
        # Even an incompletely written marker is treated as revoked.
        raise


def is_revoked():
    return _state_file(".revoked").exists()


def mark_revoked():
    _durable_create(_state_file(".revoked"))


def is_revocation_acked():
    return _state_file(".acked").exists()


def attempt_revocation_once():
    """Durably queued one-way REVOKE; a temporary API error cannot erase it."""
    if not is_revoked() or is_revocation_acked():
        return 0
    token = create_installation_token()
    count = revoke_on_tag_push(token)
    if count:
        _durable_create(_state_file(".acked"))
    return count


def revocation_retry_loop():
    delay = 5
    while True:
        try:
            with ISSUANCE_LOCK:
                if is_revoked() and not is_revocation_acked():
                    delivered = attempt_revocation_once()
                    delay = 20 if delivered else min(300, delay * 2)
                else:
                    delay = 30
        except Exception as error:
            # Do not lose signed revocation or produce a success. Persisted
            # marker survives process restarts and triggers the next attempt.
            print("revocation retry pending:", type(error).__name__, flush=True)
            delay = min(300, delay * 2)
        time.sleep(delay)


def create_installation_token():
    # The private key is kept OUTSIDE git, GitHub Actions, logs and PR artifacts.
    import jwt  # PyJWT[crypto] in the deployment-only locked environment
    app_id = int(os.environ["GITHUB_APP_ID"])
    install_id = int(os.environ["GITHUB_INSTALLATION_ID"])
    pem = Path(os.environ["GITHUB_APP_PRIVATE_KEY_FILE"]).read_bytes()
    now = int(time.time())
    encoded = jwt.encode({"iat": now - 60, "exp": now + 540, "iss": str(app_id)},
                         pem, algorithm="RS256")
    result = gh("POST", "/app/installations/" + str(install_id) + "/access_tokens",
                encoded, {"permissions": {
                    "checks": "write", "contents": "read", "pull_requests": "read"},
                    "repositories": ["HyRemote"]})
    token = result.get("token")
    need(isinstance(token, str) and bool(token), "GitHub App token missing")
    return token


def has_release_tag(token):
    try:
        gh("GET", "/repos/" + REPO + "/git/ref/tags/" + RELEASE_TAG, token)
        return True
    except urllib.error.HTTPError as exc:
        if exc.code == 404:
            return False
        raise


def read_live(pr_number, token):
    root = "/repos/" + REPO
    pr = gh("GET", root + "/pulls/" + str(pr_number), token)
    base = (pr.get("base") or {}).get("sha")
    head = (pr.get("head") or {}).get("sha")
    need(isinstance(base, str) and isinstance(head, str), "missing PR SHA")
    base_commit = gh("GET", root + "/git/commits/" + base, token)
    head_commit = gh("GET", root + "/git/commits/" + head, token)
    compare = gh("GET", root + "/compare/" + base + "..." + head, token)
    old = gh("GET", root + "/git/trees/" + base_commit["tree"]["sha"] + "?recursive=1", token)
    new = gh("GET", root + "/git/trees/" + head_commit["tree"]["sha"] + "?recursive=1", token)
    return pr, compare, base_commit, head_commit, old, new


def check_pr(pr_number, event_sha, token):
    """Emit a check ONLY on the current PR head, never a stale webhook SHA."""
    root = "/repos/" + REPO
    pr = gh("GET", root + "/pulls/" + str(pr_number), token)
    current_sha = (pr.get("head") or {}).get("sha")
    if event_sha is not None and current_sha != event_sha:
        return "STALE"
    if (pr.get("base") or {}).get("ref") != BASE_REF:
        return "UNRELATED"
    need(isinstance(current_sha, str) and len(current_sha) == 40, "invalid PR SHA")
    conclusion, note = "success", "Exact frozen source, four blobs and entire Git tree verified."
    try:
        # Reject ABA: an unreviewed A cannot receive success if a transient
        # read_live happened to see approved HEAD_SHA in the middle.
        need(current_sha == HEAD_SHA, "initial PR HEAD is not approved frozen SHA")
        need(not is_revoked(), "durable tag revocation forbids reauthorization")
        facts = read_live(pr_number, token)
        need((facts[0].get("head") or {}).get("sha") == current_sha,
             "audited facts and check target have different HEADs")
        tagged = has_release_tag(token)
        if tagged:
            mark_revoked()
        validate(*facts, tag_exists=tagged)
    except Denied as error:
        conclusion, note = "failure", str(error)
    # Any REST error is propagated, never converted to success.
    fresh = gh("GET", root + "/pulls/" + str(pr_number), token)
    if ((fresh.get("head") or {}).get("sha") != current_sha
            or (fresh.get("base") or {}).get("sha") != BASE_SHA):
        return "STALE"
    if conclusion == "success" and has_release_tag(token):
        # Persist first, then write FAILURE so later tag deletion or replay
        # cannot resurrect an obsolete SUCCESS.
        mark_revoked()
        conclusion, note = "failure", "release tag appeared before check publication"
    if conclusion == "success" and is_revoked():
        conclusion, note = "failure", "durable tag revocation forbids reauthorization"
    gh("POST", root + "/check-runs", token, {
        "name": CONTEXT, "head_sha": current_sha,
        "status": "completed", "conclusion": conclusion,
        "external_id": "v035-exact-" + BASE_SHA[:12] + "-" + current_sha[:12],
        "output": {"title": "Independent V0.3.5.0 source admission",
                   "summary": note},
    })
    return conclusion.upper()


def decide_event(event_name, payload, configured_installation_id):
    if event_name != "pull_request" or payload.get("action") not in ALLOWED_ACTIONS:
        return None
    if (payload.get("repository") or {}).get("full_name") != REPO:
        return None
    need((payload.get("installation") or {}).get("id") == configured_installation_id,
         "wrong GitHub App installation")
    pr = payload.get("pull_request") or {}
    if (pr.get("base") or {}).get("ref") != BASE_REF:
        return None
    number = pr.get("number")
    sha = (pr.get("head") or {}).get("sha")
    need(isinstance(number, int) and number > 0
         and isinstance(sha, str) and len(sha) == 40,
         "invalid PR event identity")
    return number, sha


def decide_tag_event(event_name, payload, configured_installation_id):
    """Signed tag creation must revoke a previously issued success."""
    if event_name != "push" or payload.get("ref") != "refs/tags/" + RELEASE_TAG:
        return False
    if payload.get("deleted") is True:
        return False  # deletion does not restore authorization
    if (payload.get("repository") or {}).get("full_name") != REPO:
        return False
    need((payload.get("installation") or {}).get("id") == configured_installation_id,
         "wrong GitHub App installation")
    return True


def revoke_on_tag_push(token):
    # Trust the signed push as an explicit REVOKE; do not rely on GitHub's
    # eventually-consistent tag-ref lookup before posting FAILURE.
    query = urllib.parse.urlencode({
        "state": "open", "base": BASE_REF,
        "head": "skawu:" + HEAD_REF, "per_page": 100,
    })
    prs = gh("GET", "/repos/" + REPO + "/pulls?" + query, token)
    count = 0
    for pr in prs:
        if ((pr.get("head") or {}).get("sha") != HEAD_SHA
                or (pr.get("base") or {}).get("ref") != BASE_REF):
            continue
        fresh = gh("GET", "/repos/" + REPO + "/pulls/" + str(pr["number"]), token)
        if (fresh.get("head") or {}).get("sha") != HEAD_SHA:
            continue
        gh("POST", "/repos/" + REPO + "/check-runs", token, {
            "name": CONTEXT, "head_sha": HEAD_SHA, "status": "completed",
            "conclusion": "failure",
            "external_id": "v035-tag-revoked-" + HEAD_SHA[:12],
            "output": {
                "title": "V0.3.5.0 admission revoked by signed tag push",
                "summary": "Tag mutation invalidates prior admission. "
                           "Requalification requires an explicit source review.",
            },
        })
        count += 1
    return count


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != "/github/webhook":
            self.send_error(404)
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= MAX_WEBHOOK_BYTES:
                self.send_error(413)
                return
            body = self.rfile.read(length)
            secret = os.environ["GITHUB_WEBHOOK_SECRET"]
            if not verify_signature(body, self.headers.get("X-Hub-Signature-256"), secret):
                self.send_error(401)
                return
            event = json.loads(body)
            event_name = self.headers.get("X-GitHub-Event", "")
            installed = int(os.environ["GITHUB_INSTALLATION_ID"])
            if decide_tag_event(event_name, event, installed):
                with ISSUANCE_LOCK:
                    # FIRST durably record the signed event, BEFORE any REST
                    # operation. Retrying is our job: GitHub does not
                    # automatically redeliver failed webhooks.
                    mark_revoked()
                    try:
                        attempt_revocation_once()
                    except Exception as error:
                        self.log_error("durable revocation queued: %s",
                                       type(error).__name__)
                self.send_response(202)
                self.end_headers()
                return
            match = decide_event(event_name, event, installed)
            if match is None:
                self.send_response(202)
                self.end_headers()
                return
            with ISSUANCE_LOCK:
                outcome = check_pr(match[0], match[1], create_installation_token())
            self.send_response(200 if outcome != "STALE" else 202)
            self.end_headers()
        except (Denied, ValueError, KeyError, OSError, urllib.error.URLError) as error:
            # No GitHub token or private key is ever logged.
            self.log_error("admission unavailable: %s", type(error).__name__)
            self.send_error(503)

    def log_message(self, fmt, *args):
        # Default http.server logs only request path/status, never secrets.
        super().log_message(fmt, *args)


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--serve", action="store_true")
    group.add_argument("--check-pr", type=int)
    group.add_argument("--print-ruleset", type=int, metavar="APP_ID")
    args = parser.parse_args()
    if args.print_ruleset:
        print(json.dumps(ruleset_payload(args.print_ruleset), ensure_ascii=False, indent=2))
    elif args.check_pr is not None:
        print(check_pr(args.check_pr, None, create_installation_token()))
    else:
        for name in ("GITHUB_APP_ID", "GITHUB_INSTALLATION_ID",
                     "GITHUB_APP_PRIVATE_KEY_FILE", "GITHUB_WEBHOOK_SECRET",
                     "APP_STATE_DIR"):
            need(bool(os.environ.get(name)), "missing required deployment setting " + name)
        need(_state_file(".revoked").parent.is_dir(), "durable state mount unavailable")
        host = os.environ.get("APP_LISTEN_HOST", "127.0.0.1")
        port = int(os.environ.get("APP_LISTEN_PORT", "8097"))
        Thread(target=revocation_retry_loop, daemon=True).start()
        ThreadingHTTPServer((host, port), Handler).serve_forever()


if __name__ == "__main__":
    main()
