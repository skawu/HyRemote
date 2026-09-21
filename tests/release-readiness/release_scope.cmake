# Release-train scope selection and its executable acceptance matrix (#231).
#
# Selection:   cmake -DHYREMOTE_SOURCE_DIR=<src> -DHYREMOTE_RELEASE_VERSION=<version> -P release_scope.cmake
#              prints the selected scope, and fails closed on anything it cannot classify.
# Self test:   cmake -DHYREMOTE_SOURCE_DIR=<src> -DHYREMOTE_SCOPE_SELF_TEST=ON -P release_scope.cmake
#              proves the acceptance matrix. Every case runs a child selection and checks its exit status, so the
#              cases exercise the same code path a release would, not a second implementation of the rules.
#
# The authority is one train map. A requested release version selects one train; version digits never encode
# frontend, platform, Qt family, UI family or CI lane, and a train that is merely representable never blocks an
# earlier train. Unknown versions, retired planning labels, malformed records and unclassified note keys all fail
# closed: selection must refuse to answer rather than answer wrongly.

cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

if(DEFINED HYREMOTE_RELEASE_AUTHORITY AND NOT HYREMOTE_RELEASE_AUTHORITY STREQUAL "")
    set(authority_path "${HYREMOTE_RELEASE_AUTHORITY}")
else()
    set(authority_path "${HYREMOTE_SOURCE_DIR}/.github/release/release-trains.json")
endif()

set(retired_planning_labels "0.0.1.0;0.0.2.0;0.0.3.0")

function(scope_fail reason)
    message(FATAL_ERROR "release-scope: ${reason}")
endfunction()

# ---------------------------------------------------------------- manifest loading, fail closed

if(NOT EXISTS "${authority_path}")
    scope_fail("release-train authority is missing: ${authority_path}")
endif()

file(READ "${authority_path}" authority_json)

string(JSON authority_schema ERROR_VARIABLE schema_error GET "${authority_json}" schema)
if(schema_error)
    scope_fail("release-train authority is malformed: ${schema_error}")
endif()
if(NOT authority_schema STREQUAL "2")
    scope_fail("unsupported release-train authority schema '${authority_schema}'")
endif()

string(JSON sentinel ERROR_VARIABLE sentinel_error GET "${authority_json}" development_sentinel)
if(sentinel_error OR NOT sentinel STREQUAL "0.0.0")
    scope_fail("development sentinel must remain 0.0.0")
endif()

string(JSON retired_count ERROR_VARIABLE retired_error LENGTH "${authority_json}" retired_profiles)
if(retired_error)
    scope_fail("release-train authority declares no retired_profiles list: ${retired_error}")
endif()
math(EXPR retired_last "${retired_count} - 1")
set(authority_retired "")
foreach(index RANGE 0 ${retired_last})
    string(JSON entry GET "${authority_json}" retired_profiles ${index})
    list(APPEND authority_retired "${entry}")
endforeach()
if(NOT "${authority_retired}" STREQUAL "${retired_planning_labels}")
    scope_fail("retired planning labels drifted: '${authority_retired}'")
endif()

string(JSON trains_type ERROR_VARIABLE train_error TYPE "${authority_json}" trains)
if(train_error OR NOT trains_type STREQUAL "OBJECT")
    scope_fail("release-train authority has no trains map: ${train_error}")
endif()

# A note key explains a number; it must never be the only place a number exists. That is the train-level form of the
# authority-leak rule: a reference that belongs to no classified list would be authority hiding in prose.
function(validate_train_notes version)
    string(JSON note_keys ERROR_VARIABLE note_error GET "${authority_json}" trains "${version}" notes)
    if(note_error)
        return()
    endif()
    # The note object arrives as text, so its keys are the quoted numbers at member position. Values are prose and
    # never quote a number, so this stays deterministic without depending on a member-enumeration option.
    string(REGEX MATCHALL "\"([0-9]+)\"[ \t]*:" _note_key_matches "${note_keys}")
    set(note_key_numbers "")
    foreach(_note_key IN LISTS _note_key_matches)
        string(REGEX REPLACE "[^0-9]" "" _note_key_number "${_note_key}")
        list(APPEND note_key_numbers "${_note_key_number}")
    endforeach()
    set(classified "")
    foreach(bucket IN ITEMS mandatory_children cross_version_references non_blockers conditional_items
                        embedded_deferred_issue_numbers classified_referenced_issue_numbers)
        string(JSON bucket_count ERROR_VARIABLE bucket_error LENGTH
               "${authority_json}" trains "${version}" ${bucket})
        if(bucket_error OR bucket_count EQUAL 0)
            continue()
        endif()
        math(EXPR bucket_last "${bucket_count} - 1")
        foreach(index RANGE 0 ${bucket_last})
            string(JSON entry GET "${authority_json}" trains "${version}" ${bucket} ${index})
            list(APPEND classified "${entry}")
        endforeach()
    endforeach()
    string(JSON parent GET "${authority_json}" trains "${version}" authority_parent)
    list(APPEND classified "${parent}")
    foreach(key IN LISTS note_key_numbers)
        if(NOT key IN_LIST classified)
            scope_fail(
                "release train '${version}' explains #${key} but classifies it nowhere; an authority reference "
                "must belong to a classified list")
        endif()
    endforeach()
endfunction()

# selected_scope(<version> out_parent out_children out_references out_status)
function(selected_scope version out_parent out_children out_references out_status)
    string(JSON member_type ERROR_VARIABLE member_error TYPE "${authority_json}" trains "${version}")
    if(member_error OR NOT member_type STREQUAL "OBJECT")
        scope_fail("unknown or unclassifiable release train '${version}'; selection fails closed")
    endif()

    list(FIND authority_retired "${version}" retired_position)
    if(NOT retired_position EQUAL -1)
        scope_fail("'${version}' is a retired planning label, not a release train")
    endif()

    string(JSON parent ERROR_VARIABLE parent_error GET "${authority_json}" trains "${version}" authority_parent)
    if(parent_error)
        scope_fail("release train '${version}' declares no authority_parent")
    endif()

    string(JSON status ERROR_VARIABLE status_error GET "${authority_json}" trains "${version}" scope_status)
    if(status_error OR status STREQUAL "")
        set(status "active")
    endif()

    string(JSON children_count ERROR_VARIABLE children_error LENGTH
           "${authority_json}" trains "${version}" mandatory_children)
    if(children_error)
        scope_fail("release train '${version}' declares no mandatory_children list")
    endif()
    set(children "")
    if(children_count GREATER 0)
        math(EXPR children_last "${children_count} - 1")
        foreach(index RANGE 0 ${children_last})
            string(JSON child GET "${authority_json}" trains "${version}" mandatory_children ${index})
            if(child LESS 1)
                scope_fail("release train '${version}' declares '${child}' as a mandatory child")
            endif()
            list(APPEND children "${child}")
        endforeach()
    endif()

    # An active train with nothing closeable would silently authorize a release, which is exactly what an
    # unclassified mandatory set must never do. The declared-unpopulated trains are representable-only and say so.
    if(children STREQUAL "" AND status STREQUAL "active")
        scope_fail(
            "release train '${version}' is active but declares no mandatory children, so it would authorize a "
            "release without any closeable authority")
    endif()

    set(references "")
    string(JSON references_count ERROR_VARIABLE references_error LENGTH
           "${authority_json}" trains "${version}" cross_version_references)
    if(NOT references_error AND references_count GREATER 0)
        math(EXPR references_last "${references_count} - 1")
        foreach(index RANGE 0 ${references_last})
            string(JSON reference GET "${authority_json}" trains "${version}" cross_version_references ${index})
            list(APPEND references "${reference}")
        endforeach()
    endif()

    validate_train_notes("${version}")

    set(${out_parent} "${parent}" PARENT_SCOPE)
    set(${out_children} "${children}" PARENT_SCOPE)
    set(${out_references} "${references}" PARENT_SCOPE)
    set(${out_status} "${status}" PARENT_SCOPE)
endfunction()

# conditional_train(<version> <out_active> <out_activation_authority>)
function(conditional_train version out_active out_activation)
    set(active FALSE)
    set(activation "")
    string(JSON conditional_count ERROR_VARIABLE conditional_error LENGTH "${authority_json}" conditional_trains)
    if(NOT conditional_error AND conditional_count GREATER 0)
        math(EXPR conditional_last "${conditional_count} - 1")
        foreach(index RANGE 0 ${conditional_last})
            string(JSON entry_version GET "${authority_json}" conditional_trains ${index} version)
            if(entry_version STREQUAL "${version}")
                string(JSON entry_active GET "${authority_json}" conditional_trains ${index} active)
                string(JSON entry_activation GET "${authority_json}" conditional_trains ${index} activation_authority)
                set(activation "${entry_activation}")
                if(entry_active STREQUAL "true")
                    set(active TRUE)
                endif()
            endif()
        endforeach()
    endif()
    set(${out_active} "${active}" PARENT_SCOPE)
    set(${out_activation} "${activation}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------- self test

if(DEFINED HYREMOTE_SCOPE_SELF_TEST AND HYREMOTE_SCOPE_SELF_TEST)
    set(case_failures 0)
    set(cases_run 0)

    # Each case runs a real child selection and inspects its exit status, so the matrix exercises the same code path
    # a release uses. The result is handed back through one well-known variable because the assertion helpers below
    # are macros: they run in the case scope, where the failure counter actually lives.
    function(run_selection version expectation_placeholder out_output)
        execute_process(
            COMMAND "${CMAKE_COMMAND}"
                "-DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}"
                "-DHYREMOTE_RELEASE_AUTHORITY=${authority_path}"
                "-DHYREMOTE_RELEASE_VERSION=${version}"
                -P "${CMAKE_CURRENT_LIST_FILE}"
            RESULT_VARIABLE _selection_result
            OUTPUT_VARIABLE _selection_output
            ERROR_VARIABLE _selection_error)
        set(${out_output} "${_selection_output}" PARENT_SCOPE)
        set(SCOPE_SELECTION_RESULT "${_selection_result}" PARENT_SCOPE)
        set(SCOPE_SELECTION_ERROR "${_selection_error}" PARENT_SCOPE)
    endfunction()

    macro(expect_success description version)
        run_selection("${version}" ignored _self_output)
        math(EXPR cases_run "${cases_run} + 1")
        if(NOT SCOPE_SELECTION_RESULT EQUAL 0 OR NOT _self_output MATCHES "RELEASE_SCOPE=")
            message(STATUS
                "release-scope self test: ${description} should have been accepted but was refused: "
                "${SCOPE_SELECTION_ERROR}")
            math(EXPR case_failures "${case_failures} + 1")
        endif()
    endmacro()

    macro(expect_refusal description version)
        run_selection("${version}" ignored _self_output)
        math(EXPR cases_run "${cases_run} + 1")
        if(SCOPE_SELECTION_RESULT EQUAL 0)
            message(STATUS "release-scope self test: ${description} should have been refused but was accepted")
            math(EXPR case_failures "${case_failures} + 1")
        endif()
    endmacro()

    # 1. The development sentinel is valid and is not a release train.
    run_selection("0.0.0" TRUE sentinel_output)
    math(EXPR cases_run "${cases_run} + 1")
    if(NOT sentinel_output MATCHES "RELEASE_SCOPE=development-sentinel")
        message(STATUS "release-scope self test: 0.0.0 must resolve to the development sentinel")
        math(EXPR _failures "${case_failures} + 1")
        set(case_failures "${_failures}")
    endif()

    # 2. 0.1.0.0 is a legal coherent release train.
    expect_success("0.1.0.0 is accepted" "0.1.0.0")

    # 3.-5. The retired frontend-coded planning labels stay refused.
    expect_refusal("0.0.1.0 stays refused" "0.0.1.0")
    expect_refusal("0.0.2.0 stays refused" "0.0.2.0")
    expect_refusal("0.0.3.0 stays refused" "0.0.3.0")

    # 6.-8. V0.1 resolves its own authority and exactly its own closeable children.
    run_selection("0.1.0.0" TRUE v01_output)
    math(EXPR cases_run "${cases_run} + 1")
    if(NOT v01_output MATCHES "AUTHORITY_PARENT=229")
        message(STATUS "release-scope self test: 0.1.0.0 must select authority #229")
        math(EXPR _failures "${case_failures} + 1")
        set(case_failures "${_failures}")
    endif()
    if(NOT v01_output MATCHES "MANDATORY_CHILDREN=230,231,232,237,238,241")
        message(STATUS "release-scope self test: 0.1.0.0 mandatory set drifted: ${v01_output}")
        math(EXPR _failures "${case_failures} + 1")
        set(case_failures "${_failures}")
    endif()
    math(EXPR cases_run "${cases_run} + 2")
    foreach(umbrella IN ITEMS 41 143 176 209)
        if(v01_output MATCHES "MANDATORY_CHILDREN=[^\n]*\\b${umbrella}\\b")
            message(STATUS "release-scope self test: umbrella #${umbrella} must not be a V0.1 mandatory child")
            math(EXPR _failures "${case_failures} + 1")
            set(case_failures "${_failures}")
        endif()
        math(EXPR cases_run "${cases_run} + 1")
    endforeach()

    # 9.-13. Later trains are representable, each with its own authority parent.
    foreach(pair IN ITEMS "0.2.0.0;233" "0.3.0.0;234" "0.4.0.0;235" "1.0.0.0;33" "1.1.0.0;236")
        list(GET pair 0 later_version)
        list(GET pair 1 later_authority)
        run_selection("${later_version}" TRUE later_output)
        math(EXPR cases_run "${cases_run} + 1")
        if(NOT later_output MATCHES "AUTHORITY_PARENT=${later_authority}")
            message(STATUS
                "release-scope self test: ${later_version} must select authority #${later_authority}: ${later_output}")
            math(EXPR _failures "${case_failures} + 1")
            set(case_failures "${_failures}")
        endif()
    endforeach()

    # 14. An unknown train is refused rather than resolved to the nearest or to V1.
    expect_refusal("unknown 0.9.0.0 is refused" "0.9.0.0")
    expect_refusal("unknown 2.0.0.0 is refused" "2.0.0.0")
    expect_refusal("empty version is refused" "")

    # 15. A malformed authority is refused.
    set(malformed_path "${CMAKE_CURRENT_BINARY_DIR}/release-trains-malformed.json")
    file(WRITE "${malformed_path}" "{ this is not json ")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}"
            "-DHYREMOTE_RELEASE_AUTHORITY=${malformed_path}"
            "-DHYREMOTE_RELEASE_VERSION=0.1.0.0"
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE malformed_result)
    math(EXPR cases_run "${cases_run} + 1")
    if(malformed_result EQUAL 0)
        message(STATUS "release-scope self test: a malformed authority must be refused")
        math(EXPR _failures "${case_failures} + 1")
        set(case_failures "${_failures}")
    endif()

    # 15b. A wrong schema is refused as well.
    set(wrong_schema_path "${CMAKE_CURRENT_BINARY_DIR}/release-trains-wrong-schema.json")
    file(WRITE "${wrong_schema_path}" "{ \"schema\": 1, \"development_sentinel\": \"0.0.0\" }")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}"
            "-DHYREMOTE_RELEASE_AUTHORITY=${wrong_schema_path}"
            "-DHYREMOTE_RELEASE_VERSION=0.1.0.0"
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE wrong_schema_result)
    math(EXPR cases_run "${cases_run} + 1")
    if(wrong_schema_result EQUAL 0)
        message(STATUS "release-scope self test: an unsupported authority schema must be refused")
        math(EXPR _failures "${case_failures} + 1")
        set(case_failures "${_failures}")
    endif()

    # 16. An authority reference that only exists as prose is refused, and an active train with nothing closeable
    #     is refused rather than silently authorizing a release. The leak case explains a number that no classified
    #     list of that train contains, which is authority hiding in a note.
    set(leak_path "${CMAKE_CURRENT_BINARY_DIR}/release-trains-leak.json")
    file(READ "${authority_path}" leak_json)
    string(REPLACE
        "\"209\": \"Cross-version umbrella."
        "\"400\": \"Cross-version umbrella."
        leak_json "${leak_json}")
    file(WRITE "${leak_path}" "${leak_json}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}"
            "-DHYREMOTE_RELEASE_AUTHORITY=${leak_path}"
            "-DHYREMOTE_RELEASE_VERSION=0.1.0.0"
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE leak_result
        ERROR_VARIABLE leak_error)
    math(EXPR cases_run "${cases_run} + 1")
    if(leak_result EQUAL 0)
        message(STATUS "release-scope self test: an unclassified authority reference must be refused")
        math(EXPR _failures "${case_failures} + 1")
        set(case_failures "${_failures}")
    endif()

    set(empty_path "${CMAKE_CURRENT_BINARY_DIR}/release-trains-empty.json")
    file(READ "${authority_path}" empty_json)
    string(REPLACE
        "\"mandatory_children\": [ 230, 231, 232, 237, 238, 241 ]"
        "\"mandatory_children\": []"
        empty_json "${empty_json}")
    file(WRITE "${empty_path}" "${empty_json}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}"
            "-DHYREMOTE_RELEASE_AUTHORITY=${empty_path}"
            "-DHYREMOTE_RELEASE_VERSION=0.1.0.0"
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE empty_result)
    math(EXPR cases_run "${cases_run} + 1")
    if(empty_result EQUAL 0)
        message(STATUS "release-scope self test: an active train with no mandatory children must be refused")
        math(EXPR _failures "${case_failures} + 1")
        set(case_failures "${_failures}")
    endif()

    # 17.-18. V1.2 is conditional: the number existing does not authorize it, and its activation authority is
    #         recorded so activation evidence has somewhere to land.
    conditional_train("1.2.0.0" v12_active v12_activation)
    math(EXPR cases_run "${cases_run} + 2")
    if(v12_active)
        message(STATUS "release-scope self test: V1.2 must not be active by default")
        math(EXPR _failures "${case_failures} + 1")
        set(case_failures "${_failures}")
    endif()
    if(NOT v12_activation STREQUAL "123")
        message(STATUS "release-scope self test: V1.2 activation authority must remain #123")
        math(EXPR _failures "${case_failures} + 1")
        set(case_failures "${_failures}")
    endif()
    expect_refusal("V1.2 is not a release scope before activation" "1.2.0.0")

    # V1 authority preserved by the migration: the full mandatory set and both authorities.
    run_selection("1.0.0.0" TRUE v1_output)
    string(REGEX MATCH "MANDATORY_CHILDREN=([^\n]*)" _ignored "${v1_output}")
    set(v1_children "${CMAKE_MATCH_1}")
    string(REPLACE "," ";" v1_children_list "${v1_children}")
    list(LENGTH v1_children_list v1_children_count)
    math(EXPR cases_run "${cases_run} + 1")
    if(NOT v1_children_count EQUAL 26)
        message(STATUS "release-scope self test: V1.0.0.0 must keep all 26 former mandatory issues")
        math(EXPR _failures "${case_failures} + 1")
        set(case_failures "${_failures}")
    endif()

    if(case_failures GREATER 0)
        message(FATAL_ERROR "release-scope self test: ${case_failures} of ${cases_run} case(s) failed")
    endif()
    message(STATUS
        "release-scope self test: PASS (schema ${authority_schema}, ${cases_run} cases, "
        "sentinel 0.0.0, refused 0.0.x, V0.1 -> #229, later trains representable, V1.2 conditional on #123)")
    return()
endif()

# ---------------------------------------------------------------- selection

if(NOT DEFINED HYREMOTE_RELEASE_VERSION OR HYREMOTE_RELEASE_VERSION STREQUAL "")
    scope_fail("HYREMOTE_RELEASE_VERSION is required for selection")
endif()

if(HYREMOTE_RELEASE_VERSION STREQUAL sentinel)
    message(STATUS "RELEASE_VERSION=${HYREMOTE_RELEASE_VERSION}")
    message(STATUS "RELEASE_SCOPE=development-sentinel")
    message(STATUS "AUTHORITY_PARENT=")
    message(STATUS "MANDATORY_CHILDREN=")
    return()
endif()

conditional_train("${HYREMOTE_RELEASE_VERSION}" selected_conditional_active selected_activation)
if(NOT selected_conditional_active)
    string(JSON conditional_count ERROR_VARIABLE conditional_error LENGTH "${authority_json}" conditional_trains)
    if(NOT conditional_error AND conditional_count GREATER 0)
        math(EXPR conditional_last "${conditional_count} - 1")
        foreach(index RANGE 0 ${conditional_last})
            string(JSON entry_version GET "${authority_json}" conditional_trains ${index} version)
            if(entry_version STREQUAL "${HYREMOTE_RELEASE_VERSION}")
                scope_fail(
                    "${HYREMOTE_RELEASE_VERSION} is conditional on activation authority #${selected_activation} and "
                    "has recorded no activation; a version number alone does not authorize a release scope")
            endif()
        endforeach()
    endif()
endif()

selected_scope("${HYREMOTE_RELEASE_VERSION}" parent children references status)

string(REPLACE ";" "," children_text "${children}")
string(REPLACE ";" "," references_text "${references}")
message(STATUS "RELEASE_VERSION=${HYREMOTE_RELEASE_VERSION}")
message(STATUS "RELEASE_SCOPE=release-train")
message(STATUS "AUTHORITY_PARENT=${parent}")
message(STATUS "SCOPE_STATUS=${status}")
message(STATUS "MANDATORY_CHILDREN=${children_text}")
message(STATUS "CROSS_VERSION_REFERENCES=${references_text}")
