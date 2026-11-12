#!/usr/bin/env python3
"""Read-only, fail-closed affected-case proposal. Never executes tests or overrides CI."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
REGISTRY = ROOT / "tests" / "case_selection_rules.json"
GATES = ("G0", "G1", "G2", "G3", "G4", "G5", "G6")
SHA = re.compile(r"[0-9a-fA-F]{40}\Z")


def load_rules(path: Path = REGISTRY) -> dict:
    obj = json.loads(path.read_text(encoding="utf-8"))
    if obj.get("version") != 1 or not isinstance(obj.get("rules"), list):
        raise ValueError("Unsupported or incomplete test selection registry")
    ids = set()
    catalog = (ROOT / "tests" / "TEST_CATALOG.md").read_text(encoding="utf-8")
    catalog_ids = set(re.findall(r"^\| `(hyremote-[^ `]+)` \|", catalog, re.MULTILINE))
    for rule in obj["rules"] + obj.get("risk_rules", []):
        if not rule.get("id") or rule["id"] in ids:
            raise ValueError("Duplicate or missing selection rule id")
        ids.add(rule["id"])
        if not rule.get("prefixes") or not rule.get("cases"):
            raise ValueError("Rule lacks scope or cases: " + rule["id"])
        if any(c not in catalog_ids for c in rule["cases"]):
            raise ValueError("Non-catalog case name in rule: " + rule["id"])
    return obj


def git(*args: str) -> bytes:
    result = subprocess.run(
        ["git", "-C", str(ROOT), *args], capture_output=True, check=True, timeout=30,
    )
    return result.stdout


def commit_sha(value: str) -> str:
    if not SHA.fullmatch(value):
        raise ValueError("Expected full 40-character commit SHA")
    return git("rev-parse", "--verify", value + "^{commit}").decode("ascii").strip()


def discover_files(base: str, head: str) -> list[str]:
    # Disabling rename detection exposes both paths as deletion/addition.
    # NUL separation preserves literal spaces and non-ASCII Git filenames.
    data = git("diff", "--no-renames", "--name-only", "-z", base, head, "--")
    return [name.decode("utf-8") for name in data.split(b"\0") if name]


def valid_path(path: str) -> bool:
    return bool(path) and not re.search(r"[:\\\x00-\x1f]", path) and all(
        part not in ("", ".", "..") for part in path.split("/")
    )


def matches(file: str, prefix: str) -> bool:
    return file.startswith(prefix) if prefix.endswith("/") else file == prefix


def select(registry: dict, files: list[str], gate: str, source_sha: str, issue: str,
           base_sha: str | None = None, risks: list[str] | None = None) -> dict:
    if gate not in GATES:
        raise ValueError("Unknown G gate")
    if not SHA.fullmatch(source_sha):
        raise ValueError("Expected full 40-character source SHA")
    risk_rules = {rule["id"]: rule for rule in registry.get("risk_rules", [])}
    risks = sorted(set(risks or []))
    unknown_risks = sorted(set(risks) - set(risk_rules))
    if unknown_risks:
        raise ValueError("Unknown risk tags: " + ", ".join(unknown_risks))
    matched: dict[str, dict] = {}
    unclassified: list[str] = []
    for file in sorted(set(files)):
        if not valid_path(file):
            raise ValueError("Invalid changed file path")
        candidates = [
            (len(prefix), rule)
            for rule in registry["rules"]
            for prefix in rule["prefixes"]
            if matches(file, prefix)
        ]
        # A specific Quick/RFB prefix overrides the generic Runtime family;
        # independent rules with equally specific paths still combine.
        specificity = max((length for length, _ in candidates), default=0)
        rules = [rule for length, rule in candidates if length == specificity]
        if not rules:
            unclassified.append(file)
        else:
            for rule in rules:
                matched[rule["id"]] = rule
    # Cross-cutting risk contracts are additive; path specificity applies only
    # to path rules. A Quick source change with security implications must
    # still select the security-on handshake test, not silently suppress it.
    for risk in risks:
        matched["risk:" + risk] = risk_rules[risk]
    cases = sorted({c for rule in matched.values() for c in rule["cases"]})
    # Plan identity excludes ephemeral paths, timestamps, cache state and runner identity.
    identity = {
        "registry_version": registry["version"], "source_sha": source_sha.lower(),
        "base_sha": base_sha, "change_source": "GIT_DIFF" if base_sha else "DECLARED_PATHS",
        "registry_hash": hashlib.sha256(json.dumps(
            registry, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest(),
        "issue": issue, "gate": gate, "changed_files": sorted(set(files)),
        "risk_tags": risks, "rule_ids": sorted(matched), "case_ids": cases,
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
    assert "hyremote-runtime-diagnostics-test" not in quick["case_ids"]
    assert quick["status"] == "ADVISORY" and not quick["execution_allowed"]
    auth = select(registry, quick["changed_files"], "G1", sha, "967",
                  risks=["security-auth"])
    assert "hyremote-rfb-vnc-auth-handshake-test" in auth["case_ids"]
    assert "hyremote-quick-capture-test" in auth["case_ids"]
    assert auth["plan_hash"] != quick["plan_hash"]
    assert select(registry, quick["changed_files"], "G1", sha, "967",
                  risks=["public-sdk-abi", "security-auth"])["plan_hash"] == select(
                      registry, quick["changed_files"], "G1", sha, "967",
                      risks=["security-auth", "public-sdk-abi"])["plan_hash"]
    assert "hyremote-qml-installed-consumer" in select(
        registry, ["src/integrations/qml/QmlRemoteAccess.cpp"], "G1", sha, "970",
        risks=["qml-registration"])["case_ids"]
    try:
        select(registry, quick["changed_files"], "G1", sha, "970",
               risks=["not-a-known-risk"])
    except ValueError:
        pass
    else:
        raise AssertionError("unregistered risk accepted")
    assert quick["plan_hash"] == select(registry, quick["changed_files"], "G1", sha, "967")["plan_hash"]
    docs = select(registry, ["docs/internal/evidence/sample.md"], "G1", sha, "970")
    assert "hyremote-runtime-dll-lifecycle-test" not in docs["case_ids"]
    unknown = select(registry, ["src/unknown_module/new_feature.cpp"], "G1", sha, "970")
    assert unknown["status"] == "BLOCKED_UNMAPPED"
    assert select(registry, ["src/core/src/session.cpp"], "G1", sha, "970")["status"] == "ADVISORY"
    rfb = select(registry, ["src/runtime/src/transport/rfb_transport.cpp"], "G1", sha, "970")
    assert "hyremote-rfb-wire-robustness-test" in rfb["case_ids"]
    assert select(registry, ["SRC/core/src/session.cpp"], "G1", sha, "970")["status"] == "BLOCKED_UNMAPPED"
    assert select(registry, ["build.cmd.backup"], "G1", sha, "970")["status"] == "BLOCKED_UNMAPPED"
    assert not valid_path("src/../outside") and not valid_path("C:/src/file")
    try:
        select(registry, ["src/core/src/session.cpp"], "G1", "a", "970")
    except ValueError:
        pass
    else:
        raise AssertionError("short SHA accepted")
    print("TEST_PLAN_SELF_TEST=PASS")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", help="Full PR base commit SHA for git diff")
    parser.add_argument("--head", help="Full PR head commit SHA for git diff")
    parser.add_argument("--file", action="append", default=[], help="Explicit changed path; repeatable")
    parser.add_argument("--sha", help="Exact 40-character test source SHA")
    parser.add_argument("--issue", default="unassigned")
    parser.add_argument("--gate", choices=GATES, default="G1")
    parser.add_argument("--risk", action="append", default=[],
                        help="Declared cross-cutting risk from registry; repeatable")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        registry = load_rules()
        if args.self_test:
            self_test(registry)
            return 0
        if bool(args.base) != bool(args.head):
            parser.error("--base and --head must appear together")
        if not args.sha:
            parser.error("--sha must be a full immutable source SHA")
        source = commit_sha(args.sha)
        base = None
        if args.base:
            if args.file:
                parser.error("--file and Git diff inputs are mutually exclusive")
            base, head = commit_sha(args.base), commit_sha(args.head)
            if head != source:
                raise ValueError("--sha must identify the same commit as --head")
            files = discover_files(base, head)
        else:
            files = args.file
        if not files:
            parser.error("No changed files supplied or discovered; zero-case plan is forbidden")
        result = select(registry, files, args.gate, source, args.issue, base,
                        risks=args.risk)
        print(json.dumps(result, ensure_ascii=False, sort_keys=True, indent=2))
        return 2 if result["status"] == "BLOCKED_UNMAPPED" else 0
    except (ValueError, OSError, subprocess.SubprocessError) as exc:
        print("TEST_PLAN_ERROR: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
