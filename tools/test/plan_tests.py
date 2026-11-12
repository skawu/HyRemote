#!/usr/bin/env python3
"""Read-only, fail-closed affected-case proposal. Never executes tests or overrides CI."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
REGISTRY = ROOT / "tests" / "case_selection_rules.json"
GATES = ("G0", "G1", "G2", "G3", "G4", "G5", "G6")


def load_rules(path: Path = REGISTRY) -> dict:
    obj = json.loads(path.read_text(encoding="utf-8"))
    if obj.get("version") != 1 or not isinstance(obj.get("rules"), list):
        raise ValueError("Unsupported or incomplete test selection registry")
    ids = set()
    for rule in obj["rules"]:
        if not rule.get("id") or rule["id"] in ids:
            raise ValueError("Duplicate or missing selection rule id")
        ids.add(rule["id"])
        if not rule.get("prefixes") or not rule.get("cases"):
            raise ValueError("Rule lacks scope or cases: " + rule["id"])
        if any(not c.startswith("hyremote-") for c in rule["cases"]):
            raise ValueError("Non-catalog case name in rule: " + rule["id"])
    return obj


def discover_files(base: str, head: str) -> list[str]:
    for ref in (base, head):
        if not ref or ref.startswith("-"):
            raise ValueError("Base/head must be explicit Git refs")
    result = subprocess.run(
        ["git", "-C", str(ROOT), "diff", "--name-only", "--diff-filter=ACMRT", base, head, "--"],
        capture_output=True, text=True, check=True, timeout=30,
    )
    return sorted(set(result.stdout.splitlines()))


def select(registry: dict, files: list[str], gate: str, source_sha: str, issue: str) -> dict:
    if gate not in GATES:
        raise ValueError("Unknown G gate")
    if len(source_sha) != 40 or any(c not in "0123456789abcdef" for c in source_sha.lower()):
        raise ValueError("Expected full 40-character source SHA")
    matched: dict[str, dict] = {}
    unclassified: list[str] = []
    for file in sorted(set(files)):
        if not file or file.startswith("/") or ".." in Path(file).parts:
            raise ValueError("Invalid changed file path")
        rules = [
            rule for rule in registry["rules"]
            if any(file.startswith(prefix) for prefix in rule["prefixes"])
        ]
        if not rules:
            unclassified.append(file)
        else:
            for rule in rules:
                matched[rule["id"]] = rule
    cases = sorted({c for rule in matched.values() for c in rule["cases"]})
    # Plan identity excludes ephemeral paths, timestamps, cache state and runner identity.
    identity = {
        "registry_version": registry["version"], "source_sha": source_sha.lower(),
        "issue": issue, "gate": gate, "changed_files": sorted(set(files)),
        "rule_ids": sorted(matched), "case_ids": cases,
    }
    key = hashlib.sha256(
        json.dumps(identity, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    return {
        **identity, "plan_hash": key, "status": "BLOCKED_UNMAPPED" if unclassified else "ADVISORY",
        "unclassified_files": unclassified,
        "reasons": {r["id"]: r["reason"] for r in matched.values()},
        "capability_note": "Registration/Qt kit guards must be validated at execution; missing cases are never PASS.",
        "authority": "Existing CI classifier, G0-G6 execution manual and branch protection remain binding.",
        "execution_allowed": False,
        "qualification": "NOT_ACCEPTANCE",
    }


def self_test(registry: dict) -> None:
    sha = "a" * 40
    quick = select(registry, ["src/runtime/src/quick/quick_gl_pbo_capture_source.cpp"], "G1", sha, "967")
    assert "hyremote-runtime-dll-lifecycle-test" in quick["case_ids"]
    assert "hyremote-quick-capture-test" in quick["case_ids"]
    assert quick["status"] == "ADVISORY" and not quick["execution_allowed"]
    assert quick["plan_hash"] == select(registry, quick["changed_files"], "G1", sha, "967")["plan_hash"]
    docs = select(registry, ["docs/internal/evidence/sample.md"], "G1", sha, "970")
    assert "hyremote-runtime-dll-lifecycle-test" not in docs["case_ids"]
    unknown = select(registry, ["src/unknown_module/new_feature.cpp"], "G1", sha, "970")
    assert unknown["status"] == "BLOCKED_UNMAPPED"
    assert select(registry, ["src/core/src/session.cpp"], "G1", sha, "970")["status"] == "ADVISORY"
    try:
        select(registry, ["src/core/src/session.cpp"], "G1", "a", "970")
    except ValueError:
        pass
    else:
        raise AssertionError("short SHA accepted")
    print("TEST_PLAN_SELF_TEST=PASS")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", help="Exact PR base/ref for git diff")
    parser.add_argument("--head", help="Exact PR head/ref for git diff")
    parser.add_argument("--file", action="append", default=[], help="Explicit changed path; repeatable")
    parser.add_argument("--sha", help="Exact 40-character test source SHA")
    parser.add_argument("--issue", default="unassigned")
    parser.add_argument("--gate", choices=GATES, default="G1")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        registry = load_rules()
        if args.self_test:
            self_test(registry)
            return 0
        if bool(args.base) != bool(args.head):
            parser.error("--base and --head must appear together")
        files = args.file + (discover_files(args.base, args.head) if args.base else [])
        if not files:
            parser.error("No changed files supplied or discovered; zero-case plan is forbidden")
        if not args.sha:
            parser.error("--sha must be a full immutable source SHA")
        result = select(registry, files, args.gate, args.sha, args.issue)
        print(json.dumps(result, ensure_ascii=False, sort_keys=True, indent=2))
        return 2 if result["status"] == "BLOCKED_UNMAPPED" else 0
    except (ValueError, OSError, subprocess.SubprocessError) as exc:
        print("TEST_PLAN_ERROR: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
