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


# #806: a previously accepted RC6 note must be explicitly reopened after newly verified release-blocking
# P2 defects. This exception is SHA-locked to a single unpublished release branch and does NOT generalize
# the right to overwrite accepted release history. Its next correction is a fresh reviewed candidate.
REOPEN_VERSION = "0.3.5.0"
REOPEN_BASE_SHA = "377b0a61b5dd99e3c16ea88356463068b255c7a6"
REOPEN_ACCEPTED_MARKER = "Status: **accepted for V0.3.5.0**"


def validate_reopen(*, git, version, notes_name, base_notes, head_notes, head_cmake,
                    actual_base_sha, develop_ref=DEVELOP_REF):
    """Fail closed for the single Owner-authorized post-acceptance #793 correction.

    This gate is evaluated by a protected-release PR review, *never* by a direct branch update.
    The candidate tree must equal reviewed develop byte-for-byte other than its root VERSION;
    its notes must be exactly the pending develop notes, not arbitrary replacement prose.
    """
    if version != REOPEN_VERSION or actual_base_sha != REOPEN_BASE_SHA:
        raise SystemExit("post-acceptance release reopen is not authorized for this exact version/base SHA")
    if not base_notes.startswith(REOPEN_ACCEPTED_MARKER):
        raise SystemExit("post-acceptance release reopen requires the exact accepted RC6 notes")
    develop_notes = git("show", f"{develop_ref}:{notes_name}").decode("utf-8")
    if not develop_notes.startswith(CANDIDATE_MARKER) or head_notes != develop_notes:
        raise SystemExit("reopened release notes must equal the exact pending develop notes")
    develop_cmake = git("show", f"{develop_ref}:CMakeLists.txt")
    if head_cmake != versioned_cmake(develop_cmake, version):
        raise SystemExit("reopened release CMakeLists must equal reviewed develop except the VERSION token")
    surface = set(git("diff", "--name-only", develop_ref, "HEAD").decode("utf-8").splitlines())
    if surface != {"CMakeLists.txt"}:
        raise SystemExit("reopened candidate tree must equal develop except the root VERSION")
    return "candidate-reopen-correction"



# #812 / #768: a second, separately reviewed, strictly bounded reopen of the
# ACCEPTED but UNPUBLISHED RC7 after independent whole-release P2 findings.
# Never use this gate for another release, accepted base, candidate tree or feature.
RC7_REOPEN_BASE_SHA = "a5fd439ed4039614021f2254dd2df6b76ffd4374"
RC7_REVIEWED_PRODUCT_SOURCE_SHA = "1b6b2f4f991f773e6f48b8484d0f3b8905d0f72e"
RC7_PRODUCT_FILES = frozenset({
    "src/runtime/src/access_instance.cpp",
    "src/runtime/tests/test_watermark_policy.cpp",
    "src/integrations/cpp/tests/test_remote_access.cpp",
    "tools/local-owner/worker.py",
})
RC7_GOVERNANCE_FILES = frozenset({
    ".github/scripts/release_correction.py",
    ".github/workflows/git-flow-policy.yml",
})


def validate_reopen_rc7(*, git, version, notes_name, base_notes, head_notes,
                        base_cmake, head_cmake, actual_base_sha, head_ref,
                        develop_ref=DEVELOP_REF,
                        fixed_source_ref=RC7_REVIEWED_PRODUCT_SOURCE_SHA):
    """Admit only the independently reviewed #812/#768 repairs, governance and
    exact pending bilingual notes on the frozen accepted-but-untagged RC7.

    Unrelated develop files (including architecture, roadmap, performance work
    orders, other versions, transport/API work) are NEVER imported by this gate.
    """
    if head_ref != "feature/812-release-preparation":
        raise SystemExit("RC7 post-acceptance reopen denied on unauthorized preparation branch")
    if version != "0.3.5.0" or actual_base_sha != RC7_REOPEN_BASE_SHA:
        raise SystemExit("RC7 post-acceptance reopen denied on unapproved version/base")
    if not base_notes.startswith(REOPEN_ACCEPTED_MARKER):
        raise SystemExit("RC7 reopen requires previously accepted notes on the exact base")
    if head_cmake != base_cmake:
        raise SystemExit("RC7 source correction must not alter the release VERSION/CMakeLists")
    pending = git("show", f"{develop_ref}:{notes_name}").decode("utf-8")
    if not pending.startswith(CANDIDATE_MARKER) or head_notes != pending:
        raise SystemExit("RC7 correction notes must equal reviewed develop's exact pending bilingual notes")
    if "# 中文" not in head_notes or "# English" not in head_notes or "## Security boundary" not in head_notes:
        raise SystemExit("RC7 corrected candidate notes must retain bilingual/security structure")
    expected = RC7_PRODUCT_FILES | RC7_GOVERNANCE_FILES | {notes_name}
    changed = set(git("diff", "--name-only", actual_base_sha, "HEAD").decode("utf-8").splitlines())
    if changed != expected:
        raise SystemExit(
            "RC7 correction changed-file scope must be the exact P2+governance+notes set: "
            f"expected {sorted(expected)}, found {sorted(changed)}")
    # The four real source fixes must be byte-for-byte identical to the SHA
    # that merged reviewed #813 and #816 into develop, independent of later
    # unrelated develop movements. The governance & pending-notes bytes come
    # only from the separately reviewed current develop branch.
    for file_name in sorted(RC7_PRODUCT_FILES):
        if git("show", f"HEAD:{file_name}") != git("show", f"{fixed_source_ref}:{file_name}"):
            raise SystemExit(f"RC7 corrected product-source blob differs from reviewed fix: {file_name}")
    for file_name in sorted(RC7_GOVERNANCE_FILES):
        if git("show", f"HEAD:{file_name}") != git("show", f"{develop_ref}:{file_name}"):
            raise SystemExit(f"RC7 correction governance blob differs from reviewed develop: {file_name}")
    return "rc7-scoped-post-acceptance-reopen"


# #95/#1047: one exact untagged V0.3.5.0 security correction.
# It cannot import later develop product work or repeat for a new base.
V035_NUMERIC_BASE_SHA = "b0680d5df4f76d8b84b02a1b950030a65a89ddca"
V035_NUMERIC_HEAD_REF = "feature/589-release-preparation"
# The independently reviewed #1047 was squash-merged into develop.
# Pin that reachable immutable merge commit; the PR's unmerged HEAD object
# is not guaranteed to exist in an isolated release-PR checkout.
V035_NUMERIC_FIXED_SOURCE_SHA = "73cd3e11dabeff2a114ffa50a48105ef2080fd0f"
V035_NUMERIC_SOURCE_FILES = frozenset({
    "src/runtime/src/detail/watermark_store.cpp",
    "src/runtime/tests/test_watermark_policy.cpp",
})
V035_NUMERIC_GOVERNANCE_FILES = frozenset({
    ".github/scripts/release_correction.py",
    ".github/workflows/git-flow-policy.yml",
})


def validate_v035_numeric_watermark(*, git, version, notes_name,
                                   base_notes, head_notes, base_cmake,
                                   head_cmake, actual_base_sha, head_ref,
                                   fixed_source_ref=V035_NUMERIC_FIXED_SOURCE_SHA,
                                   governance_ref=DEVELOP_REF):
    """Only the independently reviewed #1047 source and reviewed gate itself.

    A protected release PR must carry both this validator and its workflow
    from the separately reviewed develop tree. This is not publication authority.
    """
    if version != "0.3.5.0" or actual_base_sha != V035_NUMERIC_BASE_SHA:
        raise SystemExit("V035 numeric correction denied: unapproved version/base")
    if head_ref != V035_NUMERIC_HEAD_REF:
        raise SystemExit("V035 numeric correction denied: unauthorized preparation branch")
    if not base_notes.startswith(CANDIDATE_MARKER) or head_notes != base_notes:
        raise SystemExit("V035 numeric correction denied: pending bilingual notes must be unchanged")
    if head_cmake != base_cmake:
        raise SystemExit("V035 numeric correction denied: VERSION/CMake must be unchanged")
    # Pending candidate notes cannot override a previously published tag.
    if git("tag", "--list", "v0.3.5.0").strip():
        raise SystemExit("V035 numeric correction denied: version already tagged")
    expected = V035_NUMERIC_SOURCE_FILES | V035_NUMERIC_GOVERNANCE_FILES
    changed = set(git("diff", "--name-only", actual_base_sha, "HEAD").decode("utf-8").splitlines())
    if changed != expected:
        raise SystemExit(
            "V035 numeric correction denied: changed-file scope differs from exact "
            f"security+governance set: {sorted(changed)}")
    # Immutable reviewed PR #1047 HEAD. No cherry-pick recomposition,
    # partial test edits, or later unaccepted develop source allowed.
    for file_name in sorted(V035_NUMERIC_SOURCE_FILES):
        if git("show", f"HEAD:{file_name}") != git("show", f"{fixed_source_ref}:{file_name}"):
            raise SystemExit("V035 numeric correction denied: unreviewed source blob " + file_name)
    # A release candidate must use governance already accepted on develop,
    # not a gate independently weakened in the release-head PR.
    for file_name in sorted(V035_NUMERIC_GOVERNANCE_FILES):
        if git("show", f"HEAD:{file_name}") != git("show", f"{governance_ref}:{file_name}"):
            raise SystemExit("V035 numeric correction denied: unreviewed governance blob " + file_name)
    return "v035-watermark-numeric-exact-security-correction"


# ---------------------------------------------------------------- self test

# #95/#1076: repair one malformed, unpublished V0.3.6.0 candidate note.
# Both gate files must be separately reviewed on develop before release use.
V036_NOTES_BASE_SHA = "75e2d2fff0500fcc6bb301dd0b81ba7f03e280e0"
V036_NOTES_HEAD_REF = "feature/95-release-preparation"
V036_NOTES_OLD_BLOB = "e85f60fcf90098331ac854cc8e9350fa245cf538"
V036_NOTES_NEW_BLOB = "46a560e4de98d9bd3b175bcebc0eb84f55304740"


def validate_v036_notes(*, git, version, notes_name, head_notes, base_cmake,
                        head_cmake, actual_base_sha, head_ref,
                        governance_ref=DEVELOP_REF):
    """Admit only the exact reviewed note repair and its two reviewed gate files."""
    if version != "0.3.6.0" or actual_base_sha != V036_NOTES_BASE_SHA:
        raise SystemExit("V036 notes correction denied: unapproved version/base")
    if head_ref != V036_NOTES_HEAD_REF or notes_name != "docs/releases/v0.3.6.0.md":
        raise SystemExit("V036 notes correction denied: unauthorized branch/notes path")
    if head_cmake != base_cmake:
        raise SystemExit("V036 notes correction denied: VERSION/CMake changed")
    if not head_notes.startswith(CANDIDATE_MARKER):
        raise SystemExit("V036 notes correction denied: missing pending marker")
    if git("ls-remote", "--tags", "origin", "refs/tags/v0.3.6.0").strip():
        raise SystemExit("V036 notes correction denied: version already tagged")
    if (git("rev-parse", f"{actual_base_sha}:{notes_name}").decode("ascii").strip()
            != V036_NOTES_OLD_BLOB
            or git("rev-parse", f"HEAD:{notes_name}").decode("ascii").strip()
            != V036_NOTES_NEW_BLOB):
        raise SystemExit("V036 notes correction denied: unapproved notes blob")
    expected = V035_NUMERIC_GOVERNANCE_FILES | {notes_name}
    changed = set(git("diff", "--name-only", actual_base_sha, "HEAD")
                  .decode("utf-8").splitlines())
    if changed != expected:
        raise SystemExit("V036 notes correction denied: changed-file scope")
    for file_name in sorted(V035_NUMERIC_GOVERNANCE_FILES):
        if git("show", f"HEAD:{file_name}") != git("show", f"{governance_ref}:{file_name}"):
            raise SystemExit("V036 notes correction denied: unreviewed gate blob")
    return "v036-exact-candidate-notes-repair"



# #1086: exact maintenance-line correction after the first G6 failure.
# The maintenance tree is deliberately not the current develop product tree.
V0351_BASE_SHA = "73c0a7ce115d6f530f86ca3316f1d23c524374db"
V0351_HEAD_REF = "feature/1086-release-preparation"
V0351_FIXED_BLOBS = {
    ".github/release/release-trains.json": "b1bcb1d1776bc908e9210184c5b3518a5f7473e9",
    "src/runtime/src/transport/rfb_transport.cpp": "4258f622cc685b95d6ea6bcbc747cf3d8e31b0c4",
    "src/runtime/tests/test_rfb_wire_robustness.cpp": "6d83931919ad43bc62470d64c7258f289966eb2d",
}


def validate_v0351_maintenance(*, git, version, notes_name, base_notes,
                               head_notes, base_cmake, head_cmake,
                               actual_base_sha, head_ref, governance_ref=DEVELOP_REF):
    """Admit only the reviewed H01.8b maintenance blobs and reviewed gate."""
    if version != "0.3.5.1" or actual_base_sha != V0351_BASE_SHA:
        raise SystemExit("V0351 correction denied: unapproved version/base")
    if head_ref != V0351_HEAD_REF or notes_name != "docs/releases/v0.3.5.1.md":
        raise SystemExit("V0351 correction denied: unauthorized branch/notes path")
    if not base_notes.startswith(CANDIDATE_MARKER) or head_notes != base_notes:
        raise SystemExit("V0351 correction denied: pending notes must be unchanged")
    if head_cmake != base_cmake:
        raise SystemExit("V0351 correction denied: VERSION/CMake changed")
    if git("ls-remote", "--tags", "origin", "refs/tags/v0.3.5.1").strip():
        raise SystemExit("V0351 correction denied: version already tagged")
    expected = set(V0351_FIXED_BLOBS) | V035_NUMERIC_GOVERNANCE_FILES
    changed = set(git("diff", "--name-only", actual_base_sha, "HEAD")
                  .decode("utf-8").splitlines())
    if changed != expected:
        raise SystemExit("V0351 correction denied: changed-file scope")
    for path, blob in V0351_FIXED_BLOBS.items():
        if git("rev-parse", f"HEAD:{path}").decode("ascii").strip() != blob:
            raise SystemExit("V0351 correction denied: unreviewed maintenance blob " + path)
    for path in V035_NUMERIC_GOVERNANCE_FILES:
        if git("show", f"HEAD:{path}") != git("show", f"{governance_ref}:{path}"):
            raise SystemExit("V0351 correction denied: unreviewed gate blob " + path)
    return "v0351-exact-maintenance-correction"



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


# #1076: preserve the reviewed V036 S1 tree while carrying the published maintenance history.
V036_LINEAGE_BASE = "1f4db80bd683d8f5aebbf77fa5ecb7391e7e2749"
V036_LINEAGE_PROPOSAL = "15238f4659111cbdc3a51a66ef3ad0a454d08d8d"
V036_LINEAGE_TAG = "3cbc1cdcf6b6d2f9156ab139d7f88ed0225529de"
V036_LINEAGE_BLOBS = {
    ".github/release/release-trains.json": "ff936855886670be555140841e4e54d31cb1d530",
    "docs/releases/v0.3.5.1.md": "ffc00d7b70c0bb657b9b7d022ee82b3fe140c9b8",
}


def validate_v036_lineage(*, git, version, notes_name, base_notes, head_notes,
                          base_cmake, head_cmake, actual_base_sha, head_ref,
                          governance_ref=DEVELOP_REF):
    """Admit only the reviewed maintenance history and independently reviewed gate files."""
    if version != "0.3.6.0" or actual_base_sha != V036_LINEAGE_BASE:
        raise SystemExit("V036 lineage denied: version/base")
    if head_ref != V036_NOTES_HEAD_REF or notes_name != "docs/releases/v0.3.6.0.md":
        raise SystemExit("V036 lineage denied: branch/notes")
    if not base_notes.startswith(CANDIDATE_MARKER) or head_notes != base_notes:
        raise SystemExit("V036 lineage denied: pending notes")
    if base_cmake != head_cmake:
        raise SystemExit("V036 lineage denied: VERSION/CMake")
    if git("ls-remote", "--tags", "origin", "refs/tags/v0.3.6.0").strip():
        raise SystemExit("V036 lineage denied: already tagged")
    peeled = git("ls-remote", "--tags", "origin", "refs/tags/v0.3.5.1^{}").decode().split()
    if peeled != [V036_LINEAGE_TAG, "refs/tags/v0.3.5.1^{}"]:
        raise SystemExit("V036 lineage denied: maintenance tag identity")
    try:
        git("merge-base", "--is-ancestor", V036_LINEAGE_PROPOSAL, "HEAD")
        git("merge-base", "--is-ancestor", V036_LINEAGE_TAG, "HEAD")
    except subprocess.CalledProcessError as exc:
        raise SystemExit("V036 lineage denied: missing reviewed ancestry") from exc
    expected = set(V036_LINEAGE_BLOBS) | V035_NUMERIC_GOVERNANCE_FILES
    changed = set(git("diff", "--name-only", actual_base_sha, "HEAD").decode().splitlines())
    if changed != expected:
        raise SystemExit("V036 lineage denied: changed-file scope")
    for path, blob in V036_LINEAGE_BLOBS.items():
        if git("rev-parse", f"HEAD:{path}").decode().strip() != blob:
            raise SystemExit("V036 lineage denied: unreviewed lineage blob " + path)
    for path in V035_NUMERIC_GOVERNANCE_FILES:
        if git("show", f"HEAD:{path}") != git("show", f"{governance_ref}:{path}"):
            raise SystemExit("V036 lineage denied: unreviewed gate blob " + path)
    return "v036-exact-maintenance-lineage"


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

    # Positive and negative tests for the exact one-off #806 accepted-RC6 reopen. This
    # independently checks SHA lock, unchanged reviewed-develop notes, and source equality.
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        _run(root, "git", "init", "-q")
        _run(root, "git", "config", "core.autocrlf", "false")
        notes_name = "docs/releases/v0.3.5.0.md"
        pending = CANDIDATE_MARKER + "\n# 中文\n\n## Security boundary\n# English\n"
        _write(root, "CMakeLists.txt", 'project(HyRemote VERSION 0.0.0 LANGUAGES CXX)\n')
        _write(root, notes_name, pending)
        _write(root, "src/a.cpp", "int value = 3;\n")
        _commit(root, "reviewed develop candidate")
        _run(root, "git", "update-ref", DEVELOP_REF, "HEAD")
        _run(root, "git", "switch", "-q", "-c", "release")
        _write(root, "CMakeLists.txt", 'project(HyRemote VERSION 0.3.5.0 LANGUAGES CXX)\n')
        _write(root, notes_name, REOPEN_ACCEPTED_MARKER + "\n# 中文\n# English\n")
        _commit(root, "accepted RC6")
        base_notes = _run(root, "git", "show", f"HEAD:{notes_name}").decode("utf-8")
        _run(root, "git", "switch", "-q", "-c", "reopen", DEVELOP_REF)
        _write(root, "CMakeLists.txt", 'project(HyRemote VERSION 0.3.5.0 LANGUAGES CXX)\n')
        _commit(root, "exact new candidate")
        git = _git_in(root)
        head_cmake = git("show", "HEAD:CMakeLists.txt")
        # Compare committed blobs, not Python's in-memory LF strings: on Windows Path.write_text()
        # may create CRLF, which git preserves with core.autocrlf=false. The production gate also
        # compares git blobs, so the self-test must reproduce that exact byte-level contract.
        committed_notes = git("show", f"HEAD:{notes_name}").decode("utf-8")
        candidates = (
            ("exact post-acceptance reopen", REOPEN_BASE_SHA, committed_notes, None),
            ("wrong accepted base SHA blocked", "0" * 40, committed_notes, "not authorized"),
            ("arbitrary notes rewrite blocked", REOPEN_BASE_SHA, committed_notes + "edited\n", "pending develop notes"),
        )
        for name, tested_base, tested_notes, expected_error in candidates:
            cases += 1
            try:
                validate_reopen(git=git, version=REOPEN_VERSION, notes_name=notes_name,
                                base_notes=base_notes, head_notes=tested_notes,
                                head_cmake=head_cmake, actual_base_sha=tested_base)
                if expected_error is not None:
                    print(f"FAIL {name}: expected {expected_error}")
                    failures += 1
                else:
                    print(f"ok: {name}")
            except SystemExit as error:
                if expected_error is None or expected_error not in str(error):
                    print(f"FAIL {name}: {error}")
                    failures += 1
                else:
                    print(f"ok: {name}")

    # #812/#768: exact post-RC7 candidate release guard. These fake Git blobs test
    # byte-level scope and identities without depending on live GitHub state.
    rc7_notes_name = "docs/releases/v0.3.5.0.md"
    rc7_pending = CANDIDATE_MARKER + "\n# 中文\n# English\n## Security boundary\n"
    rc7_accepted = REOPEN_ACCEPTED_MARKER + "\n# 中文\n# English\n"
    rc7_files = RC7_PRODUCT_FILES | RC7_GOVERNANCE_FILES | {rc7_notes_name}
    rc7_cmake = b"project(HyRemote VERSION 0.3.5.0 LANGUAGES CXX)\n"

    def rc7_scenario(name, *, wrong_base=False, wrong_branch=False, missing_file=False,
                     wrong_product=False, wrong_governance=False,
                     wrong_notes=False, wrong_cmake=False, expect_error=None):
        nonlocal failures, cases
        cases += 1
        changed = rc7_files - ({"tools/local-owner/worker.py"} if missing_file else set())

        def git_fixture(*args):
            if args[:2] == ("diff", "--name-only"):
                return ("\n".join(sorted(changed)) + "\n").encode("utf-8")
            if args[0] == "show" and len(args) == 2:
                address = args[1]
                if address == f"{DEVELOP_REF}:{rc7_notes_name}":
                    return rc7_pending.encode("utf-8")
                if ":" not in address:
                    raise AssertionError("unexpected fixture Git show: " + address)
                ref, path = address.split(":", 1)
                if path in RC7_PRODUCT_FILES:
                    if ref == "HEAD" and wrong_product and path == "tools/local-owner/worker.py":
                        return b"BAD-PRODUCT-BLOB\n"
                    return ("REVIEWED-SOURCE:" + path).encode("utf-8")
                if path in RC7_GOVERNANCE_FILES:
                    if ref == "HEAD" and wrong_governance and path == ".github/workflows/git-flow-policy.yml":
                        return b"BAD-GOVERNANCE-BLOB\n"
                    return ("REVIEWED-GOVERNANCE:" + path).encode("utf-8")
            raise AssertionError("unexpected fixture Git query: " + repr(args))

        try:
            validate_reopen_rc7(
                git=git_fixture, version="0.3.5.0", notes_name=rc7_notes_name,
                base_notes=rc7_accepted,
                head_notes=rc7_pending + ("arbitrary edit\n" if wrong_notes else ""),
                base_cmake=rc7_cmake,
                head_cmake=rc7_cmake + (b"tampered\n" if wrong_cmake else b""),
                actual_base_sha=("0" * 40 if wrong_base else RC7_REOPEN_BASE_SHA),
                head_ref=("feature/768-release-preparation" if wrong_branch
                          else "feature/812-release-preparation"),
                fixed_source_ref="reviewed-source")
            if expect_error is not None:
                print(f"FAIL {name}: expected rejection containing '{expect_error}'")
                failures += 1
            else:
                print(f"ok: {name}")
        except SystemExit as error:
            if expect_error is None or expect_error not in str(error):
                print(f"FAIL {name}: {error}")
                failures += 1
            else:
                print(f"ok: {name}")

    rc7_scenario("RC7 exact P2+governance correction accepted")
    rc7_scenario("RC7 unrelated issue preparation branch rejected", wrong_branch=True,
                 expect_error="unauthorized preparation branch")
    rc7_scenario("RC7 different accepted base rejected", wrong_base=True,
                 expect_error="unapproved version/base")
    rc7_scenario("RC7 incomplete source scope rejected", missing_file=True,
                 expect_error="changed-file scope")
    rc7_scenario("RC7 altered worker source rejected", wrong_product=True,
                 expect_error="product-source blob differs")
    rc7_scenario("RC7 unreviewed governance rejected", wrong_governance=True,
                 expect_error="governance blob differs")
    rc7_scenario("RC7 arbitrary pending notes rejected", wrong_notes=True,
                 expect_error="exact pending bilingual notes")
    rc7_scenario("RC7 CMake/version alteration rejected", wrong_cmake=True,
                 expect_error="must not alter the release VERSION")

    # #95/#1047: one-time unpublished b068 candidate security correction.
    # Probe successful exact source + multiple fail-closed adversarial inputs.
    v035_notes = CANDIDATE_MARKER + "\n# 中文\n# English\n## Security boundary\n"
    v035_cmake = b"project(HyRemote VERSION 0.3.5.0 LANGUAGES CXX)\n"
    v035_files = V035_NUMERIC_SOURCE_FILES | V035_NUMERIC_GOVERNANCE_FILES

    def v035_scenario(name, *, wrong_version=False, wrong_base=False,
                      wrong_head=False, wrong_notes=False, wrong_cmake=False,
                      extra_file=False, missing_file=False, bad_source=False,
                      bad_governance=False, tagged=False, expect_error=None):
        nonlocal failures, cases
        cases += 1
        changed = set(v035_files)
        if missing_file:
            changed.remove("src/runtime/tests/test_watermark_policy.cpp")
        if extra_file:
            changed.add("src/runtime/src/transport/rfb_transport.cpp")

        def git_fixture(*args):
            if args == ("tag", "--list", "v0.3.5.0"):
                return b"v0.3.5.0\n" if tagged else b""
            if args[:2] == ("diff", "--name-only"):
                return ("\n".join(sorted(changed)) + "\n").encode("utf-8")
            if args[0] == "show" and len(args) == 2:
                ref, file_name = args[1].split(":", 1)
                if file_name in V035_NUMERIC_SOURCE_FILES:
                    if ref == "HEAD" and bad_source:
                        return b"TAMPERED-SOURCE\n"
                    return ("REVIEWED-1047:" + file_name).encode("utf-8")
                if file_name in V035_NUMERIC_GOVERNANCE_FILES:
                    if ref == "HEAD" and bad_governance:
                        return b"TAMPERED-GOVERNANCE\n"
                    return ("REVIEWED-GOVERNANCE:" + file_name).encode("utf-8")
            raise AssertionError("unexpected Git query: " + repr(args))

        try:
            validate_v035_numeric_watermark(
                git=git_fixture,
                version=("0.3.6.0" if wrong_version else "0.3.5.0"),
                notes_name="docs/releases/v0.3.5.0.md",
                base_notes=v035_notes,
                head_notes=(v035_notes + "changed\n" if wrong_notes else v035_notes),
                base_cmake=v035_cmake,
                head_cmake=(v035_cmake + b"changed" if wrong_cmake else v035_cmake),
                actual_base_sha=("0" * 40 if wrong_base else V035_NUMERIC_BASE_SHA),
                head_ref=("feature/789-release-preparation" if wrong_head
                          else V035_NUMERIC_HEAD_REF),
                fixed_source_ref="reviewed-source",
                governance_ref="reviewed-governance")
            if expect_error is not None:
                print(f"FAIL {name}: expected {expect_error}")
                failures += 1
            else:
                print(f"ok: {name}")
        except SystemExit as error:
            if expect_error is None or expect_error not in str(error):
                print(f"FAIL {name}: {error}")
                failures += 1
            else:
                print(f"ok: {name}")

    v035_scenario("V035 exact #1047 security correction accepted")
    v035_scenario("V035 wrong version denied", wrong_version=True,
                  expect_error="unapproved version/base")
    v035_scenario("V035 wrong frozen candidate denied", wrong_base=True,
                  expect_error="unapproved version/base")
    v035_scenario("V035 unrelated issue branch denied", wrong_head=True,
                  expect_error="unauthorized preparation branch")
    v035_scenario("V035 notes rewrite denied", wrong_notes=True,
                  expect_error="pending bilingual notes")
    v035_scenario("V035 project VERSION change denied", wrong_cmake=True,
                  expect_error="VERSION/CMake")
    v035_scenario("V035 extra RFB product edit denied", extra_file=True,
                  expect_error="changed-file scope")
    v035_scenario("V035 missing regression test denied", missing_file=True,
                  expect_error="changed-file scope")
    v035_scenario("V035 altered source blob denied", bad_source=True,
                  expect_error="unreviewed source blob")
    v035_scenario("V035 altered gate blob denied", bad_governance=True,
                  expect_error="unreviewed governance blob")
    v035_scenario("V035 already-tagged version denied", tagged=True,
                  expect_error="version already tagged")


    # #1076 exact note repair: keep the existing self-test identity.
    v036_path = "docs/releases/v0.3.6.0.md"
    v036_cmake = b"project(HyRemote VERSION 0.3.6.0)\n"

    def v036_scenario(name, *, wrong_base=False, wrong_branch=False,
                      bad_old=False, bad_new=False, extra=False,
                      bad_gate=False, tagged=False, wrong_cmake=False,
                      missing_marker=False, expect_error=None):
        nonlocal cases, failures
        cases += 1
        files = V035_NUMERIC_GOVERNANCE_FILES | {v036_path}
        if extra:
            files = files | {"src/runtime/src/access_instance.cpp"}

        def git_fixture(*args):
            if args == ("ls-remote", "--tags", "origin", "refs/tags/v0.3.6.0"):
                return b"tagged\n" if tagged else b""
            if args == ("rev-parse", f"{V036_NOTES_BASE_SHA}:{v036_path}"):
                return (("0" * 40 if bad_old else V036_NOTES_OLD_BLOB) + "\n").encode("ascii")
            if args == ("rev-parse", f"HEAD:{v036_path}"):
                return (("0" * 40 if bad_new else V036_NOTES_NEW_BLOB) + "\n").encode("ascii")
            if args[:2] == ("diff", "--name-only"):
                return "\n".join(sorted(files)).encode("utf-8")
            if args[0] == "show":
                ref, file_name = args[1].split(":", 1)
                if file_name in V035_NUMERIC_GOVERNANCE_FILES:
                    return b"altered" if ref == "HEAD" and bad_gate else b"reviewed"
            raise AssertionError("unexpected Git query: " + repr(args))

        try:
            validate_v036_notes(
                git=git_fixture, version="0.3.6.0", notes_name=v036_path,
                head_notes="" if missing_marker else CANDIDATE_MARKER,
                base_cmake=v036_cmake,
                head_cmake=v036_cmake + b"changed" if wrong_cmake else v036_cmake,
                actual_base_sha="0" * 40 if wrong_base else V036_NOTES_BASE_SHA,
                head_ref="feature/789-release-preparation" if wrong_branch else V036_NOTES_HEAD_REF)
            if expect_error is not None:
                print(f"FAIL {name}: expected {expect_error}")
                failures += 1
            else:
                print(f"ok: {name}")
        except SystemExit as error:
            if expect_error is None or expect_error not in str(error):
                print(f"FAIL {name}: {error}")
                failures += 1
            else:
                print(f"ok: {name}")

    v036_scenario("V036 exact candidate notes correction accepted")
    v036_scenario("V036 wrong frozen candidate denied", wrong_base=True, expect_error="unapproved version/base")
    v036_scenario("V036 wrong branch denied", wrong_branch=True, expect_error="unauthorized branch")
    v036_scenario("V036 wrong old notes denied", bad_old=True, expect_error="unapproved notes blob")
    v036_scenario("V036 wrong new notes denied", bad_new=True, expect_error="unapproved notes blob")
    v036_scenario("V036 extra product edit denied", extra=True, expect_error="changed-file scope")
    v036_scenario("V036 altered gate denied", bad_gate=True, expect_error="unreviewed gate blob")
    v036_scenario("V036 tagged version denied", tagged=True, expect_error="version already tagged")
    v036_scenario("V036 version change denied", wrong_cmake=True, expect_error="VERSION/CMake")
    v036_scenario("V036 missing candidate marker denied", missing_marker=True, expect_error="missing pending marker")

    # Reuse the existing self-test entry, exercising each exact-correction boundary.
    for fault, expected_error in (
        ("none", None), ("version", "version/base"), ("base", "version/base"),
        ("branch", "branch/notes"), ("notes", "pending notes"),
        ("cmake", "VERSION/CMake"), ("tag", "already tagged"),
        ("extra", "changed-file scope"), ("missing", "changed-file scope"),
        ("blob", "unreviewed maintenance blob"), ("gate", "unreviewed gate blob"),
    ):
        cases += 1
        files = set(V0351_FIXED_BLOBS) | V035_NUMERIC_GOVERNANCE_FILES
        if fault == "extra":
            files.add("src/runtime/src/access_instance.cpp")
        if fault == "missing":
            files.remove("src/runtime/tests/test_rfb_wire_robustness.cpp")

        def maintenance_git(*args):
            if args == ("ls-remote", "--tags", "origin", "refs/tags/v0.3.5.1"):
                return b"tagged" if fault == "tag" else b""
            if args[:2] == ("diff", "--name-only"):
                return "\n".join(sorted(files)).encode()
            if args[0] == "rev-parse" and args[1].startswith("HEAD:"):
                blob = V0351_FIXED_BLOBS[args[1][5:]]
                return ("0" * 40 if fault == "blob" else blob).encode()
            if args[0] == "show":
                ref, path = args[1].split(":", 1)
                if path in V035_NUMERIC_GOVERNANCE_FILES:
                    return b"altered" if ref == "HEAD" and fault == "gate" else b"reviewed"
            raise AssertionError("unexpected Git query: " + repr(args))

        try:
            validate_v0351_maintenance(
                git=maintenance_git, version="0.3.5.0" if fault == "version" else "0.3.5.1",
                notes_name="docs/releases/v0.3.5.1.md", base_notes=CANDIDATE_MARKER,
                head_notes="" if fault == "notes" else CANDIDATE_MARKER,
                base_cmake=b"version", head_cmake=b"changed" if fault == "cmake" else b"version",
                actual_base_sha="0" * 40 if fault == "base" else V0351_BASE_SHA,
                head_ref="feature/95-release-preparation" if fault == "branch" else V0351_HEAD_REF)
            error = None
        except SystemExit as exc:
            error = str(exc)
        if (expected_error is None and error is not None
                or expected_error is not None and (error is None or expected_error not in error)):
            print(f"FAIL V0351 {fault}: {error}")
            failures += 1
        else:
            print(f"ok: V0351 {fault}")


    # Exact V036 lineage: reuse the existing release-correction self-test.
    for fault, expected_error in (
        ("none", None), ("version", "version/base"), ("base", "version/base"),
        ("branch", "branch/notes"), ("notes", "pending notes"),
        ("cmake", "VERSION/CMake"), ("tag", "already tagged"),
        ("old-tag", "maintenance tag identity"), ("ancestry", "missing reviewed ancestry"),
        ("extra", "changed-file scope"), ("missing", "changed-file scope"),
        ("blob", "unreviewed lineage blob"), ("gate", "unreviewed gate blob"),
    ):
        cases += 1
        files = set(V036_LINEAGE_BLOBS) | V035_NUMERIC_GOVERNANCE_FILES
        if fault == "extra":
            files.add("src/runtime/src/access_instance.cpp")
        if fault == "missing":
            files.remove("docs/releases/v0.3.5.1.md")

        def lineage_git(*args):
            if args == ("ls-remote", "--tags", "origin", "refs/tags/v0.3.6.0"):
                return b"tagged" if fault == "tag" else b""
            if args == ("ls-remote", "--tags", "origin", "refs/tags/v0.3.5.1^{}"):
                return (("0" * 40 if fault == "old-tag" else V036_LINEAGE_TAG)
                        + "\trefs/tags/v0.3.5.1^{}\n").encode()
            if args[:2] == ("merge-base", "--is-ancestor"):
                if fault == "ancestry":
                    raise subprocess.CalledProcessError(1, args)
                return b""
            if args[:2] == ("diff", "--name-only"):
                return "\n".join(sorted(files)).encode()
            if args[0] == "rev-parse" and args[1].startswith("HEAD:"):
                return ("0" * 40 if fault == "blob" else V036_LINEAGE_BLOBS[args[1][5:]]).encode()
            if args[0] == "show":
                ref, path = args[1].split(":", 1)
                if path in V035_NUMERIC_GOVERNANCE_FILES:
                    return b"altered" if ref == "HEAD" and fault == "gate" else b"reviewed"
            raise AssertionError("unexpected Git query: " + repr(args))

        try:
            validate_v036_lineage(
                git=lineage_git, version="0.3.5.1" if fault == "version" else "0.3.6.0",
                notes_name="docs/releases/v0.3.6.0.md", base_notes=CANDIDATE_MARKER,
                head_notes="" if fault == "notes" else CANDIDATE_MARKER,
                base_cmake=b"version", head_cmake=b"changed" if fault == "cmake" else b"version",
                actual_base_sha="0" * 40 if fault == "base" else V036_LINEAGE_BASE,
                head_ref="feature/1086-release-preparation" if fault == "branch" else V036_NOTES_HEAD_REF)
            error = None
        except SystemExit as exc:
            error = str(exc)
        if (expected_error is None and error is not None
                or expected_error is not None and (error is None or expected_error not in error)):
            print(f"FAIL V036 lineage {fault}: {error}")
            failures += 1
        else:
            print(f"ok: V036 lineage {fault}")

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
