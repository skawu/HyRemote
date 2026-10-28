# A01/E01 guard: the V0.3.4.0 exact-cell matrix must stay exact and must not overclaim.
#
# This is not release authority and does not create a second product matrix. It only checks that the two
# user-facing documents and the internal route matrix keep saying what the Owner decisions they cite say
# (D2 route truth, D7 maintained viewer, D1 deferred popup visual), and that no cell silently upgrades a
# rejected or experimental route to Supported. Raising a claim needs a new Owner decision, not a table edit.
#
# Usage:
#   cmake -DHYREMOTE_SOURCE_DIR=<repo> -P tests/release-readiness/check_compatibility_cells.cmake

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(_zh_doc "${HYREMOTE_SOURCE_DIR}/docs/compatibility.md")
set(_en_doc "${HYREMOTE_SOURCE_DIR}/docs/en/compatibility.md")
set(_matrix_doc "${HYREMOTE_SOURCE_DIR}/docs/internal/windows-qt-lts-route-matrix-2026-10-24.md")

foreach(_path IN ITEMS "${_zh_doc}" "${_en_doc}" "${_matrix_doc}")
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR "A01/E01 evidence document is missing: ${_path}")
    endif()
endforeach()

file(READ "${_zh_doc}" _zh_text)
file(READ "${_en_doc}" _en_text)
file(READ "${_matrix_doc}" _matrix_text)

# The published V0.3.0.0 reference matrix is history: it must stay present and must not be rewritten.
foreach(_history_marker
        "V0.3.0.0 已发布时的历史参考矩阵"
        "Historical released V0.3.0.0 reference matrix")
    string(FIND "${_zh_text}${_en_text}" "${_history_marker}" _history_pos)
    if(_history_pos EQUAL -1)
        message(FATAL_ERROR
            "the published V0.3.0.0 reference matrix marker is missing ('${_history_marker}'); "
            "published history must not be deleted or rewritten by a current-line table update.")
    endif()
endforeach()

# Extract the current development-line section: from its heading to the next level-2 heading.
function(_extract_section text marker out_var)
    string(FIND "${text}" "${marker}" _start)
    if(_start EQUAL -1)
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()
    string(LENGTH "${marker}" _marker_length)
    math(EXPR _body_start "${_start} + ${_marker_length}")
    string(SUBSTRING "${text}" ${_body_start} -1 _tail)
    string(FIND "${_tail}" "\n## " _end)
    if(_end EQUAL -1)
        set(${out_var} "${_tail}" PARENT_SCOPE)
    else()
        string(SUBSTRING "${_tail}" 0 ${_end} _body)
        set(${out_var} "${_body}" PARENT_SCOPE)
    endif()
endfunction()

_extract_section("${_zh_text}" "## 当前开发线" _zh_section)
_extract_section("${_en_text}" "## Current development line" _en_section)

if(_zh_section STREQUAL "")
    message(FATAL_ERROR "docs/compatibility.md has no current development-line section")
endif()
if(_en_section STREQUAL "")
    message(FATAL_ERROR "docs/en/compatibility.md has no current development-line section")
endif()

# One row must carry the complete cell identity: exact Qt patch + compiler/ABI + OS/arch + integration + UI target
# + status. Any dimension left out is a dimension a later edit can silently change.
set(_required_rows
    "6.8.3|msvc2022_64|Windows x86_64|C++ API|Widgets|**Supported**"
    "6.8.3|msvc2022_64|Windows x86_64|C++ API|Qt Quick|**Supported**"
    "6.8.3|msvc2022_64|Windows x86_64|QML API|Qt Quick|**Supported**"
    "6.8.3|msvc2022_64|Windows x86_64|Generic Plugin|Widgets / Quick|**Supported**"
    "6.8.3 exact|msvc2022_64|Windows x86_64|QPA|Widgets / Quick|**Limited**"
    "5.15.2|msvc2019_64|Windows x86_64|C++ API|Widgets|**Limited**"
    "5.15.2|msvc2019_64|Windows x86_64|C++ API|Qt Quick|**Experimental**"
    "5.15.2|msvc2019_64|Windows x86_64|QML API|**Unsupported**"
    "5.15.2|msvc2019_64|Windows x86_64|Generic Plugin|**Unsupported**"
    "5.15.2|msvc2019_64|Windows x86_64|QPA|**Not applicable**"
    "6.5 / 6.12|**Detected, not qualified**"
    "User-validation pending")

function(_require_row section document tokens)
    string(REPLACE "|" ";" _tokens "${tokens}")
    string(REPLACE "\n" ";" _lines "${section}")
    foreach(_line IN LISTS _lines)
        set(_missing "")
        foreach(_token IN LISTS _tokens)
            string(FIND "${_line}" "${_token}" _pos)
            if(_pos EQUAL -1)
                list(APPEND _missing "${_token}")
            endif()
        endforeach()
        if(NOT _missing)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR
        "${document}: no current-development-line row carries '${tokens}'.\n"
        "A01 requires exact Qt patch + compiler/ABI + OS/arch + integration + UI + status on one row; "
        "a cell missing here would be an unstated qualification claim.")
endfunction()

foreach(_row IN LISTS _required_rows)
    _require_row("${_zh_section}" "docs/compatibility.md" "${_row}")
    _require_row("${_en_section}" "docs/en/compatibility.md" "${_row}")
endforeach()

# D7/D1: the maintained viewer and the deferred popup visual are part of the cell boundary, not a footnote.
foreach(_viewer_token
        "RealVNC Viewer 7.15.1"
        "NOT_CONFIRMED")
    string(FIND "${_zh_section}" "${_viewer_token}" _zh_viewer_pos)
    string(FIND "${_en_section}" "${_viewer_token}" _en_viewer_pos)
    if(_zh_viewer_pos EQUAL -1 OR _en_viewer_pos EQUAL -1)
        message(FATAL_ERROR
            "the current development-line section must state '${_viewer_token}' in both languages; "
            "the maintained viewer and the deferred popup visual are part of the cell boundary.")
    endif()
endforeach()

# Owner D2: a Qt 5 route may never appear as Supported in the current line.
function(_forbid_supported section document qt_token)
    string(REPLACE "\n" ";" _lines "${section}")
    foreach(_line IN LISTS _lines)
        string(FIND "${_line}" "${qt_token}" _qt_pos)
        string(FIND "${_line}" "**Supported**" _supported_pos)
        if(NOT _qt_pos EQUAL -1 AND NOT _supported_pos EQUAL -1)
            message(FATAL_ERROR
                "${document}: '${qt_token}' is claimed **Supported** on one row.\n"
                "Owner D2 keeps Qt 5 C++/Widgets Limited, Quick Experimental and QML/Generic/QPA "
                "unsupported or not applicable. Raising that claim needs an Owner decision, not a table edit.")
        endif()
    endforeach()
endfunction()

_forbid_supported("${_zh_section}" "docs/compatibility.md" "5.15.2")
_forbid_supported("${_en_section}" "docs/en/compatibility.md" "5.15.2")
_forbid_supported("${_zh_section}" "docs/compatibility.md" "6.5")
_forbid_supported("${_en_section}" "docs/en/compatibility.md" "6.5")

# The internal route matrix must carry the same exact Kit identity and the candidate evidence anchor.
foreach(_matrix_token
        "msvc2022_64"
        "msvc2019_64"
        "4a00ada"
        "docs/en/compatibility.md")
    string(FIND "${_matrix_text}" "${_matrix_token}" _matrix_pos)
    if(_matrix_pos EQUAL -1)
        message(FATAL_ERROR
            "docs/internal/windows-qt-lts-route-matrix-2026-10-24.md is missing '${_matrix_token}'; "
            "the internal matrix and the public documents must describe the same cells.")
    endif()
endforeach()

# Route-level agreement, not just shared metadata: each internal row has to keep the status the public cell
# states. A Qt 5 route silently promoted here would otherwise pass the checks above.
foreach(_matrix_row
        "C++ / Widgets|QUALIFIED (bounded)"
        "C++ / Widgets via qmake|QUALIFIED (bounded)"
        "C++ / Quick|EXPERIMENTAL / NOT_QUALIFIED"
        "QML|UNSUPPORTED"
        "Generic|UNSUPPORTED"
        "QPA|REJECTED"
        "HyRemoteTool|LIMITED"
        "QPA|QUALIFIED (exact 6.8.3)")
    _require_row("${_matrix_text}"
                 "docs/internal/windows-qt-lts-route-matrix-2026-10-24.md"
                 "${_matrix_row}")
endforeach()

message(STATUS "compatibility cell matrix: exact cells and Owner D2 route truth are consistent")
