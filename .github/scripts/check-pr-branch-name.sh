#!/usr/bin/env bash
# Enforce the Git Flow branch families for pull request heads.
#
# Only four families may open a pull request, and only with the shape the policy names:
#
#   feature/<issue>-<topic>    -> develop
#   hotfix/<issue>-<topic>     -> main or develop
#   release/vX.Y.Z.W           -> main
#   backmerge/vX.Y.Z.W         -> develop
#
# Anything else fails. The historical default case used to accept any name when it targeted develop, which is exactly
# how `fix/*`, `refactor/*`, `chore/*` and one-off branch names accumulated. There is no grandfather list: an old
# branch that still needs to land must be replayed onto a compliant branch, because a permanent exception list is the
# same loophole under a different name.
#
# The numeric part of feature/ and hotfix/ is an issue number, and this script only checks the shape. Whether that
# issue exists - and is an issue rather than a pull request - is checked by the workflow, which is the part that can
# call GitHub.
#
# `--self-test` proves the matrix without a workflow runner.
set -uo pipefail

check_name() {
    # check_name <head-ref> <base-ref>; 0 = allowed family and base, 1 = rejected (reason on stdout)
    local head="$1" base="$2" issue=""
    case "${head}" in
        feature/*)
            if [[ "${base}" != "develop" ]]; then
                echo "feature/* PRs must target develop"
                return 1
            fi
            local topic="${head#feature/}"
            issue="${topic%%-*}"
            if [[ ! "${issue}" =~ ^[0-9]+$ ]]; then
                echo "feature branches must be feature/<issue>-<topic> with a numeric issue"
                return 1
            fi
            if [[ "${topic}" == "${issue}" || "${topic#*-}" == "" ]]; then
                echo "feature branches must be feature/<issue>-<topic> with a non-empty topic"
                return 1
            fi
            ;;
        hotfix/*)
            case "${base}" in
                main|develop) ;;
                *) echo "hotfix/* PRs may target only main or develop"; return 1 ;;
            esac
            local hotfix_topic="${head#hotfix/}"
            issue="${hotfix_topic%%-*}"
            if [[ ! "${issue}" =~ ^[0-9]+$ ]]; then
                echo "hotfix branches must be hotfix/<issue>-<topic> with a numeric issue"
                return 1
            fi
            if [[ "${hotfix_topic}" == "${issue}" || "${hotfix_topic#*-}" == "" ]]; then
                echo "hotfix branches must be hotfix/<issue>-<topic> with a non-empty topic"
                return 1
            fi
            ;;
        release/v*)
            if [[ "${base}" != "main" ]]; then
                echo "release/v* PRs must target main"
                return 1
            fi
            if [[ ! "${head#release/v}" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
                echo "release branches must be release/vMajor.Minor.Feature.Maintenance"
                return 1
            fi
            ;;
        backmerge/v*)
            if [[ "${base}" != "develop" ]]; then
                echo "backmerge/v* PRs must target develop"
                return 1
            fi
            if [[ ! "${head#backmerge/v}" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
                echo "backmerge branches must be backmerge/vMajor.Minor.Feature.Maintenance"
                return 1
            fi
            ;;
        *)
            echo "branch '${head}' is not an allowed Git Flow family; use feature/<issue>-<topic>, hotfix/<issue>-<topic>, release/vX.Y.Z.W or backmerge/vX.Y.Z.W"
            return 1
            ;;
    esac
    return 0
}

self_test() {
    local failures=0
    run_case() {
        local description="$1" head="$2" base="$3" expected="$4" status
        check_name "${head}" "${base}" >/dev/null 2>&1
        status=$?
        if [[ "${status}" -ne "${expected}" ]]; then
            echo "CASE FAILED: ${description}: exit ${status}, expected ${expected}" >&2
            failures=$((failures + 1))
        else
            echo "  ok: ${description}"
        fi
    }

    run_case "feature/<issue>-<topic> to develop" feature/237-examples develop 0
    run_case "feature without an issue number" feature/foo develop 1
    run_case "feature without a topic" feature/237 develop 1
    run_case "feature targeting main" feature/237-examples main 1
    run_case "release/vX.Y.Z.W topology" release/v0.1.0.0 main 0
    run_case "backmerge/vX.Y.Z.W topology" backmerge/v0.1.0.0 develop 0
    run_case "hotfix/<issue>-<topic> to main" hotfix/123-topic main 0
    run_case "hotfix without an issue number" hotfix/topic develop 1
    foreach_missing_family() {
        local family="$1"
        check_name "${family}/foo" develop >/dev/null 2>&1
        if [[ $? -ne 1 ]]; then
            echo "CASE FAILED: ${family}/* is not an allowed family" >&2
            failures=$((failures + 1))
        else
            echo "  ok: ${family}/* is rejected"
        fi
    }
    foreach_missing_family fix
    foreach_missing_family refactor
    foreach_missing_family chore
    foreach_missing_family bugfix
    foreach_missing_family test
    foreach_missing_family tmp
    check_name "topic-without-family" develop >/dev/null 2>&1
    if [[ $? -ne 1 ]]; then
        echo "CASE FAILED: an unknown prefix must be rejected" >&2
        failures=$((failures + 1))
    else
        echo "  ok: unknown prefix is rejected"
    fi

    if (( failures > 0 )); then
        echo "check-pr-branch-name self test: ${failures} case(s) failed"
        return 1
    fi
    echo "check-pr-branch-name self test: PASS (four families allowed with their bases; every other family rejected)"
    return 0
}

if [[ "${1:-}" == "--self-test" ]]; then
    self_test
    exit $?
fi

if [[ $# -ne 2 ]]; then
    echo "usage: check-pr-branch-name.sh <head-ref> <base-ref>" >&2
    exit 2
fi

if ! check_name "$1" "$2"; then
    exit 1
fi
echo "branch family accepted: ${1} -> ${2}"
exit 0
