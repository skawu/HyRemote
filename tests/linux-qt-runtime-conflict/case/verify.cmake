if(NOT CASE_DIR OR NOT ASSET_DIR OR NOT TEST_CASE OR NOT SELECTED_ROOT OR NOT CONSUMER OR NOT PLUGIN
        OR NOT PLUGIN_NAME)
    message(FATAL_ERROR
        "linux-qt-runtime-conflict: CASE_DIR, ASSET_DIR, TEST_CASE, SELECTED_ROOT, CONSUMER, PLUGIN and "
        "PLUGIN_NAME are required")
endif()
if(NOT DEFINED PREPOPULATE)
    set(PREPOPULATE OFF)
endif()

# Stages the deployment tree the way an install would, runs the generated deployment step, and asserts what
# the hotfix promises. Every assertion is made against the deployment code's own effect on real files: a
# message in a log cannot prove that the selected lineage was the one staged.

set(_deploy "${CASE_DIR}/deploy")
set(_case_lib_name "libhyremote-qtconflict-consumer.so")
set(_selected_runtime "${SELECTED_ROOT}/libQt6Core.so.6")
set(_foreign_runtime "${ASSET_DIR}/qt-b/lib/libQt6Core.so.6")

file(REMOVE_RECURSE "${_deploy}")
file(MAKE_DIRECTORY "${_deploy}/lib" "${_deploy}/plugins/platforms")
file(COPY "${CONSUMER}" DESTINATION "${_deploy}/lib")
file(COPY "${PLUGIN}" DESTINATION "${_deploy}/plugins/platforms")
if(NOT EXISTS "${_deploy}/plugins/platforms/${PLUGIN_NAME}")
    message(FATAL_ERROR
        "linux-qt-runtime-conflict: the plugin was staged as '${PLUGIN_NAME}' but is not there, so the "
        "second depending file this case needs would be missing")
endif()
if(PREPOPULATE)
    file(COPY "${_selected_runtime}" DESTINATION "${_deploy}/lib")
endif()

function(linux_qt_conflict_run_uncollected_probe out_var)
    file(WRITE "${CASE_DIR}/no-collection-probe.cmake"
"file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES
    \"${_deploy}/lib/${_case_lib_name}\"
    \"${_deploy}/plugins/platforms/${PLUGIN_NAME}\"
    DIRECTORIES \"${SELECTED_ROOT}\"
    RESOLVED_DEPENDENCIES_VAR _uncollected_resolved
    UNRESOLVED_DEPENDENCIES_VAR _uncollected_unresolved
)
")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -P "${CASE_DIR}/no-collection-probe.cmake"
        RESULT_VARIABLE _probe_rc
        OUTPUT_VARIABLE _probe_out
        ERROR_VARIABLE _probe_out)
    set(${out_var} "${_probe_rc}" PARENT_SCOPE)
    set(linux_qt_conflict_probe_output "${_probe_out}" PARENT_SCOPE)
endfunction()

if(TEST_CASE STREQUAL "without-collection")
    # The control case: the same two roots and the same two depending files, resolved by the un-adjudicated
    # call the defect was reported against. Ambiguity must reproduce here, or every "the hotfix decides"
    # assertion below would be asserting against a situation that never was ambiguous.
    linux_qt_conflict_run_uncollected_probe(_rc)
    if(_rc EQUAL 0)
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: ambiguity did not reproduce without conflict collection, so this "
            "regression would not be able to prove the hotfix decides anything")
    endif()
    string(FIND "${linux_qt_conflict_probe_output}" "Multiple conflicting paths found for libQt6Core.so.6"
        _fragment)
    if(_fragment EQUAL -1)
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: the un-collected call failed for an unexpected reason: "
            "${linux_qt_conflict_probe_output}")
    endif()
    message(STATUS "linux-qt-runtime-conflict: without-collection ambiguity reproduced")
    return()
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DQT_DEPLOY_PREFIX=${_deploy}"
        "-DQT_DEPLOY_LIB_DIR=lib"
        "-DQT_DEPLOY_PLUGINS_DIR=plugins"
        -P "${CASE_DIR}/runtime-bootstrap.cmake"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _out)

function(linux_qt_conflict_require_deployed_selected_payload)
    set(_deployed "${_deploy}/lib/libQt6Core.so.6")
    if(NOT EXISTS "${_deployed}")
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: the deployment staged no Qt runtime at all: ${_deployed}")
    endif()
    if(NOT EXISTS "${_selected_runtime}" OR NOT EXISTS "${_foreign_runtime}")
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: this case compares payloads that are not there: "
            "${_selected_runtime} / ${_foreign_runtime}")
    endif()
    file(MD5 "${_selected_runtime}" _selected_md5)
    file(MD5 "${_foreign_runtime}" _foreign_md5)
    file(MD5 "${_deployed}" _deployed_md5)
    if(_deployed_md5 STREQUAL _foreign_md5)
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: the deployment staged the foreign same-SONAME payload instead of the "
            "selected consumer Qt runtime")
    endif()
    if(NOT _deployed_md5 STREQUAL _selected_md5)
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: the deployed Qt runtime is neither the selected root's payload nor a "
            "copy of it")
    endif()
    file(GLOB _deployed_runtime "${_deploy}/lib/libQt6Core.so.6*")
    list(LENGTH _deployed_runtime _deployed_runtime_count)
    if(NOT _deployed_runtime_count EQUAL 1)
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: the deployment tree carries ${_deployed_runtime_count} copies of the "
            "Qt runtime: ${_deployed_runtime}")
    endif()
endfunction()

function(linux_qt_conflict_require_failure expected_fragment)
    if(_rc EQUAL 0)
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: '${TEST_CASE}' had to fail closed and did not: ${_out}")
    endif()
    string(FIND "${_out}" "${expected_fragment}" _fragment)
    if(_fragment EQUAL -1)
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: '${TEST_CASE}' failed closed for the wrong reason: ${_out}")
    endif()
    message(STATUS "linux-qt-runtime-conflict: ${TEST_CASE} failed closed")
endfunction()

if(TEST_CASE STREQUAL "selected-root-wins")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: a conflict between the selected root and a foreign root must be "
            "decided, not refused: ${_out}")
    endif()
    linux_qt_conflict_require_deployed_selected_payload()
    message(STATUS "linux-qt-runtime-conflict: selected-root-wins deployed the selected payload")
elseif(TEST_CASE STREQUAL "deployed-copy-wins")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: a redeployment over an already staged selected payload must succeed: ${_out}")
    endif()
    linux_qt_conflict_require_deployed_selected_payload()
    message(STATUS "linux-qt-runtime-conflict: deployed-copy-wins kept the staged selected payload")
elseif(TEST_CASE STREQUAL "no-selected-candidate")
    linux_qt_conflict_require_failure("is not a Qt lineage conflict HyRemote may decide")
elseif(TEST_CASE STREQUAL "ambiguous-selected-candidates")
    linux_qt_conflict_require_failure("could not choose")
elseif(TEST_CASE STREQUAL "non-qt-conflict")
    linux_qt_conflict_require_failure("is not a Qt lineage conflict HyRemote may decide")
else()
    message(FATAL_ERROR "linux-qt-runtime-conflict: unknown case '${TEST_CASE}'")
endif()
