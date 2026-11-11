cmake_minimum_required(VERSION 3.21)
# Script discipline guard (fail-closed).
#
# Executed as a `facts-layout` readiness case by
# tests/release-readiness/run_grouped_checks.cmake, which the no-Qt
# authority-governance lane runs. `ci-merge-verdict` requires that lane to
# succeed, and the protected develop ruleset requires ci-merge-verdict.
# A failure here therefore blocks the merge without any client-side setting.
#
# Standalone:
#   cmake -DHYREMOTE_SOURCE_DIR=<repo> -P check_script_discipline.cmake
#   cmake -DHYREMOTE_SOURCE_DIR=<repo> -DHYREMOTE_SCRIPT_SELF_TEST=ON \
#         -P check_script_discipline.cmake
#
# script_inventory.txt is the planned script register: declare a new script
# there during task planning, before creating it. It is also the archive-safe
# inventory. When .git is present the register must match the tracked script
# set exactly; source archives without .git use the register alone. The file
# contains one repository-relative path per line and no comments, so CMake
# list parsing cannot mistake prose for an entry.

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "script-discipline: HYREMOTE_SOURCE_DIR is required")
endif()
get_filename_component(_script_discipline_root "${HYREMOTE_SOURCE_DIR}" ABSOLUTE)
if(NOT IS_DIRECTORY "${_script_discipline_root}")
    message(FATAL_ERROR "script-discipline: HYREMOTE_SOURCE_DIR must be an existing directory: ${HYREMOTE_SOURCE_DIR}")
endif()
set(HYREMOTE_SOURCE_DIR "${_script_discipline_root}")

set(_script_suffixes ".py" ".ps1" ".cmd" ".bat" ".sh")

# Directories where an executable script may live. Derived from the tracked
# script inventory, not invented: .github/, cmake/, docs/, src/**/tests/,
# tests/, tools/perf/, tools/local-owner/, plus the single root build.cmd.
set(_planned_prefixes
    ".github/release/"
    ".github/scripts/"
    "cmake/"
    "docs/"
    "src/integrations/cpp/tests/"
    "tests/"
    "tools/perf/"
    "tools/local-owner/"
)
set(_planned_root_files "build.cmd")

function(_script_discipline_starts_with value prefix out_var)
    string(FIND "${value}" "${prefix}" _pos)
    if(_pos EQUAL 0)
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(_script_discipline_verdict rel_path out_var)
    set(_verdict "REJECT")
    if(rel_path MATCHES "/")
        foreach(_prefix IN LISTS _planned_prefixes)
            _script_discipline_starts_with("${rel_path}" "${_prefix}" _matches)
            if(_matches)
                set(_verdict "OK")
                break()
            endif()
        endforeach()
        # src/**/tests/ is allowed generally, not just the one integration.
        if(NOT _verdict STREQUAL "OK" AND rel_path MATCHES "^src/.*/tests/")
            set(_verdict "OK")
        endif()
    else()
        foreach(_allowed IN LISTS _planned_root_files)
            if(rel_path STREQUAL _allowed)
                set(_verdict "OK")
            endif()
        endforeach()
    endif()
    # tmp/ and .codebuddy/ are never script homes, even if a prefix above
    # would otherwise match.
    _script_discipline_starts_with("${rel_path}" "tmp/" _in_tmp)
    _script_discipline_starts_with("${rel_path}" ".codebuddy/" _in_assets)
    if(_in_tmp OR _in_assets)
        set(_verdict "REJECT")
    endif()
    set(${out_var} "${_verdict}" PARENT_SCOPE)
endfunction()

function(_script_discipline_assert rel_path expected)
    _script_discipline_verdict("${rel_path}" _actual)
    if(NOT _actual STREQUAL expected)
        message(FATAL_ERROR "script-discipline self-test: '${rel_path}' expected ${expected}, got ${_actual}")
    endif()
endfunction()

# Positive/negative self-test: the classifier itself must not silently pass.
if(HYREMOTE_SCRIPT_SELF_TEST)
    _script_discipline_assert("tools/perf/aggregate_cpu_probe.py" "OK")
    _script_discipline_assert("tests/preflight/run_vencrypt_interop_probe.py" "OK")
    _script_discipline_assert(".github/scripts/resolve-ci-scope.py" "OK")
    _script_discipline_assert("cmake/HyRemoteQtKitPrompt.cmd" "OK")
    _script_discipline_assert("build.cmd" "OK")
    _script_discipline_assert("tmp/sem/observer.py" "REJECT")
    _script_discipline_assert("tmp/w922-off/probe.py" "REJECT")
    _script_discipline_assert(".codebuddy/hooks/helper.py" "REJECT")
    _script_discipline_assert("probe.py" "REJECT")
    _script_discipline_assert("fg0.cmd" "REJECT")
    _script_discipline_assert("evidence/run.py" "REJECT")
    message(STATUS "script-discipline self-test PASS (6 positive, 5 negative)")
    return()
endif()

# 1. The institution itself must stay present and enforceable. Removing or
#    disabling any of these must fail the governance lane.
set(_required_assets
    ".codebuddy/rules/session-anchor.md"
    ".codebuddy/rules/evidence-and-workspace-discipline.md"
    ".codebuddy/skills/script-planning/SKILL.md"
)
foreach(_asset IN LISTS _required_assets)
    if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/${_asset}")
        message(FATAL_ERROR "script-discipline: required project asset missing: ${_asset}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/.codebuddy/rules/session-anchor.md" _anchor_text)
string(FIND "${_anchor_text}" "alwaysApply: true" _anchor_pos)
if(_anchor_pos EQUAL -1)
    message(FATAL_ERROR "script-discipline: session-anchor.md no longer has alwaysApply: true")
endif()
string(FIND "${_anchor_text}" "Never create a script under `tmp/`" _gate_pos)
if(_gate_pos EQUAL -1)
    message(FATAL_ERROR "script-discipline: session-anchor.md lost the tmp script gate")
endif()

file(READ "${HYREMOTE_SOURCE_DIR}/.codebuddy/rules/evidence-and-workspace-discipline.md" _evidence_text)
string(FIND "${_evidence_text}" "never executable scripts" _evidence_pos)
if(_evidence_pos EQUAL -1)
    message(FATAL_ERROR "script-discipline: workspace rule lost the 'tmp holds outputs only' clause")
endif()

# 2. Read the planned script register. It is the archive-safe inventory: a
#    released source archive has no .git, so the regular facts-layout CTest
#    must still run without any Git metadata.
set(_inventory_path "${HYREMOTE_SOURCE_DIR}/tests/release-readiness/script_inventory.txt")
if(NOT EXISTS "${_inventory_path}")
    message(FATAL_ERROR "script-discipline: missing planned script register: ${_inventory_path}")
endif()
file(READ "${_inventory_path}" _inventory_text)
string(REPLACE "\r\n" "\n" _inventory_text "${_inventory_text}")
string(REPLACE "\n" ";" _inventory_lines "${_inventory_text}")
set(_declared "")
foreach(_line IN LISTS _inventory_lines)
    string(STRIP "${_line}" _line)
    if(_line STREQUAL "" OR _line MATCHES "^#")
        continue()
    endif()
    list(APPEND _declared "${_line}")
endforeach()
if("${_declared}" STREQUAL "")
    message(FATAL_ERROR "script-discipline: planned script register is empty (false green)")
endif()

# 3. When Git metadata is available, the register must match reality exactly:
#    a new script that was not declared fails the gate; a stale declared script
#    that no longer exists fails too. Without .git, the register is the source
#    of truth (source-archive / distributed-source build).
set(_tracked_scripts "")
set(_git_available FALSE)
if(EXISTS "${HYREMOTE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND git ls-files
        WORKING_DIRECTORY "${HYREMOTE_SOURCE_DIR}"
        RESULT_VARIABLE _git_result
        OUTPUT_VARIABLE _tracked_text
        ERROR_VARIABLE _git_error
    )
    if(_git_result EQUAL 0 AND NOT "${_tracked_text}" STREQUAL "")
        set(_git_available TRUE)
        string(REPLACE "\r\n" "\n" _tracked_text "${_tracked_text}")
        string(REPLACE "\n" ";" _tracked_list "${_tracked_text}")
        foreach(_rel IN LISTS _tracked_list)
            string(STRIP "${_rel}" _rel)
            if(_rel STREQUAL "")
                continue()
            endif()
            foreach(_suffix IN LISTS _script_suffixes)
                if(_rel MATCHES "${_suffix}$")
                    list(APPEND _tracked_scripts "${_rel}")
                    break()
                endif()
            endforeach()
        endforeach()
    endif()
endif()

if(_git_available)
    set(_expected_sorted "${_declared}")
    set(_actual_sorted "${_tracked_scripts}")
    list(SORT _expected_sorted)
    list(SORT _actual_sorted)
    if(NOT "${_expected_sorted}" STREQUAL "${_actual_sorted}")
        message(FATAL_ERROR
            "script-discipline: planned script register differs from tracked scripts. "
            "Declare new scripts in tests/release-readiness/script_inventory.txt during planning. "
            "declared=[${_expected_sorted}] tracked=[${_actual_sorted}]")
    endif()
    set(_to_check "${_tracked_scripts}")
    set(_origin "git index + register")
else()
    set(_to_check "${_declared}")
    set(_origin "register only (no Git metadata)")
endif()

set(_script_count 0)
set(_violations "")
foreach(_rel IN LISTS _to_check)
    math(EXPR _script_count "${_script_count} + 1")
    _script_discipline_verdict("${_rel}" _verdict)
    if(_verdict STREQUAL "REJECT")
        list(APPEND _violations "${_rel}")
    endif()
endforeach()

if(_script_count EQUAL 0)
    message(FATAL_ERROR "script-discipline: zero scripts found (false green)")
endif()
if(NOT "${_violations}" STREQUAL "")
    message(FATAL_ERROR "script-discipline: scripts outside planned directories: ${_violations}")
endif()

# 3. The guard must itself be registered, otherwise it never executes.
file(READ "${HYREMOTE_SOURCE_DIR}/tests/release-readiness/run_grouped_checks.cmake" _runner_text)
string(FIND "${_runner_text}" "check_script_discipline.cmake" _registered_pos)
if(_registered_pos EQUAL -1)
    message(FATAL_ERROR "script-discipline: guard is not registered in run_grouped_checks.cmake")
endif()

message(STATUS "script-discipline PASS: ${_script_count} scripts in planned directories (${_origin}); institution intact")
