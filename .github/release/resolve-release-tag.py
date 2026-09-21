#!/usr/bin/env python3
"""Resolve which release tag a Git Flow policy run must audit, and whether it must audit one at all.

The release-tag audit has two entry points and one target:

* a normal tag push audits the tag that was pushed;
* an explicit workflow dispatch audits an existing immutable tag named by its input;
* anything else audits nothing, so an ordinary manual policy run never triggers a tag audit.

This resolver exists so that decision is a pure function that can be executed by a self test. The previous audit
compared the tag against an environment value a sibling job happened to set, and jobs do not share environments, so
the comparison could never pass - a defect that a testable function would have caught before the first real tag.

The release-train authority itself is not re-implemented here. Resolving a version to an authorized train stays with
the machine selector in tests/release-readiness/release_scope.cmake, which already refuses unknown versions, retired
planning labels and conditional versions without recorded activation. This script only decides the target and asks that
selector whether the target names a train.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

VERSION_PATTERN = re.compile(r"v([0-9]+(?:\.[0-9]+){3})")
SELECTOR = Path("tests/release-readiness/release_scope.cmake")


class ResolutionError(Exception):
    """The run cannot produce a trustworthy target."""


def resolve_target(event: str, ref_name: str, release_tag: str) -> tuple[bool, str]:
    """Return (skip, tag). Raises ResolutionError for input that must fail rather than skip."""
    if event == "push":
        # A branch push is not a release; only a tag ref is.
        if not ref_name.startswith("refs/tags/") and not VERSION_PATTERN.fullmatch(ref_name):
            return True, ""
        candidate = ref_name[len("refs/tags/"):] if ref_name.startswith("refs/tags/") else ref_name
    elif event == "workflow_dispatch":
        candidate = release_tag.strip()
        if not candidate:
            # A manual policy run that names no tag is deliberately not an audit.
            return True, ""
    else:
        return True, ""

    if not VERSION_PATTERN.fullmatch(candidate):
        raise ResolutionError(
            f"release tags must use vMajor.Minor.Feature.Maintenance, got '{candidate}'"
        )
    return False, candidate


def assert_authorized(repo_root: Path, tag: str) -> str:
    """Ask the machine selector whether the tag names an authorized train; return the version."""
    version = tag[1:]
    selector = repo_root / SELECTOR
    if not selector.is_file():
        raise ResolutionError(f"missing release authority selector {selector}")
    completed = subprocess.run(
        ["cmake", f"-DHYREMOTE_SOURCE_DIR={repo_root}", f"-DHYREMOTE_RELEASE_VERSION={version}",
         "-P", str(selector)],
        text=True, capture_output=True,
    )
    if completed.returncode != 0:
        detail = "\n".join(
            line for line in (completed.stdout + completed.stderr).splitlines()
            if "release-scope" in line or "CMake Error" not in line
        ).strip()
        raise ResolutionError(f"tag {tag} is not an authorized release train: {detail}")
    return version


def self_test() -> int:
    failures = 0
    repo_root = Path(__file__).resolve().parents[2]

    def expect(description: str, event: str, ref_name: str, release_tag: str,
               expected_skip: bool | None, expected_tag: str | None) -> None:
        nonlocal failures
        try:
            skip, tag = resolve_target(event, ref_name, release_tag)
        except ResolutionError as error:
            if expected_skip is None:
                print(f"case ok: {description} -> refused ({error})")
                return
            print(f"CASE FAILED: {description}: refused with {error}")
            failures += 1
            return
        if expected_skip is None:
            print(f"CASE FAILED: {description}: expected refusal, got skip={skip} tag={tag!r}")
            failures += 1
            return
        if skip != expected_skip or tag != expected_tag:
            print(f"CASE FAILED: {description}: skip={skip} tag={tag!r}, expected skip={expected_skip} "
                  f"tag={expected_tag!r}")
            failures += 1
            return
        print(f"case ok: {description} (skip={skip} tag={tag or '-'})")

    # A pushed tag is the audit target.
    expect("pushed tag is the target", "push", "v0.1.0.0", "", False, "v0.1.0.0")
    expect("pushed refs/tags ref is the target", "push", "refs/tags/v0.1.0.0", "", False, "v0.1.0.0")
    # A dispatch naming a tag audits the same target as the push would have.
    expect("dispatch with a tag matches the push target", "workflow_dispatch", "", "v0.1.0.0", False, "v0.1.0.0")
    # A dispatch with no tag, a branch push and any other event audit nothing.
    expect("manual run without a tag audits nothing", "workflow_dispatch", "", "", True, "")
    expect("manual run with blank whitespace audits nothing", "workflow_dispatch", "", "   ", True, "")
    expect("branch push audits nothing", "push", "refs/heads/develop", "", True, "")
    expect("unrelated event audits nothing", "pull_request", "refs/pull/1/merge", "", True, "")
    # Malformed input must fail, not skip silently.
    expect("malformed tag is refused", "workflow_dispatch", "", "foo", None, None)
    expect("short version is refused", "workflow_dispatch", "", "v0.1", None, None)
    expect("missing v prefix is refused", "workflow_dispatch", "", "0.1.0.0", None, None)

    # The authority cases go through the machine selector, not through a second parser.
    for description, tag, expected_pass in (
            ("authorized train resolves", "v0.1.0.0", True),
            ("retired planning label is refused", "v0.0.1.0", False),
            ("unknown version is refused", "v9.9.9.9", False)):
        try:
            version = assert_authorized(repo_root, tag)
            if expected_pass:
                print(f"case ok: {description} (version={version})")
            else:
                print(f"CASE FAILED: {description}: resolved to {version} but must be refused")
                failures += 1
        except ResolutionError as error:
            if expected_pass:
                print(f"CASE FAILED: {description}: {error}")
                failures += 1
            else:
                print(f"case ok: {description} -> refused")

    if failures:
        print(f"resolve-release-tag self test: {failures} contradiction(s)")
        return 1
    print("resolve-release-tag self test: PASS (one audit target for both entry points, manual runs without a tag "
          "audit nothing, malformed tags fail, and the release train authority stays with the machine selector)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--event", default=os.environ.get("EVENT_NAME", ""))
    parser.add_argument("--ref-name", default=os.environ.get("REF_NAME", ""))
    parser.add_argument("--release-tag", default=os.environ.get("RELEASE_TAG_INPUT", ""))
    parser.add_argument("--repo-root", default=str(Path(__file__).resolve().parents[2]))
    parser.add_argument("--skip-authority", action="store_true",
                        help="only decide the target; used by the workflow for the skip path")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    try:
        skip, tag = resolve_target(args.event, args.ref_name, args.release_tag)
        version = ""
        if not skip and not args.skip_authority:
            version = assert_authorized(Path(args.repo_root), tag)
    except ResolutionError as error:
        print(f"resolve-release-tag: {error}", file=sys.stderr)
        return 1

    lines = [f"skip={'true' if skip else 'false'}", f"target_tag={tag}", f"version={version}"]
    output_path = os.environ.get("GITHUB_OUTPUT")
    if output_path:
        with open(output_path, "a", encoding="utf-8") as handle:
            handle.write("\n".join(lines) + "\n")
    for line in lines:
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
