#!/usr/bin/env bash
# Audit that a mainline push came through a merged pull request into the pushed branch.
#
# GitHub's commit-to-pull-request association is eventually consistent: a few seconds after a merge the
# `commits/<sha>/pulls` endpoint can legitimately answer empty, which made a real merged PR into `develop` look like a
# direct push. The audit therefore retries with backoff, and it keeps its meaning while doing so:
#
#   * a merged PR whose base is the pushed branch is the only thing that passes;
#   * exhausting the attempts fails the job - this is never downgraded to a warning;
#   * a two-parent commit, a `Merge pull request` message, or any local shape is NOT evidence and is never accepted.
#
# The association query is a single hook (`MAINLINE_AUDIT_QUERY`), so `--self-test` can prove the retry behaviour with
# a stub instead of waiting for real propagation timing.
set -uo pipefail

attempts="${MAINLINE_AUDIT_ATTEMPTS:-12}"
delay="${MAINLINE_AUDIT_DELAY_SECONDS:-5}"
repo="${GITHUB_REPOSITORY:-}"
sha="${GITHUB_SHA:-}"
branch="${BRANCH:-}"

query_pr_association() {
    # Only bare pull-request numbers count as an association. A failed request can still print an error body on
    # stdout, and treating that as evidence would make the audit pass on its own failures - the one outcome an audit
    # of this kind must never produce.
    if [[ -n "${MAINLINE_AUDIT_QUERY:-}" ]]; then
        "${MAINLINE_AUDIT_QUERY}" "${branch}" "${sha}" "${repo}" | grep -E '^[0-9]+$' || true
        return 0
    fi
    gh api "repos/${repo}/commits/${sha}/pulls" \
        --jq ".[] | select(.merged_at != null and .base.ref == \"${branch}\") | .number" 2>/dev/null \
        | grep -E '^[0-9]+$' || true
}

audit() {
    local attempt attempt_output
    for (( attempt = 1; attempt <= attempts; attempt++ )); do
        attempt_output="$(query_pr_association)"
        if [[ -n "${attempt_output//[$'\n\r\t ']/}" ]]; then
            echo "mainline push audit passed for ${branch}: merged PR(s) $(echo "${attempt_output}" | tr '\n' ' ')"
            return 0
        fi
        if (( attempt < attempts )); then
            echo "mainline push audit: no merged PR association yet for ${sha} into ${branch} (attempt ${attempt}/${attempts})"
            sleep "${delay}"
        fi
    done
    echo "mainline direct-push audit failed: ${sha} has no merged PR into ${branch} after ${attempts} attempt(s)" >&2
    echo "This is governance evidence only; the workflow cannot roll back the push." >&2
    return 1
}

expect_exit() {
    # expect_exit <description> <expected-status> <query-hook> <branch>
    # The cases set the variables the audit actually reads, not only the environment names it initialises from, so a
    # case cannot pass by accident when the resolved value differs from the raw one.
    local description="$1" expected="$2" hook="$3" case_branch="$4" status
    MAINLINE_AUDIT_QUERY="${hook}"
    MAINLINE_AUDIT_ATTEMPTS=3
    MAINLINE_AUDIT_DELAY_SECONDS=0
    attempts=3
    delay=0
    repo="example/repo"
    sha="deadbeefdeadbeef"
    branch="${case_branch}"
    audit >/dev/null 2>&1
    status=$?
    if [[ "${status}" -ne "${expected}" ]]; then
        echo "CASE FAILED: ${description}: exit ${status}, expected ${expected}" >&2
        return 1
    fi
    echo "  ok: ${description}"
    return 0
}

self_test() {
    local failures=0
    tmpdir="$(mktemp -d)"
    trap 'rm -rf "${tmpdir}"' EXIT

    local delayed="${tmpdir}/query-delayed.sh"
    printf '%s\n' \
        '#!/usr/bin/env bash' \
        'counter="$(dirname "$0")/calls"' \
        'calls=0' \
        '[ -f "${counter}" ] && calls="$(cat "${counter}")"' \
        'calls=$((calls + 1))' \
        'echo "${calls}" >"${counter}"' \
        '[ "${calls}" -ge 2 ] && echo 249' \
        'exit 0' >"${delayed}"
    chmod +x "${delayed}"

    local silent="${tmpdir}/query-silent.sh"
    printf '%s\n' \
        '#!/usr/bin/env bash' \
        'exit 0' >"${silent}"
    chmod +x "${silent}"

    local other_branch="${tmpdir}/query-other-branch.sh"
    printf '%s\n' \
        '#!/usr/bin/env bash' \
        '[ "${1}" = "develop" ] && echo 249' \
        'exit 0' >"${other_branch}"
    chmod +x "${other_branch}"

    # A. The association appears after the propagation delay -> PASS.
    rm -f "${tmpdir}/calls"
    expect_exit "association found on a later attempt passes" 0 "${delayed}" develop || failures=$((failures + 1))
    # B. No association at all -> FAIL (never a warning, never an automatic pass).
    expect_exit "no association after every attempt fails" 1 "${silent}" develop || failures=$((failures + 1))
    # C. The association belongs to another branch -> FAIL for the pushed branch.
    expect_exit "association for another branch fails" 1 "${other_branch}" main || failures=$((failures + 1))
    # D. The same association on its own branch -> PASS.
    expect_exit "association for the pushed branch passes" 0 "${other_branch}" develop || failures=$((failures + 1))

    if (( failures > 0 )); then
        echo "mainline-push-audit self test: ${failures} case(s) failed"
        return 1
    fi
    echo "mainline-push-audit self test: PASS (retry passes on a delayed association; no association fails; another branch fails)"
    return 0
}

if [[ "${1:-}" == "--self-test" ]]; then
    self_test
    exit $?
fi

if [[ -z "${repo}" || -z "${sha}" || -z "${branch}" ]]; then
    echo "GITHUB_REPOSITORY, GITHUB_SHA and BRANCH are required" >&2
    exit 2
fi

audit
exit $?
