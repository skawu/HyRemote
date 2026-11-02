#!/usr/bin/env python3
"""Opt-in HyRemote Local Owner inbox. Only reviewed orders on develop are executable.

No Issue/PR comment, task field or remote file is treated as shell commands.
The operator must explicitly install/run this worker on the Windows test host.
"""
from __future__ import annotations

import argparse
import base64
from contextlib import contextmanager
import datetime as dt
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

REPO = "skawu/HyRemote"
ORDERS = ".github/local-owner/orders"
ID = re.compile(r"^[a-z0-9][a-z0-9-]{5,79}$")
SHA = re.compile(r"^[0-9a-f]{40}$")
REF = re.compile(r"^release/v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$")
TASKS = {"hosted-l2", "canonical-g6", "installed-generic-qpa-unicode"}
FIELDS = {"schema", "id", "repository", "issue", "ref", "sha", "task",
          "kit", "expires_utc", "state", "requires_physical_viewer"}
METRICS = ("DISCOVERED_TEST_COUNT", "EXECUTED_TEST_COUNT")
UTC = dt.timezone.utc


class Blocked(Exception):
    """A safe, explicit stop. Never downgrade to success."""


class TestFailed(Exception):
    """The qualified test ran and produced contrary evidence (not missing permissions)."""


def validate(order: dict, *, now: dt.datetime | None = None) -> dict:
    if not isinstance(order, dict) or set(order) != FIELDS:
        raise Blocked("invalid work-order schema/field set")
    if type(order["schema"]) is not int or order["schema"] != 1:
        raise Blocked("unsupported schema")
    if not isinstance(order["id"], str) or not ID.fullmatch(order["id"]):
        raise Blocked("invalid task id")
    if order["repository"] != REPO or type(order["issue"]) is not int or order["issue"] != 95:
        raise Blocked("untrusted repository/issue")
    if not isinstance(order["ref"], str) or not REF.fullmatch(order["ref"]):
        raise Blocked("untrusted release ref")
    if not isinstance(order["sha"], str) or not SHA.fullmatch(order["sha"]):
        raise Blocked("invalid exact SHA")
    if not isinstance(order["task"], str) or order["task"] not in TASKS or order["kit"] != "win-qt683-msvc2022":
        raise Blocked("unapproved task/kit")
    if order["state"] != "QUEUED" or type(order["requires_physical_viewer"]) is not bool:
        raise Blocked("invalid order state/viewer flag")
    if order["requires_physical_viewer"] != (order["task"] == "installed-generic-qpa-unicode"):
        raise Blocked("incorrect physical evidence boundary")
    if not isinstance(order["expires_utc"], str):
        raise Blocked("invalid expiry")
    try:
        expires = dt.datetime.fromisoformat(order["expires_utc"].replace("Z", "+00:00"))
    except ValueError as exc:
        raise Blocked("invalid expiry") from exc
    if expires.utcoffset() is None or expires <= (now or dt.datetime.now(UTC)):
        raise Blocked("expired order")
    return order


def run(argv: list[str], *, cwd: Path | None = None, output: Path | None = None,
        env: dict | None = None) -> subprocess.CompletedProcess:
    if output is None:
        return subprocess.run(argv, cwd=cwd, env=env, text=True,
                              encoding="utf-8", errors="replace",
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    with output.open("w", encoding="utf-8") as handle:
        return subprocess.run(argv, cwd=cwd, env=env, stdout=handle,
                              stderr=subprocess.STDOUT, check=False)


def gh_json(endpoint: str) -> object:
    result = run(["gh", "api", endpoint, "--header", "Accept: application/vnd.github+json"])
    if result.returncode:
        raise Blocked("GitHub API read unavailable; check gh auth/status")
    try:
        return json.loads(result.stdout)
    except ValueError as exc:
        raise Blocked("invalid GitHub API response") from exc


def github_orders() -> list[dict]:
    listing = gh_json(f"repos/{REPO}/contents/{ORDERS}?ref=develop")
    if not isinstance(listing, list):
        raise Blocked("order directory unavailable")
    orders = []
    for item in sorted(listing, key=lambda x: x.get("name", "")):
        name = item.get("name", "")
        if not re.fullmatch(r"[a-z0-9-]+\.json", name):
            raise Blocked("unexpected order file")
        record = gh_json(f"repos/{REPO}/contents/{ORDERS}/{name}?ref=develop")
        if not isinstance(record, dict) or record.get("encoding") != "base64":
            raise Blocked("order is not a GitHub content blob")
        try:
            raw = base64.b64decode(record["content"], validate=False)
            parsed = json.loads(raw.decode("utf-8"))
            order = validate(parsed)
        except Blocked as exc:
            if str(exc) == "expired order":
                # Old immutable orders stay as audit history, but must never
                # prevent a newer validated order from being received.
                print("EXPIRED_SKIPPED", name)
                continue
            raise
        except (ValueError, KeyError, UnicodeError) as exc:
            raise Blocked("malformed reviewed work order") from exc
        if name != order["id"] + ".json":
            raise Blocked("order id/path mismatch")
        orders.append(order)
    return orders


def branch_head(ref: str) -> str:
    branch = gh_json(f"repos/{REPO}/branches/{ref}")
    try:
        sha = branch["commit"]["sha"]
    except (TypeError, KeyError) as exc:
        raise Blocked("release SHA unavailable") from exc
    if not SHA.fullmatch(str(sha)):
        raise Blocked("invalid GitHub release SHA")
    return sha


def comment(issue: int, payload: dict) -> None:
    # Only controlled metadata crosses GitHub. Never post test stdout or watermark text.
    body = "LOCAL_OWNER_WORKER " + json.dumps(payload, ensure_ascii=True, sort_keys=True)
    p = run(["gh", "issue", "comment", str(issue), "--repo", REPO, "--body", body])
    if p.returncode:
        raise Blocked("GitHub status ACK/comment could not be delivered")


def ledger_read(path: Path) -> dict:
    if not path.exists():
        return {}
    try:
        obj = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise Blocked("local ledger unreadable; never replay an ambiguous task") from exc
    if not isinstance(obj, dict):
        raise Blocked("local ledger invalid")
    return obj


def ledger_write(path: Path, ledger: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(prefix=".owner-ledger-", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as handle:
            json.dump(ledger, handle, sort_keys=True, indent=2)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def verify_workspace(workspace: Path, sha: str) -> None:
    head = run(["git", "rev-parse", "HEAD"], cwd=workspace)
    dirty = run(["git", "status", "--porcelain", "--untracked-files=normal"], cwd=workspace)
    if head.returncode or head.stdout.strip() != sha or dirty.returncode or dirty.stdout.strip():
        raise Blocked("local worktree must already be clean at the exact candidate SHA")


def metric(log: str, key: str) -> int:
    found = re.findall(r"(?m)(?:^|\s)" + re.escape(key) + r"=(\d+)", log)
    return int(found[-1]) if found else 0


def hosted_l2(order: dict) -> dict:
    # Check for an already submitted exact-SHA dispatch to avoid unnecessary duplicates.
    runs = gh_json(f"repos/{REPO}/actions/runs?event=workflow_dispatch&per_page=100")
    if not isinstance(runs, dict):
        raise Blocked("cannot inspect earlier dispatched runs")
    for item in runs.get("workflow_runs", []):
        if (item.get("name") == "CI" and item.get("head_sha") == order["sha"]
                and item.get("head_branch") == order["ref"]):
            return {"status": "DISPATCHED", "run_id": item["id"], "reused": True}
    p = run(["gh", "workflow", "run", "ci.yml", "--repo", REPO,
             "--ref", order["ref"], "-f", "purpose=release-readiness",
             "-f", "expected_sha=" + order["sha"], "-f", "legacy_dual_platform=false"])
    if p.returncode:
        raise Blocked("workflow dispatch denied")
    # A successful dispatch is NOT proof of the remote L2 outcome.
    return {"status": "DISPATCHED", "run_id": None, "reused": False}


def canonical_g6(order: dict, workspace: Path, kit_config: Path | None, state: Path) -> dict:
    verify_workspace(workspace, order["sha"])
    if os.name != "nt":
        raise Blocked("canonical Windows G6 requires Windows")
    if kit_config is None or not kit_config.is_file():
        raise Blocked("local Kit profile is missing; it must be selected on this machine")
    if not (workspace / "build.cmd").is_file():
        raise Blocked("canonical build.cmd not found")
    log = state / (order["id"] + ".g6.log")
    args = ["cmd.exe", "/d", "/c", str(workspace / "build.cmd"), "test",
            "--config=" + str(kit_config)]
    proc = run(args, cwd=workspace, output=log)
    text = log.read_text(encoding="utf-8", errors="replace")
    discovered = metric(text, "DISCOVERED_TEST_COUNT")
    executed = metric(text, "EXECUTED_TEST_COUNT")
    if discovered == 0 or executed == 0:
        raise Blocked("canonical G6 did not report positive discovered/executed counts")
    if proc.returncode or executed != discovered:
        raise TestFailed("canonical G6 returned failure or incomplete execution; inspect local log")
    return {"status": "MACHINE_PASS", "discovered": discovered,
            "executed": executed, "log": log.name}


def watermark_cases() -> list[tuple[str, str]]:
    # ASCII-only source code: use Windows native wide arguments without passing GBK script text.
    cjk = "".join(chr(v) for v in (0x6C34, 0x5370, 0x5019, 0x9009))
    spaced = "".join(chr(v) for v in (0x6301, 0x4E45)) + " " + "".join(
        chr(v) for v in (0x5019, 0x9009))
    return [("ascii", "W1-MARK"), ("cjk", cjk), ("quoted-space", spaced)]


def unicode_probe(order: dict, workspace: Path, prefix_bin: Path | None,
                  state: Path) -> dict:
    verify_workspace(workspace, order["sha"])
    if os.name != "nt":
        raise Blocked("installed Unicode probe requires Windows")
    if prefix_bin is None or not prefix_bin.is_dir():
        raise Blocked("installed RC2 prefix bin not selected")
    plugin_dir = prefix_bin.parent / "plugins"
    if not plugin_dir.is_dir():
        raise Blocked("deployed plugin directory missing")
    names = (("generic", "hyremote-learning-03-widgets.exe", "-plugin",
              "hyremote:port=5921;watermark="),
             ("qpa", "hyremote-qpa-proxy-existing-app.exe", "-platform",
              "hyremote:hyremote-watermark="))
    results = []
    for route, name, option, prefix in names:
        executable = prefix_bin / name
        if not executable.is_file():
            raise Blocked("installed example missing for " + route)
        for label, expected in watermark_cases():
            selected = state / "probe" / order["id"] / route / label
            if selected.exists():
                raise Blocked("probe state already exists; no overwrite/reuse")
            selected.mkdir(parents=True)
            env = os.environ.copy()
            env["HYREMOTE_STATE_DIR"] = str(selected)
            env["QT_PLUGIN_PATH"] = str(plugin_dir)
            log = state / (order["id"] + "." + route + "-" + label + ".log")
            # No shell or Start-Process -ArgumentList: Python creates native Win32
            # argv, preserving spaces in the single -platform/-plugin value.
            result = run([str(executable), option, prefix + expected,
                          "--test-seconds", "5"], cwd=prefix_bin, env=env, output=log)
            policy = selected / "watermark.policy"
            try:
                body = policy.read_text(encoding="utf-8")
                text_fields = [line[5:] for line in body.splitlines()
                               if line.startswith("text=")]
            except (OSError, UnicodeError):
                text_fields = []
            correct = result.returncode == 0 and len(text_fields) == 1 and text_fields[0] == expected
            results.append({"route": route, "case": label, "exact_codepoints": correct,
                            "code_units": len(text_fields[0]) if text_fields else 0})
            if not correct:
                raise TestFailed("installed Unicode policy mismatch at " + route + "/" + label)
    # Machine policy equivalence is not an actual RealVNC Viewer visual judgement.
    return {"status": "PHYSICAL_REQUIRED", "cases": results,
            "viewer": "NOT_RUN", "note": "six installed cases passed policy codepoints only"}


def execute(order: dict, args: argparse.Namespace, state: Path) -> dict:
    if branch_head(order["ref"]) != order["sha"]:
        raise Blocked("release ref moved: exact SHA mismatch")
    if order["task"] == "hosted-l2":
        return hosted_l2(order)
    if order["task"] == "canonical-g6":
        return canonical_g6(order, args.workspace, args.kit_config, state)
    return unicode_probe(order, args.workspace, args.prefix_bin, state)


def process(order: dict, args: argparse.Namespace, ledger: dict, ledger_path: Path) -> None:
    task_id = order["id"]
    previous = ledger.get(task_id)
    if previous and previous.get("status") == "FINISHED_UNREPORTED":
        # A result posting failure never replays the completed operation.
        comment(order["issue"], previous["result"])
        previous["status"] = "RECORDED"
        ledger_write(ledger_path, ledger)
        return
    if previous and previous.get("status") not in {"RECEIVED"}:
        # STARTED or RECORDED may have performed non-idempotent operations.
        # Only an explicit reconciliation by the local operator may recover.
        return
    if not args.execute:
        if not previous:
            print("QUEUED_UNDELIVERED", task_id, order["sha"], order["task"])
        return
    if branch_head(order["ref"]) != order["sha"]:
        envelope = {"id": task_id, "sha": order["sha"], "task": order["task"],
                    "state": "BLOCKED",
                    "result": {"status": "BLOCKED", "reason": "release SHA mismatch"}}
        ledger[task_id] = {"status": "FINISHED_UNREPORTED", "result": envelope}
        ledger_write(ledger_path, ledger)
        comment(order["issue"], envelope)
        ledger[task_id]["status"] = "RECORDED"
        ledger_write(ledger_path, ledger)
        print("BLOCKED_SHA", task_id)
        return
    if not previous:
        ledger[task_id] = {"status": "RECEIVED", "sha": order["sha"],
                           "task": order["task"]}
        ledger_write(ledger_path, ledger)
    # A crash or GitHub outage at RECEIVED cannot have started the operation:
    # retry the ACK and proceed safely. At STARTED, execution is not replayed.
    comment(order["issue"], {"id": task_id, "sha": order["sha"], "state": "RECEIVED"})
    ledger[task_id]["status"] = "STARTED"
    ledger_write(ledger_path, ledger)
    comment(order["issue"], {"id": task_id, "sha": order["sha"], "state": "STARTED"})
    try:
        result = execute(order, args, ledger_path.parent)
    except TestFailed as exc:
        result = {"status": "FAIL", "reason": str(exc)}
    except (Blocked, OSError, subprocess.SubprocessError) as exc:
        result = {"status": "BLOCKED", "reason": str(exc)}
    # Even successful local evidence is stale if someone moved the release ref
    # while this potentially long-running operation was executing.
    try:
        if branch_head(order["ref"]) != order["sha"]:
            result = {"status": "BLOCKED", "reason": "release SHA moved during execution"}
    except Blocked:
        result = {"status": "BLOCKED", "reason": "cannot reverify release SHA"}
    envelope = {"id": task_id, "sha": order["sha"], "task": order["task"],
                "state": result["status"], "result": result}
    ledger[task_id] = {"status": "FINISHED_UNREPORTED", "result": envelope}
    ledger_write(ledger_path, ledger)
    comment(order["issue"], envelope)
    ledger[task_id]["status"] = "RECORDED"
    ledger_write(ledger_path, ledger)
    print(task_id, result["status"])


@contextmanager
def single_worker(state_dir: Path):
    """Never run concurrent polling cycles against the same local ledger."""
    lock = state_dir / "worker.lock"
    try:
        fd = os.open(lock, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError as exc:
        raise Blocked("another worker is active (or stale lock requires manual review)") from exc
    try:
        with os.fdopen(fd, "w", encoding="ascii") as handle:
            handle.write(str(os.getpid()))
        yield
    finally:
        lock.unlink()


def poll(args: argparse.Namespace) -> None:
    ledger_path = args.state_dir / "ledger.json"
    args.state_dir.mkdir(parents=True, exist_ok=True)
    with single_worker(args.state_dir):
        poll_locked(args, ledger_path)


def poll_locked(args: argparse.Namespace, ledger_path: Path) -> None:
    while True:
        try:
            records = github_orders()
            ledger = ledger_read(ledger_path)
            for order in records:
                process(order, args, ledger, ledger_path)
        except (Blocked, OSError, subprocess.SubprocessError) as exc:
            print("POLL_BLOCKED", str(exc), file=sys.stderr)
            if args.once:
                raise SystemExit(2) from exc
        if args.once:
            return
        time.sleep(args.poll_seconds)


def self_test() -> None:
    sample = {"schema": 1, "id": "rc2-hosted-l2-20261102", "repository": REPO,
              "issue": 95, "ref": "release/v0.3.5.0", "sha": "a" * 40,
              "task": "hosted-l2", "kit": "win-qt683-msvc2022",
              "expires_utc": "2099-01-01T00:00:00Z", "state": "QUEUED",
              "requires_physical_viewer": False}
    assert validate(sample)
    for override in ({"sha": "b" * 39}, {"task": "arbitrary-shell"},
                     {"kit": "some-other-kit"}, {"ref": "develop"},
                     {"repository": "attacker/fork"}, {"issue": 123},
                     {"expires_utc": "2020-01-01T00:00:00Z"},
                     {"requires_physical_viewer": True}):
        bad = dict(sample, **override)
        try:
            validate(bad)
        except Blocked:
            pass
        else:
            raise AssertionError("negative contract admitted " + str(override))
    bad = dict(sample, command="powershell -Command Invoke-Expression ...")
    try:
        validate(bad)
    except Blocked:
        pass
    else:
        raise AssertionError("arbitrary remote command field admitted")
    with tempfile.TemporaryDirectory() as temp:
        path = Path(temp) / "ledger.json"
        ledger_write(path, {sample["id"]: {"status": "STARTED"}})
        assert ledger_read(path)[sample["id"]]["status"] == "STARTED"
        assert [len(t) for _, t in watermark_cases()] == [7, 4, 5]
        assert tuple(ord(x) for x in watermark_cases()[1][1]) == (
            0x6C34, 0x5370, 0x5019, 0x9009)
        assert metric("DISCOVERED_TEST_COUNT=79\nEXECUTED_TEST_COUNT=79",
                      "EXECUTED_TEST_COUNT") == 79
    # Simulated end-to-end receipt/ACK/status and replay check; no network,
    # shell or GitHub credentials are used by the self-test.
    from types import SimpleNamespace
    from unittest.mock import patch
    with tempfile.TemporaryDirectory() as temp:
        path = Path(temp) / "ledger.json"
        local = {}
        reports = []
        command = SimpleNamespace(execute=True, workspace=Path(temp),
                                  kit_config=None, prefix_bin=None)
        with patch.dict(globals(), {
            "branch_head": lambda ref: sample["sha"],
            "comment": lambda issue, msg: reports.append((issue, msg)),
            "execute": lambda order, args, state: {"status": "DISPATCHED",
                                                   "run_id": 42},
        }):
            process(sample, command, local, path)
            assert [msg["state"] for _, msg in reports] == [
                "RECEIVED", "STARTED", "DISPATCHED"]
            assert ledger_read(path)[sample["id"]]["status"] == "RECORDED"
            process(sample, command, local, path)
            assert len(reports) == 3, "duplicate delivery replayed a task"
        # Wrong release ref is observable BLOCKED, not an ACK or a run.
        blocked = {}
        reports.clear()
        with patch.dict(globals(), {
            "branch_head": lambda ref: "b" * 40,
            "comment": lambda issue, msg: reports.append((issue, msg)),
            "execute": lambda *a, **k: (_ for _ in ()).throw(
                AssertionError("moved ref must not execute")),
        }):
            process(sample, command, blocked, path)
            assert len(reports) == 1 and reports[0][1]["state"] == "BLOCKED"
            assert ledger_read(path)[sample["id"]]["status"] == "RECORDED"
        reports.clear()
        # A failed initial ACK may leave a durable RECEIVED record; it is
        # safe to resume because the action has not yet started.
        previous_acks = len(reports)
        saved = {sample["id"]: {"status": "RECEIVED", "sha": sample["sha"],
                                "task": sample["task"]}}
        ledger_write(path, saved)
        with patch.dict(globals(), {
            "branch_head": lambda ref: sample["sha"],
            "comment": lambda issue, msg: reports.append((issue, msg)),
            "execute": lambda order, args, state: {"status": "DISPATCHED",
                                                   "run_id": 43},
        }):
            process(sample, command, saved, path)
            assert len(reports) == previous_acks + 3
            assert ledger_read(path)[sample["id"]]["status"] == "RECORDED"
        # Simulate an already-finished physical operation whose first network
        # report failed; only its recorded RESULT is sent on retry.
        saved = ledger_read(path)
        saved[sample["id"]] = {"status": "FINISHED_UNREPORTED",
                               "result": {"state": "MACHINE_PASS", "id": sample["id"]}}
        ledger_write(path, saved)
        reports.clear()
        with patch.dict(globals(), {
            "comment": lambda issue, msg: reports.append((issue, msg)),
            "execute": lambda *a, **k: (_ for _ in ()).throw(
                AssertionError("a finished task must not rerun")),
        }):
            process(sample, command, saved, path)
            assert len(reports) == 1 and reports[0][1]["state"] == "MACHINE_PASS"
            assert ledger_read(path)[sample["id"]]["status"] == "RECORDED"
    print("local-owner worker self-test: PASS (schema, Unicode, ACK, replay, outbox)")


def self_test_orders() -> None:
    root = Path(__file__).resolve().parents[2]
    files = sorted((root / ORDERS).glob("*.json"))
    if not files:
        raise Blocked("no reviewed local work orders in checkout")
    fixed = dt.datetime(2026, 11, 2, tzinfo=UTC)
    for path in files:
        order = validate(json.loads(path.read_text(encoding="utf-8")), now=fixed)
        if order["id"] + ".json" != path.name:
            raise Blocked("committed task filename does not match id")
    print("local-owner committed orders: PASS", len(files))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--self-test-orders", action="store_true")
    parser.add_argument("--execute", action="store_true",
                        help="Explicit operator opt-in; otherwise discovery only")
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--poll-seconds", type=int, default=120)
    parser.add_argument("--workspace", type=Path, default=Path.cwd())
    parser.add_argument("--kit-config", type=Path, default=None)
    parser.add_argument("--prefix-bin", type=Path, default=None)
    parser.add_argument("--state-dir", type=Path,
                        default=Path.home() / ".hyremote-local-owner")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if args.self_test_orders:
        self_test_orders()
        return
    if args.poll_seconds < 30:
        parser.error("poll interval must be >=30 seconds")
    args.workspace = args.workspace.resolve()
    args.state_dir = args.state_dir.resolve()
    if args.kit_config is not None:
        args.kit_config = args.kit_config.resolve()
    if args.prefix_bin is not None:
        args.prefix_bin = args.prefix_bin.resolve()
    poll(args)


if __name__ == "__main__":
    main()
