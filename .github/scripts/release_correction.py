"""Candidate correction for an already-versioned protected release branch.

A release branch is versioned by its first preparation PR. Before this module existed the only admitted
correction was hard-coded to the historical V0.3.0.0 pair of commits, so any later release that had to
re-base onto a reviewed develop candidate (for example to pick up an evidence-neutral CI fix) had no
reviewed path at all: the branch could only finalize its notes.

The generalized rule keeps the same safety property without naming a version:

* the head tree must be the exact ``origin/develop`` candidate tree, with only the root
  ``project(VERSION ...)`` token replaced by the release version;
* the pending bilingual notes file must be byte-identical to the one on the release branch;
* therefore ``git diff --name-only origin/develop HEAD`` is exactly ``{"CMakeLists.txt"}``.

It does not authorize a merge, a tag or a release, and it does not bypass branch protection: the
correction still arrives as a reviewed ``feature/<issue>-release-preparation`` pull request and the
release still needs its exact-SHA gate.

Run ``python .github/scripts/release_correction.py --self-test`` for the negative/positive cases.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

PROJECT_VERSION = re.compile(
    rb"^[ \t]*project\s*\(\s*HyRemote\b(?:(?!\)).)*?"
    rb"\bVERSION\s+(?P<version>[0-9]+(?:\.[0-9]+){2,3})(?=\s|\))",
    re.M | re.S,
)

CANDIDATE_MARKER = "Status: **candidate / acceptance pending**"
DEVELOP_REF = "refs/remotes/origin/develop"


def versioned_cmake(develop_cmake: bytes, version: str) -> bytes:
    """Return the develop CMakeLists with only the root VERSION token replaced."""
    matches = list(PROJECT_VERSION.finditer(develop_cmake))
    if len(matches) != 1:
        raise SystemExit("the develop candidate must declare exactly one unambiguous HyRemote VERSION")
    match = matches[0]
    return (develop_cmake[: match.start("version")] + version.encode("ascii")
            + develop_cmake[match.end("version"):])


def validate(*, git, version, notes_name, base_notes, head_notes, head_cmake, develop_ref=DEVELOP_REF):
    """Raise SystemExit unless the correction is exactly a re-base onto the develop candidate.

    ``git`` is a callable returning bytes for a git argument tuple.
    """
    if not base_notes.startswith(CANDIDATE_MARKER) or head_notes != base_notes:
        raise SystemExit(
            "candidate correction must keep the pending bilingual notes unchanged; "
            "finalizing them is a separate notes-finalization PR")
    if CANDIDATE_MARKER not in head_notes:
        raise SystemExit("candidate correction must not remove the pending candidate marker")

    develop_cmake = git("show", f"{develop_ref}:CMakeLists.txt")
    if head_cmake != versioned_cmake(develop_cmake, version):
        raise SystemExit(
            "candidate correction may differ from the develop candidate CMakeLists only in the root "
            f"project VERSION token ({version})")

    surface = set(git("diff", "--name-only", develop_ref, "HEAD").decode("utf-8").splitlines())
    if surface != {"CMakeLists.txt"}:
        raise SystemExit(
            "candidate correction head must equal the develop candidate except the root VERSION: "
            f"found {sorted(surface)} instead of ['CMakeLists.txt']")

    if git("rev-parse", develop_ref).decode("ascii").strip() == git(
            "rev-parse", "HEAD").decode("ascii").strip():
        raise SystemExit("candidate correction head cannot be the develop commit itself")

    return "candidate-correction"


# ---------------------------------------------------------------- self test

def _run(cwd: Path, *args: str) -> bytes:
    return subprocess.check_output(args, cwd=cwd, stderr=subprocess.DEVNULL)


def _write(root: Path, relative: str, text: str) -> None:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _commit(root: Path, message: str) -> None:
    _run(root, "git", "add", "-A")
    _run(root, "git", "-c", "user.email=t@example.com", "-c", "user.name=t", "commit", "-m", message)


def _git_in(root: Path):
    def git(*args: str) -> bytes:
        return _run(root, "git", *args)
    return git


def self_test() -> int:
    failures = 0
    cases = 0

    marker_notes = CANDIDATE_MARKER + "\n\n# 中文\n- 候选\n\n# English\n- candidate\n"

    def scenario(name: str, *, develop_extra: bool, head_extra_file: bool,
                 break_notes: bool, break_cmake: bool, expect_error: str | None,
                 break_version_token: bool = False) -> None:
        nonlocal failures, cases
        cases += 1
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            _run(root, "git", "init", "-q")
            _run(root, "git", "config", "core.autocrlf", "false")
            _write(root, "CMakeLists.txt",
                   'project(HyRemote VERSION 0.0.0 DESCRIPTION "x" LANGUAGES C CXX)\n')
            _write(root, "docs/releases/v9.9.9.9.md", marker_notes)
            _write(root, "src/a.cpp", "int a = 1;\n")
            _commit(root, "develop base")
            if develop_extra:
                _write(root, "src/b.cpp", "int b = 2;\n")
                _commit(root, "develop extra")
            _run(root, "git", "update-ref", DEVELOP_REF, "HEAD")

            # Release branch base: versioned from the same tree, notes pending.
            _run(root, "git", "switch", "-q", "-c", "release")
            _write(root, "CMakeLists.txt",
                   'project(HyRemote VERSION 9.9.9.9 DESCRIPTION "x" LANGUAGES C CXX)\n')
            _commit(root, "version")
            base_notes = _run(root, "git", "show",
                              "HEAD:docs/releases/v9.9.9.9.md").decode("utf-8")

            # Head: re-base onto the develop candidate, then version it.
            _run(root, "git", "switch", "-q", "-c", "head", DEVELOP_REF)
            if head_extra_file:
                _write(root, "src/c.cpp", "int c = 3;\n")
                _commit(root, "undisclosed extra")
            if break_cmake:
                _write(root, "src/a.cpp", "int a = 42;\n")
                _commit(root, "product change")
            _write(root, "CMakeLists.txt",
                   'project(HyRemote VERSION 9.9.9.9 DESCRIPTION "y" LANGUAGES C CXX)\n'
                   if break_version_token else
                   'project(HyRemote VERSION 9.9.9.9 DESCRIPTION "x" LANGUAGES C CXX)\n')
            if break_notes:
                _write(root, "docs/releases/v9.9.9.9.md", marker_notes + "\nextra\n")
            _commit(root, "correction")

            git = _git_in(root)
            try:
                validate(git=git, version="9.9.9.9", notes_name="docs/releases/v9.9.9.9.md",
                         base_notes=base_notes,
                         head_notes=git("show", "HEAD:docs/releases/v9.9.9.9.md").decode("utf-8"),
                         head_cmake=git("show", "HEAD:CMakeLists.txt"))
                if expect_error is not None:
                    print(f"FAIL {name}: correction was accepted but '{expect_error}' was expected")
                    failures += 1
                else:
                    print(f"ok: {name}")
            except SystemExit as error:
                if expect_error is None:
                    print(f"FAIL {name}: unexpected rejection: {error}")
                    failures += 1
                elif expect_error not in str(error):
                    print(f"FAIL {name}: rejected with '{error}', expected '{expect_error}'")
                    failures += 1
                else:
                    print(f"ok: {name}")

    scenario("exact develop candidate re-base is admitted",
             develop_extra=True, head_extra_file=False, break_notes=False,
             break_cmake=False, expect_error=None)
    scenario("undeclared product drift is rejected",
             develop_extra=True, head_extra_file=True, break_notes=False,
             break_cmake=False, expect_error="must equal the develop candidate")
    scenario("product change beyond the candidate is rejected",
             develop_extra=False, head_extra_file=False, break_notes=False,
             break_cmake=True, expect_error="must equal the develop candidate")
    scenario("CMake metadata change beyond the VERSION token is rejected",
             develop_extra=False, head_extra_file=False, break_notes=False,
             break_cmake=False, expect_error="may differ from the develop candidate",
             break_version_token=True)
    scenario("notes must stay pending and unchanged",
             develop_extra=False, head_extra_file=False, break_notes=True,
             break_cmake=False, expect_error="pending bilingual notes unchanged")

    if failures:
        print(f"release-correction self test: {failures} of {cases} case(s) failed")
        return 1
    print(f"release-correction self test: PASS ({cases} cases)")
    return 0


def main(argv: list[str]) -> int:
    if "--self-test" in argv:
        return self_test()

    head = os.environ.get("HEAD_REF", "")
    base = os.environ.get("BASE_REF", "")
    branch = re.fullmatch(r"release/v([0-9]+(?:\.[0-9]+){3})", base)
    if not branch or not re.fullmatch(r"feature/[0-9]+-release-preparation", head):
        raise SystemExit("only issue-linked feature/<issue>-release-preparation may target an exact release branch")
    version = branch.group(1)
    notes_name = f"docs/releases/v{version}.md"

    def git(*args: str) -> bytes:
        return subprocess.check_output(("git", *args))

    base_tree = f"refs/remotes/origin/{base}"
    base_notes = git("show", f"{base_tree}:{notes_name}").decode("utf-8")
    head_notes = git("show", f"HEAD:{notes_name}").decode("utf-8")
    head_cmake = git("show", "HEAD:CMakeLists.txt")

    stage = validate(git=git, version=version, notes_name=notes_name, base_notes=base_notes,
                     head_notes=head_notes, head_cmake=head_cmake)
    changed = set(git("diff", "--name-only", base_tree, "HEAD").decode("utf-8").splitlines())
    print(f"protected release preparation gate PASS: {head} -> {base} phase={stage} "
          f"scope={sorted(changed)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
