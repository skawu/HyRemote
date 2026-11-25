#!/usr/bin/env bash
# Enforce the Git Flow branch families for pull request heads.
#
# Ordinary feature/<topic> -> develop; hotfix/<topic> -> main or develop.
# Semantic topics use lowercase kebab-case and a separate PR Issue: #N field.
# Existing numbered branches remain readable during migration; do not create
# new numbered ordinary branches. Protected feature/<issue>-release-preparation,
# release/vX.Y.Z.W and backmerge/vX.Y.Z.W keep their exact existing topology.
# --issue extracts the same-repository Issue number; the workflow validates
# its existence and non-PR identity. --self-test exercises both local parsers.
set -uo pipefail

check_topic() {
    local topic="$1"
    # Compatibility for an already existing numbered branch. New ordinary
    # branch names follow the semantic form documented in AGENTS.md.
    [[ "$topic" =~ ^[0-9]+-.+$ || "$topic" =~ ^[a-z][a-z0-9]*(-[a-z0-9]+)*$ ]]
}

resolve_issue() {
    local head="$1" body="$2" topic line number="" count=0
    case "$head" in feature/*|hotfix/*) ;; *) return 1 ;; esac
    topic="${head#*/}"
    if [[ "$topic" =~ ^([0-9]+)-.+$ ]]; then
        printf "%s\n" "${BASH_REMATCH[1]}"
        return 0
    fi
    check_topic "$topic" || return 1
    local issue_line="^Issue:[[:blank:]]*#([1-9][0-9]*)[[:blank:]]*$"
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line%
    # check_name <head-ref> <base-ref>; 0 = allowed family and base, 1 = rejected (reason on stdout)
    local head="$1" base="$2"
    case "${head}" in
        feature/*)
            if [[ "${base}" != "develop" ]]; then
                # Release branches are protected against direct writes. Admit
                # one auditable PR path exclusively for their exact-version
                # preparation; the workflow also enforces a strict file scope.
                if [[ "${base}" =~ ^release/v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ &&
                      "${head}" =~ ^feature/[0-9]+-release-preparation$ ]]; then
                    :
                else
                    echo "feature/* targets develop; only feature/<issue>-release-preparation may target an exact release/vX.Y.Z.W"
                    return 1
                fi
            fi
            check_topic "${head#feature/}" || {
                echo "feature branch requires a semantic lowercase topic"
                return 1
            }
            ;;
        hotfix/*)
            case "${base}" in
                main|develop) ;;
                *) echo "hotfix/* PRs may target only main or develop"; return 1 ;;
            esac
            check_topic "${head#hotfix/}" || {
                echo "hotfix branch requires a semantic lowercase topic"
                return 1
            }
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
            echo "branch '${head}' is not an allowed Git Flow family; use feature/<topic>, hotfix/<topic>, release/vX.Y.Z.W or backmerge/vX.Y.Z.W"
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
    run_case "semantic feature" feature/qt-quick-capture develop 0
    run_case "invalid semantic uppercase" feature/QtQuick develop 1
    run_case "semantic feature cannot target release" feature/qt-quick-capture release/v0.3.6.0 1
    run_case "feature without a topic" feature/237 develop 1
    run_case "feature targeting main" feature/237-examples main 1
    run_case "release preparation PR to exact protected release branch" feature/466-release-preparation release/v0.3.0.0 0
    run_case "ordinary feature PR into protected release denied" feature/466-unrelated-feature release/v0.3.0.0 1
    run_case "release preparation PR to malformed release branch denied" feature/466-release-preparation release/v0.3 1
    run_case "release preparation PR to main denied" feature/466-release-preparation main 1
    run_case "release/vX.Y.Z.W topology" release/v0.1.0.0 main 0
    run_case "backmerge/vX.Y.Z.W topology" backmerge/v0.1.0.0 develop 0
    run_case "hotfix/<issue>-<topic> to main" hotfix/123-topic main 0
    run_case "semantic hotfix" hotfix/rfb-disconnect develop 0
    run_case "semantic hotfix cannot target release" hotfix/rfb-disconnect release/v0.3.6.0 1
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

    # Existing self-test also covers the metadata parser; no separate test runner.
    if [[ "$(resolve_issue feature/qt-quick-capture "Issue: #609")" != 609 ]] ||
       [[ "$(resolve_issue feature/466-release-preparation "")" != 466 ]] ||
       [[ "$(resolve_issue hotfix/rfb-disconnect 
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

if [[ "${1:-}" == "--issue" ]]; then
    [[ $# -eq 3 ]] || exit 2
    resolve_issue "$2" "$3"
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
\r'}"
        if [[ "$line" =~ ^Issue: ]]; then
            [[ "$line" =~ $issue_line ]] || return 1
            number="${BASH_REMATCH[1]}"
            ((count += 1))
        fi
    done <<< "$body"
    [[ "$count" -eq 1 ]] || return 1
    printf "%s\n" "$number"
}

check_name() {
    # check_name <head-ref> <base-ref>; 0 = allowed family and base, 1 = rejected (reason on stdout)
    local head="$1" base="$2" issue=""
    case "${head}" in
        feature/*)
            if [[ "${base}" != "develop" ]]; then
                # Release branches are protected against direct writes. Admit
                # one auditable PR path exclusively for their exact-version
                # preparation; the workflow also enforces a strict file scope.
                if [[ "${base}" =~ ^release/v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ &&
                      "${head}" =~ ^feature/[0-9]+-release-preparation$ ]]; then
                    :
                else
                    echo "feature/* targets develop; only feature/<issue>-release-preparation may target an exact release/vX.Y.Z.W"
                    return 1
                fi
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
    run_case "release preparation PR to exact protected release branch" feature/466-release-preparation release/v0.3.0.0 0
    run_case "ordinary feature PR into protected release denied" feature/466-unrelated-feature release/v0.3.0.0 1
    run_case "release preparation PR to malformed release branch denied" feature/466-release-preparation release/v0.3 1
    run_case "release preparation PR to main denied" feature/466-release-preparation main 1
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
Issue: #567\r\nSummary')" != 567 ]] ||
       resolve_issue feature/qt-quick-capture "Refs #609" >/dev/null ||
       resolve_issue feature/qt-quick-capture 
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
\r'}"
        if [[ "$line" =~ ^Issue: ]]; then
            [[ "$line" =~ $issue_line ]] || return 1
            number="${BASH_REMATCH[1]}"
            ((count += 1))
        fi
    done <<< "$body"
    [[ "$count" -eq 1 ]] || return 1
    printf "%s\n" "$number"
}

check_name() {
    # check_name <head-ref> <base-ref>; 0 = allowed family and base, 1 = rejected (reason on stdout)
    local head="$1" base="$2" issue=""
    case "${head}" in
        feature/*)
            if [[ "${base}" != "develop" ]]; then
                # Release branches are protected against direct writes. Admit
                # one auditable PR path exclusively for their exact-version
                # preparation; the workflow also enforces a strict file scope.
                if [[ "${base}" =~ ^release/v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ &&
                      "${head}" =~ ^feature/[0-9]+-release-preparation$ ]]; then
                    :
                else
                    echo "feature/* targets develop; only feature/<issue>-release-preparation may target an exact release/vX.Y.Z.W"
                    return 1
                fi
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
    run_case "release preparation PR to exact protected release branch" feature/466-release-preparation release/v0.3.0.0 0
    run_case "ordinary feature PR into protected release denied" feature/466-unrelated-feature release/v0.3.0.0 1
    run_case "release preparation PR to malformed release branch denied" feature/466-release-preparation release/v0.3 1
    run_case "release preparation PR to main denied" feature/466-release-preparation main 1
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
Issue: #609\nIssue: #95' >/dev/null ||
       resolve_issue feature/qt-quick-capture "Issue: other/repo#609" >/dev/null; then
        echo "CASE FAILED: Issue metadata parsing" >&2
        failures=$((failures + 1))
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
\r'}"
        if [[ "$line" =~ ^Issue: ]]; then
            [[ "$line" =~ $issue_line ]] || return 1
            number="${BASH_REMATCH[1]}"
            ((count += 1))
        fi
    done <<< "$body"
    [[ "$count" -eq 1 ]] || return 1
    printf "%s\n" "$number"
}

check_name() {
    # check_name <head-ref> <base-ref>; 0 = allowed family and base, 1 = rejected (reason on stdout)
    local head="$1" base="$2" issue=""
    case "${head}" in
        feature/*)
            if [[ "${base}" != "develop" ]]; then
                # Release branches are protected against direct writes. Admit
                # one auditable PR path exclusively for their exact-version
                # preparation; the workflow also enforces a strict file scope.
                if [[ "${base}" =~ ^release/v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ &&
                      "${head}" =~ ^feature/[0-9]+-release-preparation$ ]]; then
                    :
                else
                    echo "feature/* targets develop; only feature/<issue>-release-preparation may target an exact release/vX.Y.Z.W"
                    return 1
                fi
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
    run_case "release preparation PR to exact protected release branch" feature/466-release-preparation release/v0.3.0.0 0
    run_case "ordinary feature PR into protected release denied" feature/466-unrelated-feature release/v0.3.0.0 1
    run_case "release preparation PR to malformed release branch denied" feature/466-release-preparation release/v0.3 1
    run_case "release preparation PR to main denied" feature/466-release-preparation main 1
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
