#!/usr/bin/env python3
"""Resolve the CI scope for one event.

The workflow calls this script instead of carrying the classifier inline, so the rules that decide which capabilities
to build and which tests to exclude are expressed once and can be executed by a self test. A classifier that only
exists inside a workflow cannot be tested, and an untested classifier is how a lane that claims to be "full
integration" can quietly exclude the release-readiness gates forever.

Contract:
  * ``--event``, ``--base``, ``--head``, ``--draft`` describe the run.
  * Every derived value is written to ``$GITHUB_OUTPUT`` when it is set, and printed otherwise.
  * ``--self-test`` proves the required cases and exits non-zero on the first contradiction.

Lanes (``lane``):
  * ``PR_DRAFT`` - a draft pull request runs cheap governance only. Product jobs require a ready pull request, so a
    draft never starts the Windows/Linux Qt matrix.
  * ``PR_FAST`` - a ready pull request is classified from its real ``base...head`` diff and validates only what that
    diff can affect: the affected product capabilities, the deploy evidence for contracts it touches, and readiness
    when it touches a readiness-consumed surface.
  * ``DEVELOP_SENTINEL`` - a ``develop`` push runs seconds-scale repository policy. The merged pull request already
    passed its own hosted acceptance, so re-running the whole dual-platform matrix on every merge only duplicates
    cost without producing a new decision.
  * ``FULL_GATE`` - the complete dual-platform, four-frontend, all-evidence run. It is reachable only through an
    explicit ``workflow_dispatch`` (an exact candidate, a release branch or main validation is dispatched the same
    way), never as a side effect of an ordinary change.
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

# Examples and logo assets are only product inputs when they are actually built. A README under examples/ is
# documentation: treating every path below examples/ as product code meant that editing examples/README.md selected
# all four frontends and started two Qt runners to build nothing.
EXAMPLE_SOURCE_ROOTS = ("examples/", "logo/")
EXAMPLE_BUILD_SUFFIXES = (
    ".cmake", ".cpp", ".cc", ".cxx", ".c", ".h", ".hh", ".hpp", ".qml", ".qrc", ".ui", ".ts", ".json",
)
EXAMPLE_BUILD_NAMES = ("CMakeLists.txt",)

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

# Release-authority surfaces that need no Qt SDK, no Windows runner and no product build: they are CMake policy and
# selection scripts over the repository itself. They run in a lightweight governance job, and they must not drag the
# product matrix along just to execute a policy script.
GOVERNANCE_PREFIXES = (
    ".github/release/",
    ".github/workflows/git-flow-policy.yml",
    "tests/release-readiness/release_scope.cmake",
    "tests/release-readiness/check_release_authority_policy.cmake",
)

# The same two scripts, listed exactly, are excluded from product selection even though they live under a directory
# whose other contents are product-relevant: a change to the selector or the authority policy is governance.
GOVERNANCE_ONLY_PATHS = (
    "tests/release-readiness/release_scope.cmake",
    "tests/release-readiness/check_release_authority_policy.cmake",
)

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


def is_built_example_path(path: str) -> bool:
    """True when a path below examples/ or logo/ is something the build actually consumes."""
    if not path.startswith(EXAMPLE_SOURCE_ROOTS):
        return False
    name = path.rsplit("/", 1)[-1]
    return name in EXAMPLE_BUILD_NAMES or name.endswith(EXAMPLE_BUILD_SUFFIXES)


def lane_for(event: str, draft: bool) -> str:
    if event == "workflow_dispatch":
        return "FULL_GATE"
    if event == "push":
        return "DEVELOP_SENTINEL"
    if event == "pull_request":
        return "PR_DRAFT" if draft else "PR_FAST"
    return "UNKNOWN"


def changed_paths(event: str, base: str, head: str) -> list[str]:
    if event == "pull_request":
        return subprocess.check_output(
            ["git", "diff", "--name-only", f"{base}...{head}"], text=True
        ).splitlines()
    # A develop push and a manual dispatch are decided by their event, not by a diff. The sentinel lane runs policy
    # only, and the full gate is full by definition, so no path needs to be inspected - and inventing one (the former
    # "CMakeLists.txt" placeholder) claimed every capability for every merge.
    return []


def resolve(event: str, changed: list[str], draft: bool = False) -> dict[str, str]:
    lane = lane_for(event, draft)
    full_gate = lane == "FULL_GATE"
    product_lane = lane in ("PR_FAST", "FULL_GATE")
    selected: set[str] = set()

    # The full gate is full by definition: every frontend and every piece of deploy evidence, regardless of a diff,
    # because its whole purpose is the complete dual-platform acceptance run.
    if full_gate:
        selected.update(PEERS)

    for path in changed:
        if path in GOVERNANCE_ONLY_PATHS:
            continue
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
        if is_built_example_path(path):
            selected.update(PEERS)
        elif path.startswith("tests/"):
            selected.update(PEERS)

    def evidence_for(prefixes: tuple[str, ...]) -> bool:
        return full_gate or any(
            path.startswith(COMMON_DEPLOY_PATHS + prefixes) for path in changed
        )

    generic_evidence = evidence_for(("src/integrations/generic/", "tests/consumer-installed-generic/"))
    qml_evidence = evidence_for(("src/integrations/qml/", "tests/consumer-installed-qml/"))
    qpa_evidence = evidence_for(("src/integrations/qpa/", "tests/consumer-installed-qpa/"))
    cpp_evidence = evidence_for((
        "src/integrations/cpp/", "tests/consumer-installed-sdk/", "tests/consumer-installed-cpp/",
    ))

    # The full gate always runs readiness. A ready pull request runs it only when the change can invalidate it.
    readiness_evidence = full_gate or any(
        path.startswith(READINESS_PREFIXES) for path in changed
    )

    governance = full_gate or any(
        path.startswith(GOVERNANCE_PREFIXES) or path in GOVERNANCE_ONLY_PATHS for path in changed
    )

    ordered = [peer for peer in PEERS if peer in selected]
    integrations = ",".join(ordered)
    # A capability is only built when the lane is allowed to build anything: the sentinel is policy, and a draft has
    # not asked for review yet.
    product = bool(ordered) and product_lane

    # Fast pull requests keep runtime/product correctness tests but move clean SDK/deploy sub-builds to the PRs
    # that can affect those contracts.
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
    # executed nothing. That is a false green, not a formatting detail. A lane that runs no product job has no
    # exclusion to report at all, so it reports none rather than a meaningless expression.
    test_exclude = "^(" + "|".join(excluded) + ")" if (excluded and product) else ""

    return {
        "lane": lane,
        "product": "true" if product else "false",
        "integrations": integrations if product else "",
        "qpa": "true" if ("qpa" in selected and product) else "false",
        "qml": "true" if ("qml" in selected and product) else "false",
        "cpp": "true" if ("cpp" in selected and product) else "false",
        "generic_evidence": "true" if (generic_evidence and product) else "false",
        "cpp_evidence": "true" if (cpp_evidence and product) else "false",
        "qml_evidence": "true" if (qml_evidence and product) else "false",
        "qpa_evidence": "true" if (qpa_evidence and product) else "false",
        "readiness_evidence": "true" if readiness_evidence else "false",
        "governance": "true" if governance else "false",
        "test_exclude": test_exclude,
    }


def self_test() -> int:
    failures = 0

    cases = [
        # Draft pull requests and the develop sentinel must not start the product matrix at all.
        ("draft product PR runs no product job", "pull_request", ["src/runtime/runtime.cpp"], True,
         {"lane": "PR_DRAFT", "product": "false", "integrations": ""}),
        ("draft governance PR runs no product job", "pull_request", ["README.md"], True,
         {"lane": "PR_DRAFT", "product": "false"}),
        ("develop push is the sentinel lane", "push", [], False,
         {"lane": "DEVELOP_SENTINEL", "product": "false", "integrations": "", "test_exclude": ""}),
        ("develop sentinel runs no deploy evidence", "push", [], False,
         {"generic_evidence": "false", "cpp_evidence": "false", "qml_evidence": "false", "qpa_evidence": "false"}),
        ("manual dispatch is the full gate", "workflow_dispatch", [], False,
         {"lane": "FULL_GATE", "product": "true", "integrations": "cpp,qml,generic,qpa"}),
        ("full gate runs all evidence", "workflow_dispatch", [], False,
         {"generic_evidence": "true", "cpp_evidence": "true", "qml_evidence": "true", "qpa_evidence": "true",
          "readiness_evidence": "true", "test_exclude": ""}),
        # Example documentation is documentation; example build inputs are product inputs.
        ("examples README does not select product", "pull_request", ["examples/README.md"], False,
         {"product": "false", "integrations": ""}),
        ("learning example README does not select product", "pull_request",
         ["examples/learning/01-widgets-cpp/README.md"], False, {"product": "false"}),
        ("example source file selects product", "pull_request",
         ["examples/learning/01-widgets-cpp/main.cpp"], False, {"product": "true"}),
        ("example build file selects product", "pull_request",
         ["examples/learning/02-quick-cpp/CMakeLists.txt"], False, {"product": "true"}),
        ("logo resource selects product", "pull_request", ["logo/hyremote-branding.qrc"], False,
         {"product": "true"}),
        ("logo documentation does not select product", "pull_request", ["logo/README.md"], False,
         {"product": "false"}),
        # Release authority is governance: no Qt SDK, no Windows runner, no product matrix.
        ("release authority is governance, not product", "pull_request",
         [".github/release/release-trains.json"], False,
         {"governance": "true", "product": "false"}),
        ("release scope selector is governance, not product", "pull_request",
         ["tests/release-readiness/release_scope.cmake"], False,
         {"governance": "true", "product": "false"}),
        ("authority policy gate is governance, not product", "pull_request",
         ["tests/release-readiness/check_release_authority_policy.cmake"], False,
         {"governance": "true", "product": "false"}),
        ("release policy workflow is governance", "pull_request",
         [".github/workflows/git-flow-policy.yml"], False, {"governance": "true"}),
        # Ready product pull requests are classified by the real diff (#244 stays intact).
        ("ready runtime PR builds every capability", "pull_request", ["src/runtime/runtime.cpp"], False,
         {"lane": "PR_FAST", "product": "true", "integrations": "cpp,qml,generic,qpa"}),
        ("ready cpp PR builds the cpp capability", "pull_request",
         ["src/integrations/cpp/cpp_remote_access.cpp"], False, {"product": "true"}),
        ("unrelated docs keep the fast lane", "pull_request", ["docs/proposals/notes.md"], False,
         {"lane": "PR_FAST", "product": "false"}),
        # Readiness-consumed surfaces must execute readiness.
        ("release package manifest runs readiness", "pull_request", ["docs/release-package-manifest.md"], False,
         {"readiness_evidence": "true"}),
        ("input model runs readiness", "pull_request", ["docs/input-model.md"], False,
         {"readiness_evidence": "true"}),
        ("readiness gate change runs readiness", "pull_request",
         ["tests/release-readiness/check_release_metadata.cmake"], False, {"readiness_evidence": "true"}),
        ("release authority change runs readiness", "pull_request", [".github/release/release-trains.json"], False,
         {"readiness_evidence": "true"}),
        ("install authority change runs readiness", "pull_request", ["cmake/HyRemoteInstall.cmake"], False,
         {"readiness_evidence": "true"}),
        # A PR that reaches every evidence contract is full integration for that run, so it excludes nothing - an
        # empty exclusion must survive as an empty exclusion.
        ("full-evidence PR excludes nothing", "pull_request",
         ["src/integrations/cpp/cpp_remote_access.cpp",
          "src/integrations/qml/qml_remote_access.cpp",
          "src/integrations/generic/generic_plugin.cpp",
          "src/integrations/qpa/qpa_platform.cpp",
          "tests/release-readiness/run_release_evidence.cmake"], False, {"test_exclude": ""}),
    ]

    for description, event, changed, draft, expected in cases:
        resolved = resolve(event, changed, draft)
        for key, value in expected.items():
            if resolved[key] != value:
                print(f"CASE FAILED: {description}: {key}={resolved[key]!r}, expected {value!r}")
                failures += 1
        # A readiness case must really stop excluding the gates, not merely report true.
        if expected.get("readiness_evidence") == "true" and resolved["product"] == "true":
            if READINESS_TEST_PREFIX in resolved["test_exclude"]:
                print(f"CASE FAILED: {description}: readiness still excluded from {resolved['test_exclude']!r}")
                failures += 1
        if expected.get("readiness_evidence") == "false" and resolved["product"] == "true":
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
    cpp_only = resolve("pull_request", ["src/integrations/cpp/cpp_remote_access.cpp"])
    if "hyremote-cpp-installed-consumers$" in cpp_only["test_exclude"]:
        print("CASE FAILED: a C++ PR must keep its own clean consumer evidence")
        failures += 1

    # Whatever the lane, an exclusion must never be able to exclude everything: "^()" is the shape that turned a
    # full-integration lane into a no-op, and any other total expression would be just as dishonest. This is a real
    # match test against a name no exclusion may ever match, not a string comparison of the expression.
    for description, event, changed, draft in (
            ("develop sentinel", "push", [], False),
            ("full gate", "workflow_dispatch", [], False),
            ("fast PR", "pull_request", ["src/core/session/session.cpp"], False),
            ("documentation PR", "pull_request", ["docs/proposals/notes.md"], False),
            ("draft product PR", "pull_request", ["src/runtime/runtime.cpp"], True)):
        resolved = resolve(event, changed, draft)
        if exclusion_is_total(resolved["test_exclude"]):
            print(f"CASE FAILED: {description} produced an exclusion that matches every test: "
                  f"{resolved['test_exclude']!r}")
            failures += 1

    if failures:
        print(f"resolve-ci-scope self test: {failures} contradiction(s)")
        return 1
    print("resolve-ci-scope self test: PASS (draft and sentinel lanes run no product job, examples documentation "
          "selects no capability, release authority is governance, empty exclusions stay empty, no exclusion can "
          "match every test, fast lane preserved, readiness runs where it is consumed)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--event", default=os.environ.get("EVENT_NAME", ""))
    parser.add_argument("--base", default=os.environ.get("BASE_SHA", ""))
    parser.add_argument("--head", default=os.environ.get("HEAD_SHA", ""))
    parser.add_argument("--draft", default=os.environ.get("PR_IS_DRAFT", ""))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    if not args.event:
        print("--event is required", file=sys.stderr)
        return 2

    draft = str(args.draft).strip().lower() in ("1", "true", "yes")
    outputs = resolve(args.event, changed_paths(args.event, args.base, args.head), draft)

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
            handle.write(f"- lane: {outputs['lane']}\n")
            handle.write(f"- integrations: {outputs['integrations'] or 'none'}\n")
            handle.write(f"- Qt build: {'yes' if outputs['product'] == 'true' else 'no'}\n")
            handle.write("- build type: Release\n")
            handle.write(f"- Generic/C++/QML/QPA deploy evidence: {outputs['generic_evidence']}/"
                         f"{outputs['cpp_evidence']}/{outputs['qml_evidence']}/{outputs['qpa_evidence']}\n")
            handle.write(f"- release readiness: {outputs['readiness_evidence']}\n")
            handle.write(f"- release-authority governance: {outputs['governance']}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
