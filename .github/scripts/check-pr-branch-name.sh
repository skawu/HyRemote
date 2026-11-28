#!/usr/bin/env bash
# Ordinary semantic feature/hotfix branches link an Issue in PR metadata.
# Keep existing numbered branches compatible and protected release topology exact.
set -uo pipefail

check_topic() {
    local topic="$1"
    [[ "$topic" =~ ^[0-9]+-.+$ || "$topic" =~ ^[a-z][a-z0-9]*(-[a-z0-9]+)*$ ]]
}

resolve_issue() {
    local head="$1" body="$2" topic line number="" count=0
    case "$head" in feature/*|hotfix/*) ;; *) return 1 ;; esac
    topic="${head#*/}"
    if [[ "$topic" =~ ^([0-9]+)-.+$ ]]; then
        printf '%s\n' "${BASH_REMATCH[1]}"
        return 0
    fi
    check_topic "$topic" || return 1
    local issue_line='^Issue:[[:blank:]]*#([1-9][0-9]*)[[:blank:]]*$'
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line%$'\r'}"
        if [[ "$line" =~ ^Issue: ]]; then
            [[ "$line" =~ $issue_line ]] || return 1
            number="${BASH_REMATCH[1]}"
            ((count += 1))
        fi
    done <<< "$body"
    [[ "$count" -eq 1 ]] || return 1
    printf '%s\n' "$number"
}

check_name() {
    local head="$1" base="$2"
    case "$head" in
        feature/*)
            if [[ "$base" != develop ]]; then
                # Keep the exact existing protected release-preparation exception.
                [[ "$base" =~ ^release/v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ &&
                   "$head" =~ ^feature/[0-9]+-release-preparation$ ]] || {
                    echo 'feature/* targets develop; only feature/<issue>-release-preparation may target an exact release/vX.Y.Z.W'
                    return 1
                }
            fi
            check_topic "${head#feature/}" || { echo 'feature branch requires a lowercase semantic topic or legacy numbered topic'; return 1; }
            ;;
        hotfix/*)
            case "$base" in main|develop) ;; *) echo 'hotfix/* PRs may target only main or develop'; return 1 ;; esac
            check_topic "${head#hotfix/}" || { echo 'hotfix branch requires a lowercase semantic topic or legacy numbered topic'; return 1; }
            ;;
        release/v*)
            [[ "$base" == main && "${head#release/v}" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
                echo 'release/vMajor.Minor.Feature.Maintenance must target main'
                return 1
            }
            ;;
        backmerge/v*)
            [[ "$base" == develop && "${head#backmerge/v}" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
                echo 'backmerge/vMajor.Minor.Feature.Maintenance must target develop'
                return 1
            }
            ;;
        *) echo "branch '$head' is not an allowed Git Flow family"; return 1 ;;
    esac
    return 0
}

self_test() {
    local failures=0 family
    run_case() {
        local description="$1" head="$2" base="$3" expected="$4" status
        check_name "$head" "$base" >/dev/null 2>&1
        status=$?
        if [[ "$status" -ne "$expected" ]]; then
            echo "CASE FAILED: $description: exit $status, expected $expected" >&2
            failures=$((failures + 1))
        else
            echo "  ok: $description"
        fi
    }
    run_case 'legacy feature' feature/237-examples develop 0
    run_case 'semantic feature' feature/qt-quick-capture develop 0
    run_case 'uppercase rejected' feature/QtQuick develop 1
    run_case 'feature without a topic' feature/237 develop 1
    run_case 'feature targeting main' feature/237-examples main 1
    run_case 'exact protected preparation' feature/466-release-preparation release/v0.3.0.0 0
    run_case 'ordinary feature into release denied' feature/466-unrelated-feature release/v0.3.0.0 1
    run_case 'semantic feature into release denied' feature/qt-quick-capture release/v0.3.0.0 1
    run_case 'malformed release target denied' feature/466-release-preparation release/v0.3 1
    run_case 'preparation into main denied' feature/466-release-preparation main 1
    run_case 'release topology' release/v0.1.0.0 main 0
    run_case 'release wrong base' release/v0.1.0.0 develop 1
    run_case 'backmerge topology' backmerge/v0.1.0.0 develop 0
    run_case 'backmerge wrong base' backmerge/v0.1.0.0 main 1
    run_case 'legacy hotfix' hotfix/123-topic main 0
    run_case 'semantic hotfix' hotfix/rfb-disconnect develop 0
    run_case 'hotfix into release denied' hotfix/rfb-disconnect release/v0.3.6.0 1
    for family in fix refactor chore bugfix test tmp; do
        run_case "$family rejected" "$family/foo" develop 1
    done
    run_case 'unknown family rejected' topic-without-family develop 1
    if [[ "$(resolve_issue feature/qt-quick-capture 'Issue: #609')" != 609 ]] ||
       [[ "$(resolve_issue feature/466-release-preparation '')" != 466 ]] ||
       [[ "$(resolve_issue hotfix/rfb-disconnect $'Issue: #567\r\nSummary')" != 567 ]] ||
       resolve_issue feature/qt-quick-capture 'Refs #609' >/dev/null ||
       resolve_issue feature/qt-quick-capture $'Issue: #609\nIssue: #95' >/dev/null ||
       resolve_issue feature/qt-quick-capture 'Issue: other/repo#609' >/dev/null; then
        echo 'CASE FAILED: Issue metadata parsing' >&2
        failures=$((failures + 1))
    fi
    if (( failures > 0 )); then
        echo "check-pr-branch-name self test: $failures case(s) failed"
        return 1
    fi
    echo 'check-pr-branch-name self test: PASS'
    return 0
}

if [[ "${1:-}" == --self-test ]]; then
    self_test
    exit $?
fi
if [[ "${1:-}" == --issue ]]; then
    [[ $# -eq 3 ]] || exit 2
    resolve_issue "$2" "$3"
    exit $?
fi
if [[ $# -ne 2 ]]; then
    echo 'usage: check-pr-branch-name.sh <head-ref> <base-ref>' >&2
    exit 2
fi
check_name "$1" "$2" || exit 1
echo "branch family accepted: $1 -> $2"
