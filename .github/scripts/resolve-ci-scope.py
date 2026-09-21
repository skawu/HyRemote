#!/usr/bin/env python3
"""Resolve the CI scope for one event.

The workflow calls this script instead of carrying the classifier inline, so the rules that decide which
capabilities to build and which tests to exclude are expressed once and can be executed by a self test. A
classifier that only exists inside a workflow cannot be tested, and an untested classifier is how a lane that claims
to be "full integration" can quietly exclude the release-readiness gates forever.

Contract:
  * ``--event``, ``--base``, ``--head`` describe the run.
  * Every derived value is written to ``$GITHUB_OUTPUT`` when it is set, and printed otherwise.
  * ``--self-test`` proves the required cases and exits non-zero on the first contradiction.

Scope semantics:
  * ``develop`` push and ``workflow_dispatch`` are the full integration lane: every capability, every piece of
    deploy evidence, and the release-readiness gates.
  * A pull request is the fast lane (#244): it builds the affected capabilities, moves expensive clean-SDK/deploy
    evidence to the PRs that can affect those contracts, and keeps the release-readiness gates out - unless the
    change touches the readiness authority itself, in which case readiness must run, because otherwise a stale
    readiness gate can reach ``develop`` with nothing ever executing it.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys

PEERS = ["cpp", "qml", "generic", "qpa"]

COMMON_FILES = {
    "CMakeLists.txt", "compile.cmd", "build.yml", ".github/workflows/ci.yml",
}

COMMON_PREFIXES = (
    "src/core/", "src/runtime/", "cmake/", "tests/build-authority/",
    "tests/release-readiness/", ".github/scripts/",
)

FRONTEND_PREFIXES = {
    "cpp": ("src/integrations/cpp/", "tests/consumer-installed-sdk/"),
    "qml": ("src/integrations/qml/", "tests/consumer-installed-qml/"),
    "generic": ("src/integrations/generic/", "tests/consumer-installed-generic/"),
    "qpa": ("src/integrations/qpa/", "tests/consumer-installed-qpa/"),
}

BUILT_EXAMPLE_PREFIXES = ("examples/", "tests/product-e2e/", "tests/public-api-contract/", "logo/")

COMMON_DEPLOY_PATHS = (
    "cmake/HyRemoteDeploy.cmake",
    "cmake/HyRemoteInstall.cmake",
    "tests/release-readiness/run_release_evidence.cmake",
)

# Surfaces that carry release/product truth the readiness gates assert. A change here has to execute readiness, or
# the gate that reads it can go stale unnoticed. This is deliberately narrower than all of docs/: an unrelated
# guide must not drag the expensive lane along.
READINESS_PREFIXES = (
    "tests/release-readiness/",
    ".github/release/",
    ".github/workflows/git-flow-policy.yml",
    "cmake/HyRemoteInstall.cmake",
    "cmake/HyRemoteDeploy.cmake",
    "cmake/HyRemoteConfig.cmake.in",
    "README.md",
    "SECURITY.md",
    "NOTICE.md",
    "CONTRIBUTING.md",
    "docs/release-package-manifest.md",
    "docs/input-model.md",
    "docs/known-limitations.md",
    "docs/compatibility.md",
    "docs/v1-api-stability.md",
    "docs/security.md",
    "docs/security-model.md",
    "docs/versioning.md",
    "docs/dependency-policy.md",
    "docs/releases/",
    "docs/internal/repository-layout.md",
    "docs/internal/release-candidate-checklist.md",
    "docs/internal/git-flow-release.md",
    "docs/internal/v1-ga-acceptance.md",
    "docs/internal/v1-physical-acceptance.md",
    "docs/internal/development-roadmap.md",
)

READINESS_TEST_PREFIX = "hyremote-release-readiness-"

# A name no exclusion is ever allowed to match. Any expression that matches it excludes every test, which would make
# the lane claim integration while executing nothing.
DANGEROUS_REGEX_PROBE = "hyremote-probe-name-that-no-exclusion-may-match"


def exclusion_is_total(test_exclude: str) -> bool:
    """True when the exclusion expression would exclude every test, or cannot be compiled at all."""
    if not test_exclude:
        return False
    try:
        pattern = re.compile(test_exclude)
    except re.error:
        return True
    return pattern.search(DANGEROUS_REGEX_PROBE) is not None


def changed_paths(event: str, base: str, head: str) -> list[str]:
    if event == "pull_request":
        return subprocess.check_output(
            ["git", "diff", "--name-only", f"{base}...{head}"], text=True
        ).splitlines()
    # develop/manual is the full integration lane, independent of the triggering diff.
    return ["CMakeLists.txt"]


def resolve(event: str, changed: list[str]) -> dict[str, str]:
    full_evidence = event != "pull_request"
    selected: set[str] = set()

    for path in changed:
        if path in COMMON_FILES or path.startswith(COMMON_PREFIXES):
            selected.update(PEERS)
            continue
        matched = False
        for peer, prefixes in FRONTEND_PREFIXES.items():
            if path.startswith(prefixes):
                selected.add(peer)
                matched = True
        if matched:
            continue
        if path.startswith(BUILT_EXAMPLE_PREFIXES):
            selected.update(PEERS)
        elif path.startswith("tests/"):
            selected.update(PEERS)

    def evidence_for(prefixes: tuple[str, ...]) -> bool:
        return full_evidence or any(
            path.startswith(COMMON_DEPLOY_PATHS + prefixes) for path in changed
        )

    generic_evidence = evidence_for(("src/integrations/generic/", "tests/consumer-installed-generic/"))
    qml_evidence = evidence_for(("src/integrations/qml/", "tests/consumer-installed-qml/"))
    qpa_evidence = evidence_for(("src/integrations/qpa/", "tests/consumer-installed-qpa/"))
    cpp_evidence = evidence_for((
        "src/integrations/cpp/", "tests/consumer-installed-sdk/", "tests/consumer-installed-cpp/",
    ))

    # The full lane always runs readiness. The fast lane runs it only when the change can invalidate it.
    readiness_evidence = full_evidence or any(
        path.startswith(READINESS_PREFIXES) for path in changed
    )

    ordered = [peer for peer in PEERS if peer in selected]
    integrations = ",".join(ordered)
    product = bool(ordered)

    # Fast PRs keep runtime/product correctness tests but move clean SDK/deploy sub-builds to the PRs
    # that can affect those contracts. Every develop/manual integration run executes all of them.
    excluded = [] if readiness_evidence else [READINESS_TEST_PREFIX]
    if not generic_evidence:
        excluded.append("hyremote-generic-installed-consumers$")
    if not cpp_evidence:
        excluded.append("hyremote-cpp-installed-consumers$")
    if not qml_evidence:
        excluded.append("hyremote-qml-deploy-helper-")
    if not qpa_evidence:
        excluded.append("hyremote-qpa-deploy-helper-")
    # An empty exclusion must stay empty. Building "^(" + "|".join([]) + ")" produced "^()", which matches every
    # test name: CTest then excluded everything, reported success, and a lane that promised full integration
    # executed nothing. That is a false green, not a formatting detail.
    test_exclude = "^(" + "|".join(excluded) + ")" if excluded else ""

    return {
        "product": "true" if product else "false",
        "integrations": integrations,
        "qpa": "true" if "qpa" in selected else "false",
        "qml": "true" if "qml" in selected else "false",
        "cpp": "true" if "cpp" in selected else "false",
        "generic_evidence": "true" if generic_evidence else "false",
        "cpp_evidence": "true" if cpp_evidence else "false",
        "qml_evidence": "true" if qml_evidence else "false",
        "qpa_evidence": "true" if qpa_evidence else "false",
        "readiness_evidence": "true" if readiness_evidence else "false",
        "test_exclude": test_exclude,
        "lane": "full integration" if full_evidence else "PR fast",
    }


def self_test() -> int:
    cases = [
        # 1. Unrelated documentation must not drag the expensive lane along (#244 stays intact).
        ("unrelated docs keep the fast lane", "pull_request", ["docs/proposals/notes.md"],
         {"readiness_evidence": "false", "lane": "PR fast"}),
        ("unrelated docs select no capability", "pull_request", ["docs/proposals/notes.md"],
         {"product": "false"}),
        # 2.-4. Readiness-consumed surfaces must execute readiness.
        ("release package manifest runs readiness", "pull_request", ["docs/release-package-manifest.md"],
         {"readiness_evidence": "true"}),
        ("input model runs readiness", "pull_request", ["docs/input-model.md"],
         {"readiness_evidence": "true"}),
        ("readiness gate change runs readiness", "pull_request", ["tests/release-readiness/check_release_metadata.cmake"],
         {"readiness_evidence": "true"}),
        ("release authority change runs readiness", "pull_request", [".github/release/release-trains.json"],
         {"readiness_evidence": "true"}),
        ("install authority change runs readiness", "pull_request", ["cmake/HyRemoteInstall.cmake"],
         {"readiness_evidence": "true"}),
        # 5.-6. The lanes that are supposed to be full integration actually are, and they exclude nothing at all.
        ("develop push runs readiness", "push", ["CMakeLists.txt"],
         {"readiness_evidence": "true", "lane": "full integration", "test_exclude": ""}),
        ("workflow dispatch runs readiness", "workflow_dispatch", ["CMakeLists.txt"],
         {"readiness_evidence": "true", "lane": "full integration", "test_exclude": ""}),
        # 7. A PR that reaches every evidence contract is full integration for that run, so an empty exclusion must
        #    survive as an empty exclusion there too.
        ("full-evidence PR excludes nothing", "pull_request",
         ["src/integrations/cpp/cpp_remote_access.cpp",
          "src/integrations/qml/qml_remote_access.cpp",
          "src/integrations/generic/generic_plugin.cpp",
          "src/integrations/qpa/qpa_platform.cpp",
          "tests/release-readiness/check_release_metadata.cmake"],
         {"readiness_evidence": "true", "test_exclude": ""}),
    ]
    failures = 0
    for description, event, changed, expected in cases:
        resolved = resolve(event, changed)
        for key, value in expected.items():
            if resolved[key] != value:
                print(f"CASE FAILED: {description}: {key}={resolved[key]!r}, expected {value!r}")
                failures += 1
        # A readiness case must really stop excluding the gates, not merely report true.
        if expected.get("readiness_evidence") == "true":
            if READINESS_TEST_PREFIX in resolved["test_exclude"]:
                print(f"CASE FAILED: {description}: readiness still excluded from {resolved['test_exclude']!r}")
                failures += 1
        if expected.get("readiness_evidence") == "false":
            if READINESS_TEST_PREFIX not in resolved["test_exclude"]:
                print(f"CASE FAILED: {description}: fast lane lost the readiness exclusion")
                failures += 1

    # The fast lane must still move the expensive clean consumer sub-builds to the PRs that can affect them.
    fast = resolve("pull_request", ["src/core/session/session.cpp"])
    if READINESS_TEST_PREFIX in fast["test_exclude"] and "hyremote-generic-installed-consumers$" not in fast["test_exclude"]:
        print("CASE FAILED: fast lane must still exclude unrelated clean-consumer sub-builds")
        failures += 1
    for expected in ("hyremote-generic-installed-consumers$", "hyremote-cpp-installed-consumers$",
                     "hyremote-qml-deploy-helper-", "hyremote-qpa-deploy-helper-"):
        if expected not in fast["test_exclude"]:
            print(f"CASE FAILED: fast PR must exclude the sub-build it cannot affect: {expected}")
            failures += 1
    # The fast lane keeps the runtime/product tests it can affect.
    for kept in ("src/integrations/cpp/",):
        cpp_only = resolve("pull_request", [kept + "cpp_remote_access.cpp"])
        if "hyremote-cpp-installed-consumers$" in cpp_only["test_exclude"]:
            print("CASE FAILED: a C++ PR must keep its own clean consumer evidence")
            failures += 1

    # Whatever the lane, an exclusion must never be able to exclude everything: "^()" is the shape that turned a
    # full-integration lane into a no-op, and any other total expression would be just as dishonest. This is a real
    # match test against a name no exclusion may ever match, not a string comparison of the expression.
    for description, event, changed in (
            ("develop push", "push", ["CMakeLists.txt"]),
            ("workflow dispatch", "workflow_dispatch", ["CMakeLists.txt"]),
            ("fast PR", "pull_request", ["src/core/session/session.cpp"]),
            ("documentation PR", "pull_request", ["docs/proposals/notes.md"])):
        resolved = resolve(event, changed)
        if exclusion_is_total(resolved["test_exclude"]):
            print(f"CASE FAILED: {description} produced an exclusion that matches every test: "
                  f"{resolved['test_exclude']!r}")
            failures += 1

    if failures:
        print(f"resolve-ci-scope self test: {failures} contradiction(s)")
        return 1
    print("resolve-ci-scope self test: PASS (empty exclusions stay empty, no exclusion can match every test, fast "
          "lane preserved, readiness runs on develop, dispatch and readiness-consumed changes)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--event", default=os.environ.get("EVENT_NAME", ""))
    parser.add_argument("--base", default=os.environ.get("BASE_SHA", ""))
    parser.add_argument("--head", default=os.environ.get("HEAD_SHA", ""))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    if not args.event:
        print("--event is required", file=sys.stderr)
        return 2

    outputs = resolve(args.event, changed_paths(args.event, args.base, args.head))

    output_path = os.environ.get("GITHUB_OUTPUT")
    lines = [f"{key}={value}" for key, value in outputs.items()]
    if output_path:
        with open(output_path, "a", encoding="utf-8") as handle:
            handle.write("\n".join(lines) + "\n")
    for line in lines:
        print(line)

    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary_path:
        with open(summary_path, "a", encoding="utf-8") as handle:
            handle.write("## CI scope\n\n")
            handle.write(f"- integrations: {outputs['integrations'] or 'none'}\n")
            handle.write(f"- Qt build: {'yes' if outputs['product'] == 'true' else 'no - documentation/governance only'}\n")
            handle.write("- build type: Release\n")
            handle.write(f"- Generic/C++/QML/QPA deploy evidence: {outputs['generic_evidence']}/"
                         f"{outputs['cpp_evidence']}/{outputs['qml_evidence']}/{outputs['qpa_evidence']}\n")
            handle.write(f"- release readiness: {outputs['readiness_evidence']}\n")
            handle.write(f"- lane: {outputs['lane']}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
