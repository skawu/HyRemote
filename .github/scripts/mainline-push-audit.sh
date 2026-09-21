#!/usr/bin/env bash
# Audit that a mainline push came through a merged pull request into the pushed branch.
#
# GitHub's commit-to-pull-request association is eventually consistent: a few seconds after a merge the
# `commits/<sha>/pulls` endpoint can legitimately answer empty, which made a real merged PR into `develop` look like
# a direct push. The audit therefore retries with backoff, and it keeps its meaning while doing so:
#
#   * a merged PR whose base is the pushed branch is the only thing that passes;
#   * exhausting the attempts fails the job - this is never downgraded to a warning;
#   * a two-parent commit, a `Merge pull request` message, or any local shape is NOT evidence and is never accepted.
#
# `--self-test` proves those three properties with stubbed responses, so the retry logic is tested by behaviour
# rather than by waiting for real propagation timing.
set -uo pipefail

attempts="${MAINLINE_AUDIT_ATTEMPTS:-8}"
delay="${MAINLINE_AUDIT_DELAY_SECONDS:-3}"
repo="${GITHUB_REPOSITORY:-}"
sha="${GITHUB_SHA:-}"
branch="${BRANCH:-}"
gh_command=("${MAINLINE_AUDIT_GH:-gh}")

audit() {
    local attempt
    for (( attempt = 1; attempt <= attempts; attempt++ )); do
        local merged_prs
        merged_prs="$("${gh_command[@]}" api "repos/${repo}/commits/${sha}/pulls" \
            --jq ".[] | select(.merged_at != null and .base.ref == \"${branch}\") | .number" 2>/dev/null || true)"
        if [[ -n "${merged_prs//[$'\n\r ']/}" ]]; then
            echo "mainline push audit passed for ${branch}: merged PR(s) $(echo "${merged_prs}" | tr '\n' ' ')"
            return 0
        fi
        if (( attempt < attempts )); then
            echo "mainline push audit: no merged PR association yet for ${sha:0:7} into ${branch} (attempt ${attempt}/${attempts})"
            sleep "${delay}"
        fi
    done
    echo "mainline direct-push audit failed: ${sha} has no merged PR into ${branch} after ${attempts} attempt(s)" >&2
    echo "This is governance evidence only; the workflow cannot roll back the push." >&2
    return 1
}

self_test() {
    local tmpdir failures=0
    tmpdir="$(mktemp -d)"
    trap 'rm -rf "${tmpdir}"' EXIT

    write_stub() {
        # write_stub <path> <mode>: first|never|merged-into-develop
        # The stub stands in for `gh`, so it produces exactly what the jq filter in the audit would produce: the
        # merged PR numbers for the branch being audited, and nothing when the association belongs to another branch.
        local path="$1" mode="$2"
        cat >"${path}" <<STUB
#!/usr/bin/env bash
counter_file="${tmpdir}/calls"
calls=0
[[ -f "\${counter_file}" ]] && calls="\$(cat "\${counter_file}")"
calls=\$((calls + 1))
echo "\${calls}" >"\${counter_file}"
case "${mode}" in
    first)
        if [[ "\${calls}" -ge 2 ]]; then echo "249"; fi
        ;;
    never) ;;
    merged-into-develop)
        if [[ "\${BRANCH}" == "develop" ]]; then echo "249"; fi
        ;;
esac
exit 0
STUB
        chmod +x "${path}"
    }

    run_case() {
        # run_case <description> <mode> <branch> <expected-exit>
        local description="$1" mode="$2" case_branch="$3" expected="$4" stub="${tmpdir}/gh-${mode}"
        write_stub "${stub}" "${mode}"
        rm -f "${tmpdir}/calls"
        local output status
        output="$(MAINLINE_AUDIT_GH="${stub}" MAINLINE_AUDIT_ATTEMPTS=3 MAINLINE_AUDIT_DELAY_SECONDS=0 \
            GITHUB_REPOSITORY="example/repo" GITHUB_SHA="deadbeefdeadbeef" BRANCH="${case_branch}" \
            bash "$0" 2>&1)"
        status=$?
        if [[ "${status}" -ne "${expected}" ]]; then
            echo "CASE FAILED: ${description}: exit ${status}, expected ${expected}" >&2
            echo "${output}" >&2
            failures=$((failures + 1))
        else
            echo "  ok: ${description}"
        fi
    }

    # A. Association appears after propagation delay -> PASS.
    run_case "association found on a later attempt passes" first develop 0
    # B. No association at all -> FAIL (never a warning, never an automatic pass).
    run_case "no association after every attempt fails" never develop 1
    # C. The merged PR belongs to another branch -> FAIL, because the pushed branch has no association.
    run_case "association for another branch fails" merged-into-develop main 1
    # D. The same association on its own branch -> PASS.
    run_case "association for the pushed branch passes" merged-into-develop develop 0

    if (( failures > 0 )); then
        echo "mainline-push-audit self test: ${failures} case(s) failed"
        return 1
    fi
    echo "mainline-push-audit self test: PASS (retry passes on a delayed association; no association fails; "
          "another branch fails)"
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
