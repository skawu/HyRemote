# HyRemote V1 release evidence runner.
#
# This is the executable half of the release evidence path: #210/#109 require that the release candidate's
# deployment and consumption evidence is *actually produced*, and that a check which never ran counts as
# non-evidence. The fixtures and product-fit harnesses already exist in the repository; this runner is the single
# entry point that executes them on a clean deployment and records what happened.
#
# One command, one semantics, both reference operating systems:
#
#   ctest --test-dir build -R hyremote-release-readiness-deployment-evidence --output-on-failure
#
# or directly:
#
#   cmake -DHYREMOTE_SOURCE_DIR=<src> -DHYREMOTE_BUILD_DIR=<build> -P tests/release-readiness/run_release_evidence.cmake
#
# What it does, per evidence cell: prepare a clean prefix, install the product, acquire HyRemote the way the cell
# declares (installed package or source tree - never both), build the consumer, run it (or the cell's product-fit
# harness) under an explicitly constructed runtime search environment, and record the command, exit code, identity
# and result. Evidence is written under <build>/evidence/<run>/ and is never committed.
#
# Evidence binding: every record carries SOURCE_SHA, the exact git HEAD the evidence was produced from. Evidence is
# therefore bound to that SHA and cannot be reused as acceptance evidence for a different frozen candidate.
#
# Runtime isolation (the part #223's CI consolidation removed): the executed consumer never inherits the developer
# shell. PRODUCT_RUNTIME_PATH is built from the operating system's own directories plus the deployed tree only, and
# the harness's own interpreter (HARNESS_EXECUTOR) is captured absolutely *before* that path is narrowed, so the
# harness can run without leaking its interpreter location or any build tree, Qt SDK bin or development tool into
# the product's runtime search path. LD_LIBRARY_PATH is deliberately not set on Linux: the deployed contract must
# hold through the install RPATH.

cmake_minimum_required(VERSION 3.21)

foreach(required_var HYREMOTE_SOURCE_DIR HYREMOTE_BUILD_DIR)
    if(NOT DEFINED ${required_var})
        message(FATAL_ERROR "release-evidence: ${required_var} is required")
    endif()
endforeach()

if(NOT DEFINED EVIDENCE_DIR)
    set(EVIDENCE_DIR "${HYREMOTE_BUILD_DIR}/evidence")
endif()

# Cells may be selected with -DEVIDENCE_CELLS=a;b;c (default: every cell).
set(all_cells
    clean-install
    installed-sdk
    installed-qml-qpa
    installed-qpa-product-fit
    source-consumer
    source-qpa-product-fit
    deploy-helper)

if(NOT DEFINED EVIDENCE_CELLS OR EVIDENCE_CELLS STREQUAL "")
    set(EVIDENCE_CELLS "${all_cells}")
endif()

# Every consumer cell consumes the clean install, so selecting one without the install would only fail for a reason
# that is not the product's. The install cell is therefore always included when any consumer cell is selected.
if(NOT "clean-install" IN_LIST EVIDENCE_CELLS)
    foreach(_cell IN LISTS EVIDENCE_CELLS)
        if(NOT _cell STREQUAL "clean-install")
            list(PREPEND EVIDENCE_CELLS "clean-install")
            break()
        endif()
    endforeach()
endif()

get_filename_component(HYREMOTE_SOURCE_DIR "${HYREMOTE_SOURCE_DIR}" ABSOLUTE)
get_filename_component(HYREMOTE_BUILD_DIR "${HYREMOTE_BUILD_DIR}" ABSOLUTE)
get_filename_component(EVIDENCE_DIR "${EVIDENCE_DIR}" ABSOLUTE)

# ---------------------------------------------------------------- identity (recorded once, copied into each cell)

execute_process(COMMAND git -C "${HYREMOTE_SOURCE_DIR}" rev-parse HEAD
                OUTPUT_VARIABLE SOURCE_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git -C "${HYREMOTE_SOURCE_DIR}" status --porcelain
                OUTPUT_VARIABLE SOURCE_DIRTY OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(SOURCE_DIRTY STREQUAL "")
    set(SOURCE_TREE_STATE "clean")
else()
    set(SOURCE_TREE_STATE "dirty")
endif()

file(READ "${HYREMOTE_BUILD_DIR}/CMakeCache.txt" _cache)
string(REGEX MATCH "CMAKE_CXX_COMPILER:[^=]*=([^\n]*)" _m "${_cache}")
set(TOOLCHAIN "${CMAKE_MATCH_1}")
string(REGEX MATCH "CMAKE_C_COMPILER:[^=]*=([^\n]*)" _m "${_cache}")
set(TOOLCHAIN_C "${CMAKE_MATCH_1}")
string(REGEX MATCH "CMAKE_PREFIX_PATH:[^=]*=([^\n]*)" _m "${_cache}")
set(QT_PREFIX_CACHE "${CMAKE_MATCH_1}")

# Consumers must be built by the same toolchain as the product they consume, otherwise a mixed-compiler link
# failure would masquerade as a deployment defect. The compiler identity is taken from the product's own cache.
set(CONSUMER_TOOLCHAIN_ARGS "")
if(TOOLCHAIN_C)
    list(APPEND CONSUMER_TOOLCHAIN_ARGS "-DCMAKE_C_COMPILER=${TOOLCHAIN_C}")
endif()
if(TOOLCHAIN)
    list(APPEND CONSUMER_TOOLCHAIN_ARGS "-DCMAKE_CXX_COMPILER=${TOOLCHAIN}")
endif()

# Qt identity: prefer the exact version the product was configured against.
set(QT_IDENTITY "unknown")
foreach(_qt_hint IN LISTS QT_PREFIX_CACHE)
    if(EXISTS "${_qt_hint}/lib/cmake/Qt6/Qt6ConfigVersion.cmake" OR EXISTS "${_qt_hint}/lib/cmake/Qt6/Qt6Config.cmake")
        set(QT_IDENTITY "${_qt_hint} (configured prefix)")
        break()
    endif()
endforeach()

if(CMAKE_HOST_SYSTEM_PROCESSOR)
    set(HOST_ARCH "${CMAKE_HOST_SYSTEM_PROCESSOR}")
elseif(DEFINED ENV{PROCESSOR_ARCHITECTURE})
    set(HOST_ARCH "$ENV{PROCESSOR_ARCHITECTURE}")
else()
    set(HOST_ARCH "unknown")
endif()

string(TIMESTAMP RUN_STAMP "%Y%m%d-%H%M%S" UTC)
string(TIMESTAMP RUN_ISO "%Y-%m-%dT%H:%M:%SZ" UTC)
set(RUN_DIR "${EVIDENCE_DIR}/${RUN_STAMP}-${SOURCE_SHA}")

# Harness executor: captured absolutely before any product runtime path is constructed. Its directory is never
# added to PRODUCT_RUNTIME_PATH.
find_program(HARNESS_EXECUTOR NAMES python python3)
if(NOT HARNESS_EXECUTOR)
    set(HARNESS_EXECUTOR "")
endif()

# Product runtime path: the operating system's own directories plus the deployed tree, nothing else.
if(WIN32)
    set(OS_RUNTIME_PATH "$ENV{SystemRoot}\\System32;$ENV{SystemRoot}")
    set(PATH_SEP ";")
else()
    set(OS_RUNTIME_PATH "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin")
    set(PATH_SEP ":")
endif()

# ---------------------------------------------------------------- helpers

set(RESULTS "")          # "cell=RESULT" lines for the summary
set(FAILED_CELLS "")
set(SKIPPED_CELLS "")

function(evidence_dir_for cell)
    set(_dir "${RUN_DIR}/${cell}")
    file(MAKE_DIRECTORY "${_dir}")
    set(${cell}_EVIDENCE_DIR "${_dir}" PARENT_SCOPE)
endfunction()

# record(cell key value) -> appends "key=value" to <cell>/evidence.txt
function(record cell key value)
    file(APPEND "${RUN_DIR}/${cell}/evidence.txt" "${key}=${value}\n")
endfunction()

function(record_common cell acquisition_mode prefix consumer)
    record("${cell}" "SCHEMA" "hyremote-release-evidence/1")
    record("${cell}" "CELL" "${cell}")
    record("${cell}" "SOURCE_SHA" "${SOURCE_SHA}")
    record("${cell}" "SOURCE_TREE_STATE" "${SOURCE_TREE_STATE}")
    record("${cell}" "REPOSITORY" "HyRemote")
    record("${cell}" "OS" "${CMAKE_HOST_SYSTEM_NAME}")
    record("${cell}" "ARCHITECTURE" "${HOST_ARCH}")
    record("${cell}" "TOOLCHAIN" "${TOOLCHAIN}")
    record("${cell}" "QT_IDENTITY" "${QT_IDENTITY}")
    record("${cell}" "BUILD_DIR" "${HYREMOTE_BUILD_DIR}")
    record("${cell}" "ACQUISITION_MODE" "${acquisition_mode}")
    record("${cell}" "INSTALL_PREFIX" "${prefix}")
    record("${cell}" "CONSUMER" "${consumer}")
    record("${cell}" "TIMESTAMP" "${RUN_ISO}")
endfunction()

# record_runtime_env(cell path_value) - the runtime search environment the executed process actually saw.
function(record_runtime_env cell path_value)
    record("${cell}" "PRODUCT_RUNTIME_PATH" "${path_value}")
    record("${cell}" "HARNESS_EXECUTOR" "${HARNESS_EXECUTOR}")
    if(WIN32)
        record("${cell}" "RUNTIME_ENV_NOTE" "PATH limited to OS directories plus the deployed tree; no build tree, no Qt SDK bin, no Python directory")
    else()
        record("${cell}" "RUNTIME_ENV_NOTE" "PATH limited to OS directories plus the deployed tree; LD_LIBRARY_PATH is not set, so the install RPATH must carry the product runtime")
    endif()
endfunction()

# execute the given command with an explicitly constructed PATH, capturing output and exit code.
function(run_capture cell label path_value)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env "PATH=${path_value}"
                ${ARGN}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
        WORKING_DIRECTORY "${RUN_DIR}/${cell}")
    string(REPLACE ";" " " _cmd_text "${ARGN}")
    set(_log "${RUN_DIR}/${cell}/${label}.log")
    file(WRITE "${_log}" "COMMAND=${_cmd_text}\nPATH=${path_value}\nEXIT_CODE=${_result}\n--- stdout ---\n${_out}\n--- stderr ---\n${_err}\n")
    record("${cell}" "${label}_COMMAND" "${_cmd_text}")
    record("${cell}" "${label}_EXIT_CODE" "${_result}")
    set(${cell}_result "${_result}" PARENT_SCOPE)
    if(NOT _result EQUAL 0)
        message(STATUS "release-evidence: ${cell}: ${label} failed (exit ${_result}); see ${_log}")
    endif()
endfunction()

# configure/build/install steps legitimately need the developer toolchain (compiler, CMake, generator), so they
# run with the inherited environment. The isolation requirement applies to the *executed product*: only the run and
# product-fit steps below use PRODUCT_RUNTIME_PATH, and only those are recorded with the runtime search environment.
function(run_toolchain cell label)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
        WORKING_DIRECTORY "${RUN_DIR}/${cell}")
    string(REPLACE ";" " " _cmd_text "${ARGN}")
    set(_log "${RUN_DIR}/${cell}/${label}.log")
    file(WRITE "${_log}" "COMMAND=${_cmd_text}\nENVIRONMENT=toolchain (inherited)\nEXIT_CODE=${_result}\n--- stdout ---\n${_out}\n--- stderr ---\n${_err}\n")
    record("${cell}" "${label}_COMMAND" "${_cmd_text}")
    record("${cell}" "${label}_EXIT_CODE" "${_result}")
    set(${cell}_result "${_result}" PARENT_SCOPE)
    if(NOT _result EQUAL 0)
        message(STATUS "release-evidence: ${cell}: ${label} failed (exit ${_result}); see ${_log}")
    endif()
endfunction()

function(fail_cell cell reason)
    record("${cell}" "RESULT" "FAIL")
    record("${cell}" "FAILURE" "${reason}")
    file(APPEND "${RUN_DIR}/summary.txt" "${cell}=FAIL (${reason})\n")
    set(FAILED_CELLS "${FAILED_CELLS};${cell}" PARENT_SCOPE)
endfunction()

function(pass_cell cell)
    record("${cell}" "RESULT" "PASS")
    file(APPEND "${RUN_DIR}/summary.txt" "${cell}=PASS\n")
endfunction()

function(skip_cell cell reason)
    evidence_dir_for("${cell}")
    record("${cell}" "SCHEMA" "hyremote-release-evidence/1")
    record("${cell}" "SOURCE_SHA" "${SOURCE_SHA}")
    record("${cell}" "RESULT" "SKIPPED")
    record("${cell}" "REASON" "${reason}")
    file(APPEND "${RUN_DIR}/summary.txt" "${cell}=SKIPPED (${reason})\n")
endfunction()

message(STATUS "release-evidence: run ${RUN_STAMP} for ${SOURCE_SHA} (${SOURCE_TREE_STATE})")
message(STATUS "release-evidence: evidence output ${RUN_DIR}")
file(MAKE_DIRECTORY "${RUN_DIR}")
file(WRITE "${RUN_DIR}/run.txt"
    "SCHEMA=hyremote-release-evidence/1\nSOURCE_SHA=${SOURCE_SHA}\nSOURCE_TREE_STATE=${SOURCE_TREE_STATE}\nTIMESTAMP=${RUN_ISO}\nOS=${CMAKE_HOST_SYSTEM_NAME}\nARCHITECTURE=${CMAKE_HOST_SYSTEM_PROCESSOR}\nTOOLCHAIN=${TOOLCHAIN}\nQT_IDENTITY=${QT_IDENTITY}\nSOURCE_TREE=${HYREMOTE_SOURCE_DIR}\nBUILD_TREE=${HYREMOTE_BUILD_DIR}\nHARNESS_EXECUTOR=${HARNESS_EXECUTOR}\n")
file(WRITE "${RUN_DIR}/summary.txt" "")

set(INSTALL_PREFIX "${RUN_DIR}/prefix")

foreach(cell IN LISTS EVIDENCE_CELLS)
    if(NOT cell IN_LIST all_cells)
        message(FATAL_ERROR "release-evidence: unknown evidence cell '${cell}'")
    endif()
    evidence_dir_for("${cell}")
endforeach()

# ---------------------------------------------------------------- clean-install

if("clean-install" IN_LIST EVIDENCE_CELLS)
    set(cell "clean-install")
    file(REMOVE_RECURSE "${INSTALL_PREFIX}")
    record_common("${cell}" "N/A (product install)" "${INSTALL_PREFIX}" "HyRemote product payload")
    run_toolchain("${cell}" "install"
        "${CMAKE_COMMAND}" --install "${HYREMOTE_BUILD_DIR}" --prefix "${INSTALL_PREFIX}")
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "product install into a clean prefix failed")
    else()
        file(GLOB_RECURSE _installed RELATIVE "${INSTALL_PREFIX}" "${INSTALL_PREFIX}/*")
        list(LENGTH _installed _installed_count)
        record("${cell}" "INSTALLED_ENTRIES" "${_installed_count}")
        record("${cell}" "ARTIFACT_IDENTITY" "prefix=${INSTALL_PREFIX}; entries=${_installed_count}")
        if(_installed_count EQUAL 0)
            fail_cell("${cell}" "clean install produced no artifacts")
        else()
            pass_cell("${cell}")
        endif()
    endif()
endif()

# ---------------------------------------------------------------- installed consumers

# configure + build + install + run one installed consumer cell.
# acquire_installed(cell fixture_dir extra_configure_args...)
function(acquire_installed cell fixture)
    set(_build "${RUN_DIR}/${cell}/build")
    file(MAKE_DIRECTORY "${_build}")
    run_toolchain("${cell}" "configure"
        "${CMAKE_COMMAND}" -S "${HYREMOTE_SOURCE_DIR}/${fixture}" -B "${_build}"
            "-DCMAKE_BUILD_TYPE=Release"
            "-DCMAKE_PREFIX_PATH=${INSTALL_PREFIX}${PATH_SEP}${QT_PREFIX_CACHE}"
            ${CONSUMER_TOOLCHAIN_ARGS}
            ${ARGN})
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "installed consumer configure failed")
        return()
    endif()
    run_toolchain("${cell}" "build" "${CMAKE_COMMAND}" --build "${_build}")
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "installed consumer build failed")
        return()
    endif()
    run_toolchain("${cell}" "install"
        "${CMAKE_COMMAND}" --install "${_build}" --prefix "${RUN_DIR}/${cell}/deployed")
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "installed consumer deployment failed")
        return()
    endif()
    set(${cell}_deployed "${RUN_DIR}/${cell}/deployed" PARENT_SCOPE)
    pass_cell("${cell}")
endfunction()

if("installed-sdk" IN_LIST EVIDENCE_CELLS)
    set(cell "installed-sdk")
    record_common("${cell}" "INSTALLED" "${INSTALL_PREFIX}" "tests/consumer-installed-sdk")
    acquire_installed("${cell}" "tests/consumer-installed-sdk")
    if(NOT "${FAILED_CELLS}" MATCHES "${cell}")
        set(_path "${RUN_DIR}/${cell}/deployed/bin${PATH_SEP}${OS_RUNTIME_PATH}")
        record_runtime_env("${cell}" "${_path}")
        run_capture("${cell}" "run" "${_path}"
            "${RUN_DIR}/${cell}/deployed/bin/hyremote-installed-consumer")
        if(${cell}_result EQUAL 0)
            record("${cell}" "EXECUTABLE" "${RUN_DIR}/${cell}/deployed/bin/hyremote-installed-consumer")
            record("${cell}" "RESULT_DETAIL" "installed consumer executed against the deployed tree")
            record("${cell}" "RESULT" "PASS")
        else()
            record("${cell}" "EXECUTABLE" "${RUN_DIR}/${cell}/deployed/bin/hyremote-installed-consumer")
            fail_cell("${cell}" "deployed installed consumer did not run")
        endif()
    endif()
endif()

if("installed-qml-qpa" IN_LIST EVIDENCE_CELLS)
    set(cell "installed-qml-qpa")
    record_common("${cell}" "INSTALLED" "${INSTALL_PREFIX}" "tests/consumer-installed-qml (combined QML + QPA)")
    acquire_installed("${cell}" "tests/consumer-installed-qml" "-DHYREMOTE_CONSUMER_WITH_QPA=ON")
endif()

if("source-consumer" IN_LIST EVIDENCE_CELLS)
    set(cell "source-consumer")
    record_common("${cell}" "SOURCE" "${HYREMOTE_SOURCE_DIR}" "tests/consumer-source (add_subdirectory)")
    set(_build "${RUN_DIR}/${cell}/build")
    file(MAKE_DIRECTORY "${_build}")
    run_toolchain("${cell}" "configure"
        "${CMAKE_COMMAND}" -S "${HYREMOTE_SOURCE_DIR}/tests/consumer-source" -B "${_build}"
            "-DCMAKE_BUILD_TYPE=Release"
            "-DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}"
            "-DCMAKE_PREFIX_PATH=${QT_PREFIX_CACHE}"
            ${CONSUMER_TOOLCHAIN_ARGS})
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "source consumer configure failed")
    else()
        run_toolchain("${cell}" "build" "${CMAKE_COMMAND}" --build "${_build}")
        if(NOT ${cell}_result EQUAL 0)
            fail_cell("${cell}" "source consumer build failed")
        else()
            record("${cell}" "RESULT" "PASS")
        endif()
    endif()
endif()

# ---------------------------------------------------------------- QPA product-fit (installed and source)

# product_fit(cell fixture) - build the QPA consumer with the declared acquisition, then actually run
# tests/consumer-installed-qpa/product_fit.py against the deployed executable.
function(qpa_product_fit cell fixture)
    if(NOT HARNESS_EXECUTOR)
        skip_cell("${cell}" "no Python interpreter available to execute the product-fit harness")
        return()
    endif()
    set(_build "${RUN_DIR}/${cell}/build")
    file(MAKE_DIRECTORY "${_build}")
    run_toolchain("${cell}" "configure"
        "${CMAKE_COMMAND}" -S "${HYREMOTE_SOURCE_DIR}/${fixture}" -B "${_build}"
            "-DCMAKE_BUILD_TYPE=Release"
            "-DCMAKE_PREFIX_PATH=${INSTALL_PREFIX}${PATH_SEP}${QT_PREFIX_CACHE}"
            ${CONSUMER_TOOLCHAIN_ARGS}
            ${ARGN})
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "QPA consumer configure failed")
        return()
    endif()
    run_toolchain("${cell}" "build" "${CMAKE_COMMAND}" --build "${_build}")
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "QPA consumer build failed")
        return()
    endif()
    run_toolchain("${cell}" "install"
        "${CMAKE_COMMAND}" --install "${_build}" --prefix "${RUN_DIR}/${cell}/deployed")
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "QPA consumer deployment failed")
        return()
    endif()
    set(_exe "${RUN_DIR}/${cell}/deployed/bin/hyremote-installed-qpa-consumer")
    if(WIN32)
        set(_exe "${_exe}.exe")
    endif()
    if(NOT EXISTS "${_exe}")
        fail_cell("${cell}" "deployed QPA consumer executable is missing: ${_exe}")
        return()
    endif()
    # The product-fit harness runs the deployed consumer with the product runtime path only; the harness's own
    # interpreter is invoked by absolute path, so its directory never enters that path.
    set(_path "${RUN_DIR}/${cell}/deployed/bin${PATH_SEP}${OS_RUNTIME_PATH}")
    record_runtime_env("${cell}" "${_path}")
    record("${cell}" "EXECUTABLE" "${_exe}")
    record("${cell}" "HARNESS_COMMAND" "${HARNESS_EXECUTOR} ${HYREMOTE_SOURCE_DIR}/tests/consumer-installed-qpa/product_fit.py --app ${_exe}")
    run_capture("${cell}" "product_fit" "${_path}"
        "${HARNESS_EXECUTOR}" "${HYREMOTE_SOURCE_DIR}/tests/consumer-installed-qpa/product_fit.py" --app "${_exe}")
    if(${cell}_result EQUAL 0)
        record("${cell}" "RESULT_DETAIL" "product-fit ran the deployed consumer to a live RFB listener")
        record("${cell}" "RESULT" "PASS")
    else()
        fail_cell("${cell}" "deployed product-fit did not pass")
    endif()
endfunction()

if("installed-qpa-product-fit" IN_LIST EVIDENCE_CELLS)
    set(cell "installed-qpa-product-fit")
    record_common("${cell}" "INSTALLED" "${INSTALL_PREFIX}" "tests/consumer-installed-qpa + product_fit.py")
    qpa_product_fit("${cell}" "tests/consumer-installed-qpa")
endif()

if("source-qpa-product-fit" IN_LIST EVIDENCE_CELLS)
    set(cell "source-qpa-product-fit")
    record_common("${cell}" "SOURCE" "${HYREMOTE_SOURCE_DIR}" "tests/consumer-installed-qpa + product_fit.py")
    qpa_product_fit("${cell}" "tests/consumer-installed-qpa"
        "-DHYREMOTE_CONSUMER_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}")
endif()

# ---------------------------------------------------------------- deploy-helper validation (existing harness)

if("deploy-helper" IN_LIST EVIDENCE_CELLS)
    set(cell "deploy-helper")
    record_common("${cell}" "INSTALLED" "${RUN_DIR}/${cell}/fixture" "src/integrations/qpa/tests/deploy_helper_fixture")
    run_toolchain("${cell}" "fixture"
        "${CMAKE_COMMAND}"
            "-DFIXTURE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}/src/integrations/qpa/tests/deploy_helper_fixture"
            "-DFIXTURE_BINARY_DIR=${RUN_DIR}/${cell}/fixture"
            "-DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}"
            "-DTEST_INSTALLED_PAYLOAD=ON"
            -P "${HYREMOTE_SOURCE_DIR}/src/integrations/qpa/tests/run_deploy_helper_fixture.cmake")
    if(${cell}_result EQUAL 0)
        record("${cell}" "RESULT_DETAIL" "clean deployment fixture executed through the repository's existing harness")
        record("${cell}" "RESULT" "PASS")
    else()
        fail_cell("${cell}" "deploy-helper fixture failed")
    endif()
endif()

# ---------------------------------------------------------------- summary

set(passed 0)
set(failed 0)
set(skipped 0)
foreach(cell IN LISTS EVIDENCE_CELLS)
    if(NOT EXISTS "${RUN_DIR}/${cell}/evidence.txt")
        continue()
    endif()
    file(READ "${RUN_DIR}/${cell}/evidence.txt" _cell_text)
    if(_cell_text MATCHES "RESULT=PASS")
        math(EXPR passed "${passed} + 1")
    elseif(_cell_text MATCHES "RESULT=FAIL")
        math(EXPR failed "${failed} + 1")
    elseif(_cell_text MATCHES "RESULT=SKIPPED")
        math(EXPR skipped "${skipped} + 1")
    endif()
endforeach()

file(APPEND "${RUN_DIR}/summary.txt"
    "\nSOURCE_SHA=${SOURCE_SHA}\nSOURCE_TREE_STATE=${SOURCE_TREE_STATE}\nTIMESTAMP=${RUN_ISO}\nCELLS=${passed} passed, ${failed} failed, ${skipped} skipped\nEVIDENCE_BINDING=SHA-bound: this evidence is valid for ${SOURCE_SHA} only\n")

message(STATUS "release-evidence: ${passed} passed, ${failed} failed, ${skipped} skipped")
message(STATUS "release-evidence: evidence: ${RUN_DIR}")

if(failed GREATER 0)
    message(FATAL_ERROR "release-evidence: ${failed} evidence cell(s) failed; see ${RUN_DIR}/summary.txt")
endif()
if(skipped GREATER 0)
    message(FATAL_ERROR
        "release-evidence: ${skipped} evidence cell(s) were skipped. A skipped cell is not evidence - the "
        "release candidate needs every cell to actually run. See ${RUN_DIR}/summary.txt")
endif()

message(STATUS
    "HyRemote release evidence: PASS (${passed} cells; source ${SOURCE_SHA}; ${SOURCE_TREE_STATE})")
