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

# Generic is a primary V0.1 surface, so its installed consumption is part of the evidence set rather than an
# optional extra: the two consumers are Widgets and Qt Quick, and both must actually run after deployment.
list(APPEND all_cells
    installed-generic-widgets
    installed-generic-quick)

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
    set(RUNTIME_PATH_SEP ";")
else()
    set(OS_RUNTIME_PATH "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin")
    set(RUNTIME_PATH_SEP ":")
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
        # Echo this step's own log body - and only the failing step's, never the successful ones - so the chain is
        # readable end to end: the ctest log, the echo the build system performs on failure, the uploaded artifact
        # and the evidence summary all carry the real command, output and exit code instead of a pointer to a file
        # that nothing outside the runner can open.
        file(READ "${_log}" _failed_step_log)
        string(STRIP "${_failed_step_log}" _failed_step_log)
        message(STATUS "--- begin ${label}.log ---")
        message(STATUS "${_failed_step_log}")
        message(STATUS "--- end ${label}.log ---")
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
        # Echo this step's own log body - and only the failing step's, never the successful ones - so the chain is
        # readable end to end: the ctest log, the echo the build system performs on failure, the uploaded artifact
        # and the evidence summary all carry the real command, output and exit code instead of a pointer to a file
        # that nothing outside the runner can open.
        file(READ "${_log}" _failed_step_log)
        string(STRIP "${_failed_step_log}" _failed_step_log)
        message(STATUS "--- begin ${label}.log ---")
        message(STATUS "${_failed_step_log}")
        message(STATUS "--- end ${label}.log ---")
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

# CMAKE_PREFIX_PATH is a CMake **list**, so its entries are separated by the CMake list separator and never by the
# operating system's runtime search-path separator. The two differ on Linux (':' versus ';'), and reusing the
# runtime separator here produced a single bogus entry such as "<install-prefix>:<qt-prefix>": the consumer's
# find_package(HyRemote) and find_package(Qt6) then failed before it could configure at all, while the identical
# code passed on Windows only because there both separators happen to be ';'. The value crosses two expansions -
# the runner's ARGN command list and the -D argument itself - so the list separator is escaped here and unescaped
# once more on the way to the child process.
set(_consumer_prefix_list "${INSTALL_PREFIX};${QT_PREFIX_CACHE}")
string(REPLACE ";" "\\;" _consumer_prefix_list "${_consumer_prefix_list}")

# The consumer projects are built in one named configuration, and whether that is a CMake *build type* (single
# configuration generator) or a *configuration* (multi-configuration generator) depends on the generator the
# consumer ends up with, which the harness does not choose. A multi-config generator writes artifacts into a
# per-configuration directory and then needs --config on build and install alike: passing it to neither, as this
# harness did, makes the install step look for build/Release/<consumer> when the build put the binary somewhere
# else, which is what the hosted Windows lane reported. Single-configuration generators accept --config as well and
# ignore it, so the argument is declared once and passed to both steps rather than to one of them: configure, build
# and install therefore cannot disagree about which configuration is being exercised.
set(CONSUMER_CONFIGURATION "Release")
set(CONSUMER_CONFIG_ARGS --config "${CONSUMER_CONFIGURATION}")

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
            "-DCMAKE_PREFIX_PATH=${_consumer_prefix_list}"
            ${CONSUMER_TOOLCHAIN_ARGS}
            ${ARGN})
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "installed consumer configure failed")
        return()
    endif()
    run_toolchain("${cell}" "build" "${CMAKE_COMMAND}" --build "${_build}" ${CONSUMER_CONFIG_ARGS})
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "installed consumer build failed")
        return()
    endif()
    run_toolchain("${cell}" "install"
        "${CMAKE_COMMAND}" --install "${_build}" --prefix "${RUN_DIR}/${cell}/deployed" ${CONSUMER_CONFIG_ARGS})
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
        set(_path "${RUN_DIR}/${cell}/deployed/bin${RUNTIME_PATH_SEP}${OS_RUNTIME_PATH}")
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
        run_toolchain("${cell}" "build" "${CMAKE_COMMAND}" --build "${_build}" ${CONSUMER_CONFIG_ARGS})
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
            "-DCMAKE_PREFIX_PATH=${_consumer_prefix_list}"
            ${CONSUMER_TOOLCHAIN_ARGS}
            ${ARGN})
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "QPA consumer configure failed")
        return()
    endif()
    run_toolchain("${cell}" "build" "${CMAKE_COMMAND}" --build "${_build}" ${CONSUMER_CONFIG_ARGS})
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "QPA consumer build failed")
        return()
    endif()
    run_toolchain("${cell}" "install"
        "${CMAKE_COMMAND}" --install "${_build}" --prefix "${RUN_DIR}/${cell}/deployed" ${CONSUMER_CONFIG_ARGS})
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
    set(_path "${RUN_DIR}/${cell}/deployed/bin${RUNTIME_PATH_SEP}${OS_RUNTIME_PATH}")
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

# ---------------------------------------------------------------- Generic installed consumption

# generic_product_fit(cell consumer_target) - build the Generic consumers against the clean install, deploy them with
# the same helper every other payload uses, and run the deployed executable with only the deployed runtime on its
# search path. Two facts are asserted, and they stay separate: the deployed tree carries the Generic payload in Qt's
# generic plugin directory and no HyRemote platform plugin, and the application itself reports both that it is still
# on a Qt-provided platform and that the plugin is discoverable.
function(generic_product_fit cell consumer_target)
    set(_build "${RUN_DIR}/${cell}/build")
    file(MAKE_DIRECTORY "${_build}")
    run_toolchain("${cell}" "configure"
        "${CMAKE_COMMAND}" -S "${HYREMOTE_SOURCE_DIR}/tests/consumer-installed-generic" -B "${_build}"
            "-DCMAKE_BUILD_TYPE=Release"
            "-DCMAKE_PREFIX_PATH=${_consumer_prefix_list}"
            ${CONSUMER_TOOLCHAIN_ARGS})
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "Generic consumer configure failed")
        return()
    endif()
    run_toolchain("${cell}" "build" "${CMAKE_COMMAND}" --build "${_build}" ${CONSUMER_CONFIG_ARGS})
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "Generic consumer build failed")
        return()
    endif()
    run_toolchain("${cell}" "install"
        "${CMAKE_COMMAND}" --install "${_build}" --prefix "${RUN_DIR}/${cell}/deployed" ${CONSUMER_CONFIG_ARGS})
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "Generic consumer deployment failed")
        return()
    endif()

    set(_deployed "${RUN_DIR}/${cell}/deployed")
    set(_exe "${_deployed}/bin/${consumer_target}")
    if(WIN32)
        set(_exe "${_exe}.exe")
    endif()
    if(NOT EXISTS "${_exe}")
        fail_cell("${cell}" "deployed Generic consumer executable is missing: ${_exe}")
        return()
    endif()

    # Generic is a plugin payload, never a platform plugin: the payload must be present below the generic plugin
    # directory, and a HyRemote platform plugin in the deployed tree would mean the native platform identity was
    # replaced rather than preserved.
    file(GLOB _generic_payloads "${_deployed}/plugins/generic/*")
    list(LENGTH _generic_payloads _generic_payload_count)
    if(_generic_payload_count EQUAL 0)
        fail_cell("${cell}" "deployed tree carries no Generic Plugin payload under plugins/generic")
        return()
    endif()
    file(GLOB _platform_payloads "${_deployed}/plugins/platforms/*hyremote*")
    list(LENGTH _platform_payloads _platform_payload_count)
    if(NOT _platform_payload_count EQUAL 0)
        fail_cell("${cell}" "Generic deployment installed a HyRemote platform plugin: ${_platform_payloads}")
        return()
    endif()
    record("${cell}" "GENERIC_PAYLOAD" "${_generic_payloads}")

    # Generic deployment has to ship both kinds of plugin, and neither stands in for the other: exactly one HyRemote
    # generic payload, and the native Qt platform plugin that keeps the application on its normal platform identity.
    # Without the native plugin the application cannot start outside the Qt SDK, so a tree that has the generic
    # payload but not the native one is not a clean deployment.
    file(GLOB _generic_hyremote_payloads "${_deployed}/plugins/generic/*hyremote*")
    list(LENGTH _generic_hyremote_payloads _generic_hyremote_count)
    if(NOT _generic_hyremote_count EQUAL 1)
        fail_cell("${cell}" "deployed tree must carry exactly one HyRemote generic plugin payload, found ${_generic_hyremote_count}")
        return()
    endif()
    file(GLOB _native_platform_payloads "${_deployed}/plugins/platforms/*")
    list(LENGTH _native_platform_payloads _native_platform_count)
    if(_native_platform_count EQUAL 0)
        fail_cell("${cell}" "deployed tree carries no native Qt platform plugin, so the application cannot start without the Qt SDK")
        return()
    endif()
    record("${cell}" "NATIVE_PLATFORM_PAYLOAD" "${_native_platform_payloads}")

    # Source/build-tree independence, counted rather than asserted in prose: the consumer must have acquired
    # HyRemote from the clean install prefix, so neither the repository source tree nor the product build tree
    # may appear in the consumer's configure cache, and the deployed tree must be self-contained.
    set(_source_tree_hits 0)
    set(_build_tree_hits 0)
    file(READ "${_build}/CMakeCache.txt" _consumer_cache)
    string(REPLACE "\\" "/" _consumer_cache "${_consumer_cache}")
    set(_source_probe "${HYREMOTE_SOURCE_DIR}")
    set(_build_probe "${HYREMOTE_BUILD_DIR}")
    string(REPLACE "\\" "/" _source_probe "${_source_probe}")
    string(REPLACE "\\" "/" _build_probe "${_build_probe}")
    # The evidence run itself lives inside the product build tree, so lines that are about this run's own
    # directories are not build-tree acquisition; everything else pointing at the product trees is.
    foreach(_cache_line IN LISTS _consumer_cache)
        string(FIND "${_cache_line}" "${RUN_DIR}" _run_dir_hit)
        if(NOT _run_dir_hit EQUAL -1)
            continue()
        endif()
        string(FIND "${_cache_line}" "${_source_probe}" _source_hit)
        if(NOT _source_hit EQUAL -1)
            math(EXPR _source_tree_hits "${_source_tree_hits} + 1")
        endif()
        string(FIND "${_cache_line}" "${_build_probe}" _build_hit)
        if(NOT _build_hit EQUAL -1)
            math(EXPR _build_tree_hits "${_build_tree_hits} + 1")
        endif()
    endforeach()

    # Positive form of the same fact: the package the consumer resolved is the clean install, not the build tree.
    string(FIND "${_consumer_cache}" "HyRemote_DIR:PATH=${INSTALL_PREFIX}" _install_acquisition_hit)
    if(_install_acquisition_hit EQUAL -1)
        fail_cell("${cell}" "installed Generic consumer did not resolve HyRemote from the clean install prefix")
        return()
    endif()

    # The same positive form for Qt, recorded so the acquisition is auditable: the consumer must resolve Qt6 from
    # the Qt prefix this lane selected, not from a system or accidental package.
    string(REGEX MATCH "Qt6_DIR:PATH=([^\n]*)" _qt_dir_line "${_consumer_cache}")
    if("${_qt_dir_line}" STREQUAL "")
        fail_cell("${cell}" "installed Generic consumer recorded no resolved Qt6 package directory")
        return()
    endif()
    string(FIND "${_qt_dir_line}" "${QT_PREFIX_CACHE}" _qt_prefix_hit)
    if(_qt_prefix_hit EQUAL -1)
        fail_cell("${cell}" "installed Generic consumer resolved Qt6 outside the requested Qt prefix: ${_qt_dir_line}")
        return()
    endif()
    record("${cell}" "QT6_DIR" "${_qt_dir_line}")

    record("${cell}" "SOURCE_TREE_DEPENDENCY_COUNT" "${_source_tree_hits}")
    record("${cell}" "BUILD_TREE_DEPENDENCY_COUNT" "${_build_tree_hits}")
    if(NOT _source_tree_hits EQUAL 0 OR NOT _build_tree_hits EQUAL 0)
        fail_cell("${cell}" "installed Generic consumer acquired HyRemote from the source or build tree")
        return()
    endif()

    # The deployed application runs with the deployed runtime on its search path only - no build tree, no source
    # tree, no SDK prefix - which is the same isolation the other consumer cells use.
    set(_path "${_deployed}/bin${RUNTIME_PATH_SEP}${OS_RUNTIME_PATH}")
    record_runtime_env("${cell}" "${_path}")
    record("${cell}" "EXECUTABLE" "${_exe}")
    run_capture("${cell}" "deployed_smoke" "${_path}" "${_exe}")
    if(NOT ${cell}_result EQUAL 0)
        fail_cell("${cell}" "deployed Generic consumer did not exit successfully")
        return()
    endif()
    record("${cell}" "RESULT_DETAIL" "deployed Generic consumer preserved native platform identity and discovered the plugin")
    record("${cell}" "RESULT" "PASS")
    # One auditable line per required consumer in the runner's own output, so a passing run still states which
    # consumers actually executed rather than leaving that to be inferred from a cell count.
    if(consumer_target STREQUAL "generic-widgets-consumer")
        message(STATUS "release-evidence: GENERIC_WIDGETS_INSTALLED=PASS (${_exe})")
    else()
        message(STATUS "release-evidence: GENERIC_QUICK_INSTALLED=PASS (${_exe})")
    endif()
endfunction()

if("installed-generic-widgets" IN_LIST EVIDENCE_CELLS)
    set(cell "installed-generic-widgets")
    record_common("${cell}" "INSTALLED" "${INSTALL_PREFIX}" "tests/consumer-installed-generic (Widgets, Qt-only)")
    generic_product_fit("${cell}" "generic-widgets-consumer")
endif()

if("installed-generic-quick" IN_LIST EVIDENCE_CELLS)
    set(cell "installed-generic-quick")
    record_common("${cell}" "INSTALLED" "${INSTALL_PREFIX}" "tests/consumer-installed-generic (Quick, Qt-only)")
    generic_product_fit("${cell}" "generic-quick-consumer")
endif()

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
