# E02 (#602) guard: every inherited V1 machine mandatory child must stay accounted for.
#
# The 26-child list is legacy. Reclassifying it is a governance decision, but "accounted for" is mechanical:
# every number the machine still gates must appear exactly once in the reconciliation ledger with one fixed
# label, the labels must be the #602 vocabulary, and the proposed closeable set must not silently drop #57 or
# silently absorb a post-GA capability. This does not change the manifest and is not release authority.
#
# Usage:
#   cmake -DHYREMOTE_SOURCE_DIR=<repo> -P tests/release-readiness/check_ga_scope_reconciliation.cmake

cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(_authority_path "${HYREMOTE_SOURCE_DIR}/.github/release/release-trains.json")
set(_ledger_path
    "${HYREMOTE_SOURCE_DIR}/docs/internal/v1-ga-scope-reconciliation-2026-10-28.md")

foreach(_path IN ITEMS "${_authority_path}" "${_ledger_path}")
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR "E02 evidence is missing: ${_path}")
    endif()
endforeach()

file(READ "${_authority_path}" _authority_json)
file(READ "${_ledger_path}" _ledger_text)

string(JSON _count LENGTH "${_authority_json}" trains "1.0.0.0" mandatory_children)
math(EXPR _last "${_count} - 1")
set(_manifest_numbers)
foreach(_index RANGE 0 ${_last})
    string(JSON _number GET "${_authority_json}" trains "1.0.0.0" mandatory_children ${_index})
    list(APPEND _manifest_numbers "${_number}")
endforeach()

# Parse the ledger rows: "| #<issue> | title | state | <LABEL> | basis |".
string(REPLACE "\n" ";" _lines "${_ledger_text}")
set(_ledger_numbers)
set(_keep_closeable)
foreach(_line IN LISTS _lines)
    string(REGEX MATCH "^\\| #([0-9]+) \\|" _row_match "${_line}")
    if(_row_match STREQUAL "")
        continue()
    endif()
    string(REGEX REPLACE "^\\| #([0-9]+) \\|.*$" "\\1" _number "${_line}")
    string(REGEX REPLACE "^\\|" "" _stripped "${_line}")
    string(REPLACE "|" ";" _cells "${_stripped}")
    list(LENGTH _cells _cell_count)
    if(_cell_count LESS 4)
        message(FATAL_ERROR
            "${_ledger_path}: the row for #${_number} is malformed; the ledger needs "
            "Issue | title | state | classification | basis.")
    endif()
    list(GET _cells 3 _label)
    string(STRIP "${_label}" _label)
    list(APPEND _ledger_numbers "${_number}")
    set("_label_${_number}" "${_label}")
    if(_label STREQUAL "KEEP_CLOSEABLE")
        list(APPEND _keep_closeable "${_number}")
    endif()
endforeach()

if(NOT _ledger_numbers)
    message(FATAL_ERROR "${_ledger_path} contains no reconciliation rows")
endif()

list(LENGTH _ledger_numbers _ledger_count)
if(NOT _ledger_count EQUAL 26)
    message(FATAL_ERROR
        "the ledger must classify the inherited 26 child list exactly once each; found ${_ledger_count} rows. "
        "Dropping a row would make an old machine gate disappear from the audit.")
endif()

list(REMOVE_DUPLICATES _ledger_numbers)
list(LENGTH _ledger_numbers _unique_count)
if(NOT _unique_count EQUAL 26)
    message(FATAL_ERROR "the ledger classifies at least one issue more than once")
endif()

set(_labels
    KEEP_CLOSEABLE
    HISTORICAL_COMPLETE
    REPLACE
    SPLIT
    DEFER_NONBLOCKING
    OWNER_RISK_ACCEPTED)

foreach(_number IN LISTS _ledger_numbers)
    set(_label "${_label_${_number}}")
    list(FIND _labels "${_label}" _label_index)
    if(_label_index EQUAL -1)
        message(FATAL_ERROR
            "#${_number} carries '${_label}', which is not one of the six #602 classifications. "
            "Confusing these categories is how a gate becomes 'planned' forever.")
    endif()
endforeach()

# Every number the machine still gates must be in the ledger, so a later manifest edit cannot quietly add a
# requirement that no reconciliation covers.
foreach(_number IN LISTS _manifest_numbers)
    if(NOT DEFINED _label_${_number})
        message(FATAL_ERROR
            "release-trains.json still gates #${_number} for 1.0.0.0 but the ledger does not classify it. "
            "Every inherited mandatory child must stay accounted for.")
    endif()
endforeach()

# Fixed decisions this ledger claims. These are the substance, not formatting.
foreach(_expectation
        "57|KEEP_CLOSEABLE"
        "33|KEEP_CLOSEABLE"
        "165|KEEP_CLOSEABLE"
        "158|KEEP_CLOSEABLE"
        "9|KEEP_CLOSEABLE"
        "157|REPLACE"
        "170|DEFER_NONBLOCKING"
        "41|DEFER_NONBLOCKING"
        "176|SPLIT"
        "209|SPLIT")
    string(REPLACE "|" ";" _parts "${_expectation}")
    list(GET _parts 0 _number)
    list(GET _parts 1 _expected)
    if(NOT "${_label_${_number}}" STREQUAL "${_expected}")
        message(FATAL_ERROR
            "#${_number} must stay ${_expected} in the E02 ledger; found '${_label_${_number}}'. "
            "#57 and the GA authorities are the Windows contract, and post-GA capabilities (#170) or "
            "umbrellas (#176/#209) cannot become or remain GA blockers.")
    endif()
endforeach()

# The proposed closeable set is exactly what the ledger marks KEEP_CLOSEABLE.
set(_expected_keep 9 30 31 32 33 57 101 107 109 158 165)
list(SORT _keep_closeable)
list(SORT _expected_keep)
if(NOT "${_keep_closeable}" STREQUAL "${_expected_keep}")
    message(FATAL_ERROR
        "the proposed 1.0.0.0 closeable set changed.\n"
        "  ledger:   ${_keep_closeable}\n"
        "  expected: ${_expected_keep}\n"
        "Changing the GA contract needs a new Owner decision and the machine PR, not a ledger edit.")
endif()

# A reconciliation that silently turns GA ready would be a false claim.
foreach(_required_phrase "NOT_READY" "独立 PR" "不是第二份机器权威")
    string(FIND "${_ledger_text}" "${_required_phrase}" _phrase_pos)
    if(_phrase_pos EQUAL -1)
        message(FATAL_ERROR
            "the ledger must keep stating '${_required_phrase}': this document records a proposal, "
            "not a released GA and not a second machine authority.")
    endif()
endforeach()

message(STATUS
    "GA scope reconciliation: 26 inherited children accounted for; closeable set "
    "${_keep_closeable}; manifest reconciled by its own PR")
