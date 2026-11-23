"""Adversarial tests: independent App must never self-authorize release code."""
from __future__ import annotations

import copy
import hashlib
import hmac
import importlib.util
import os
import tempfile
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
PATH = ROOT / ".github" / "release" / "v035_independent_app.py"
spec = importlib.util.spec_from_file_location("v035_independent_app", PATH)
app = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = app
spec.loader.exec_module(app)


def fixtures():
    pr = {"state": "open", "number": 589,
          "base": {"ref": app.BASE_REF, "sha": app.BASE_SHA,
                   "repo": {"full_name": app.REPO}},
          "head": {"ref": app.HEAD_REF, "sha": app.HEAD_SHA,
                   "repo": {"full_name": app.REPO}}}
    old = {"CMakeLists.txt": ("version", "100644", "blob"),
           "docs/releases/v0.3.5.0.md": ("candidate-notes", "100644", "blob")}
    old.update({p: ("old-" + p, "100644", "blob") for p in app.EXPECTED})
    new = dict(old)
    new.update({p: (sha, "100644", "blob") for p, sha in app.EXPECTED.items()})
    tree = lambda data: {"truncated": False, "tree": [
        {"path": p, "sha": v[0], "mode": v[1], "type": v[2]}
        for p, v in data.items()]}
    compare = {"status": "ahead", "behind_by": 0, "ahead_by": 1,
               "total_commits": 1, "commits": [{"sha": app.HEAD_SHA}],
               "files": [{"filename": p, "status": "modified", "sha": sha}
                         for p, sha in app.EXPECTED.items()]}
    base = {"sha": app.BASE_SHA}
    head = {"sha": app.HEAD_SHA, "parents": [{"sha": app.BASE_SHA}]}
    return [pr, compare, base, head, tree(old), tree(new)]


class ExactTreeTests(unittest.TestCase):
    def test_positive(self):
        app.validate(*fixtures(), tag_exists=False)

    def test_denials(self):
        mutations = {
            "tag": lambda x: None,
            "moved base": lambda x: x[0]["base"].update(sha="a" * 40),
            "unreviewed head": lambda x: x[0]["head"].update(sha="b" * 40),
            "unreviewed branch": lambda x: x[0]["head"].update(ref="feature/evil"),
            "fork": lambda x: x[0]["head"]["repo"].update(full_name="evil/repo"),
            "closed PR": lambda x: x[0].update(state="closed"),
            "wrong commit": lambda x: x[3].update(sha="b" * 40),
            "wrong parent": lambda x: x[3]["parents"][0].update(sha="b" * 40),
            "extra commit": lambda x: x[1].update(ahead_by=2),
            "changed diff blob": lambda x: x[1]["files"][0].update(sha="other"),
            "extra diff file": lambda x: x[1]["files"].append(
                {"filename": "src/evil.cpp", "status": "added", "sha": "other"}),
            "unreviewed mode": lambda x: x[5]["tree"][2].update(mode="100755"),
            "notes changed": lambda x: next(v for v in x[5]["tree"]
                if v["path"] == "docs/releases/v0.3.5.0.md").update(sha="evil"),
            "version changed": lambda x: next(v for v in x[5]["tree"]
                if v["path"] == "CMakeLists.txt").update(sha="evil"),
            "new source file": lambda x: x[5]["tree"].append(
                {"path": "src/evil.cpp", "sha": "x", "mode": "100644", "type": "blob"}),
            "truncated": lambda x: x[5].update(truncated=True),
        }
        for name, alter in mutations.items():
            with self.subTest(name=name):
                f = fixtures()
                alter(f)
                with self.assertRaises(app.Denied):
                    app.validate(*f, tag_exists=name == "tag")

    def test_hmac(self):
        payload = b'{"action":"opened"}'
        secret = "local-only-test-key"
        sig = "sha256=" + hmac.new(secret.encode(), payload, hashlib.sha256).hexdigest()
        self.assertTrue(app.verify_signature(payload, sig, secret))
        self.assertFalse(app.verify_signature(payload + b"tampered", sig, secret))
        self.assertFalse(app.verify_signature(payload, "sha256=bad", secret))
        self.assertFalse(app.verify_signature(payload, None, secret))

    def test_ruleset_exact_source(self):
        data = app.ruleset_payload(98765)
        self.assertEqual(data["conditions"]["ref_name"]["include"],
                         ["refs/heads/release/v0.3.5.0"])
        self.assertEqual(data["bypass_actors"], [])
        rule = data["rules"][0]["parameters"]
        self.assertTrue(rule["strict_required_status_checks_policy"])
        self.assertEqual(rule["required_status_checks"],
                         [{"context": app.CONTEXT, "integration_id": 98765}])
        with self.assertRaises(app.Denied):
            app.ruleset_payload(0)


class EventAndCheckTests(unittest.TestCase):
    def setUp(self):
        # This directory represents an independently mounted durable volume.
        self._state = tempfile.TemporaryDirectory()
        self.addCleanup(self._state.cleanup)
        env = patch.dict(os.environ, {"APP_STATE_DIR": self._state.name})
        env.start()
        self.addCleanup(env.stop)

    def test_webhook_identity(self):
        pr = fixtures()[0]
        event = {"action": "opened", "repository": {"full_name": app.REPO},
                 "installation": {"id": 3}, "pull_request": pr}
        self.assertEqual(app.decide_event("pull_request", event, 3),
                         (589, app.HEAD_SHA))
        for name, alter in [
            ("not release", lambda x: x["pull_request"]["base"].update(ref="develop")),
            ("other repo", lambda x: x["repository"].update(full_name="fork/repo")),
            ("other action", lambda x: x.update(action="closed")),
        ]:
            with self.subTest(name=name):
                copy_event = copy.deepcopy(event)
                alter(copy_event)
                self.assertIsNone(app.decide_event("pull_request", copy_event, 3))
        with self.assertRaises(app.Denied):
            app.decide_event("pull_request", event, 4)

    def test_success_emits_one_exact_check(self):
        pr, cmp, base, head, old, new = fixtures()
        endpoints = {
            "/repos/" + app.REPO + "/pulls/589": pr,
            "/repos/" + app.REPO + "/git/commits/" + app.BASE_SHA: base,
            "/repos/" + app.REPO + "/git/commits/" + app.HEAD_SHA: head,
            "/repos/" + app.REPO + "/compare/" + app.BASE_SHA + "..." + app.HEAD_SHA: cmp,
        }
        posts = []
        def api(method, path, token, payload=None):
            if method == "POST":
                posts.append((path, payload))
                return {"id": 123}
            if "/git/trees/" in path:
                return old if path.endswith("old?recursive=1") else new
            return endpoints[path]
        base["tree"] = {"sha": "old"}
        head["tree"] = {"sha": "new"}
        with patch.object(app, "gh", side_effect=api), patch.object(app, "has_release_tag", return_value=False):
            result = app.check_pr(589, app.HEAD_SHA, "synthetic-token")
        self.assertEqual(result, "SUCCESS")
        self.assertEqual(len(posts), 1)
        self.assertEqual(posts[0][1]["name"], app.CONTEXT)
        self.assertEqual(posts[0][1]["head_sha"], app.HEAD_SHA)
        self.assertEqual(posts[0][1]["conclusion"], "success")

    def test_old_webhook_cannot_write_new_sha(self):
        posts = []
        pr = fixtures()[0]
        with patch.object(app, "gh", side_effect=lambda m,p,t,d=None: posts.append(p) if m == "POST" else pr):
            self.assertEqual(app.check_pr(589, "a"*40, "synthetic-token"), "STALE")
        self.assertEqual(posts, [])

    def test_branch_aba_never_grants_success_to_initial_unreviewed_head(self):
        pr = fixtures()[0]
        pr["head"]["sha"] = "a" * 40
        with patch.object(app, "gh") as api, patch.object(app, "read_live") as audit:
            api.side_effect = [pr, pr, {"id": 123}]
            self.assertEqual(app.check_pr(589, "a" * 40, "synthetic-token"),
                             "FAILURE")
            audit.assert_not_called()
            self.assertEqual(api.call_args.args[3]["head_sha"], "a" * 40)
            self.assertEqual(api.call_args.args[3]["conclusion"], "failure")

    def test_tag_appears_between_snapshots_issues_failure(self):
        pr, cmp, base, head, old, new = fixtures()
        base["tree"], head["tree"] = {"sha": "old"}, {"sha": "new"}
        posts = []
        def api(method, path, token, payload=None):
            if method == "POST":
                posts.append(payload)
                return {"id": 123}
            if "/git/trees/" in path:
                return old if path.endswith("old?recursive=1") else new
            if "/git/commits/" + app.BASE_SHA in path:
                return base
            if "/git/commits/" + app.HEAD_SHA in path:
                return head
            if "/compare/" in path:
                return cmp
            return pr
        with patch.object(app, "gh", side_effect=api), patch.object(
                app, "has_release_tag", side_effect=[False, True]):
            self.assertEqual(app.check_pr(589, app.HEAD_SHA, "synthetic-token"),
                             "FAILURE")
        self.assertEqual(len(posts), 1)
        self.assertEqual(posts[0]["conclusion"], "failure")
        self.assertEqual(posts[0]["head_sha"], app.HEAD_SHA)

    def test_signed_tag_event_revokes_existing_success(self):
        payload = {"ref": "refs/tags/" + app.RELEASE_TAG, "deleted": False,
                   "repository": {"full_name": app.REPO},
                   "installation": {"id": 77}}
        self.assertTrue(app.decide_tag_event("push", payload, 77))
        self.assertFalse(app.decide_tag_event("pull_request", payload, 77))
        bad = copy.deepcopy(payload)
        bad["deleted"] = True
        self.assertFalse(app.decide_tag_event("push", bad, 77))
        with self.assertRaises(app.Denied):
            app.decide_tag_event("push", payload, 78)
        pr = fixtures()[0]
        posts = []
        def api(method, path, token, body=None):
            if method == "POST":
                posts.append(body)
                return {"id": 456}
            if "/pulls?" in path:
                return [pr]
            if "/pulls/589" in path:
                return pr
            raise AssertionError(path)
        with patch.object(app, "gh", side_effect=api):
            self.assertEqual(app.revoke_on_tag_push("synthetic-token"), 1)
        self.assertEqual(posts[0]["conclusion"], "failure")
        self.assertEqual(posts[0]["head_sha"], app.HEAD_SHA)

    def test_revocation_survives_tag_deletion_and_pr_edited(self):
        app.mark_revoked()
        self.assertTrue(app.is_revoked())
        self.assertTrue(list(Path(self._state.name).glob("*.revoked")))
        pr = fixtures()[0]
        posts = []
        def api(method, path, token, body=None):
            if method == "POST":
                posts.append(body)
                return {"id": 999}
            return pr
        # Tag is gone, but one-way durable revocation MUST forbid success.
        with patch.object(app, "gh", side_effect=api), patch.object(
                app, "read_live") as live, patch.object(
                app, "has_release_tag", return_value=False):
            self.assertEqual(app.check_pr(589, app.HEAD_SHA, "synthetic-token"),
                             "FAILURE")
            live.assert_not_called()
        self.assertEqual(len(posts), 1)
        self.assertEqual(posts[0]["conclusion"], "failure")
        self.assertTrue(app.is_revoked())

    def test_revocation_retries_after_api_outage(self):
        app.mark_revoked()
        self.assertFalse(app.is_revocation_acked())
        with patch.object(app, "create_installation_token", return_value="test-token"), patch.object(
                app, "revoke_on_tag_push", side_effect=[OSError("synthetic REST 503"), 1]) as call:
            with self.assertRaises(OSError):
                app.attempt_revocation_once()
            self.assertTrue(app.is_revoked())  # Event survives failure.
            self.assertFalse(app.is_revocation_acked())
            self.assertEqual(app.attempt_revocation_once(), 1)
            self.assertTrue(app.is_revocation_acked())
            self.assertEqual(call.call_count, 2)

    def test_missing_durable_state_fails_closed(self):
        with patch.dict(os.environ, {"APP_STATE_DIR": ""}):
            with self.assertRaises(app.Denied):
                app.is_revoked()

    def test_denied_tree_produces_failure_not_pass(self):
        pr, cmp, base, head, old, new = fixtures()
        cmp["files"][0]["sha"] = "tampered"
        base["tree"] = {"sha": "old"}
        head["tree"] = {"sha": "new"}
        with patch.object(app, "gh") as api, patch.object(app, "has_release_tag", return_value=False):
            api.side_effect = [
                pr,  # initial
                pr, base, head, cmp, old, new, # live facts
                pr,  # current PR
                {"id": 123},  # App CHECK RUN created with FAILURE
            ]
            result = app.check_pr(589, app.HEAD_SHA, "synthetic-token")
            # sequence 8 GET then one POST
            self.assertEqual(result, "FAILURE")
            self.assertEqual(api.call_args.args[0], "POST")
            self.assertEqual(api.call_args.args[3]["conclusion"], "failure")


if __name__ == "__main__":
    unittest.main()
