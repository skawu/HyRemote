cmake_minimum_required(VERSION 3.21)

foreach(required_var
        FIXTURE_SOURCE_DIR
        FIXTURE_BINARY_ROOT
        FIXTURE_RUNNER
        HYREMOTE_SOURCE_DIR)
    if(NOT DEFINED ${required_var})
        message(FATAL_ERROR "${required_var} is required")
    endif()
endforeach()

# Positive deploy-helper matrix. Each case keeps its own fixture binary tree and
# its own runner invocation; only the CTest process bookkeeping is shared.
function(run_case name)
    set(args ${ARGN})
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${args} -P "${FIXTURE_RUNNER}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "deploy-helper positive case '${name}' failed (${result}):\n${output}\n${error}")
    endif()
    message(STATUS "deploy-helper positive case PASS: ${name}")
endfunction()

run_case(ordinary
    -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
    -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/ordinary
    -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
    -DTEST_QML=OFF
)

run_case(qml-only
    -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
    -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/qml-only
    -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
    -DTEST_QML=ON
    -DTEST_DEPLOY_QPA=OFF
)

run_case(qml-composed
    -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
    -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/qml-composed
    -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
    -DTEST_QML=ON
    -DTEST_DEPLOY_QPA=ON
)

run_case(installed-payload
    -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
    -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/installed-payload
    -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
    -DTEST_QML=OFF
    -DTEST_INSTALLED_PAYLOAD=ON
)

run_case(installed-payload-qml-only
    -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
    -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/installed-payload-qml-only
    -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
    -DTEST_QML=ON
    -DTEST_DEPLOY_QPA=OFF
    -DTEST_INSTALLED_PAYLOAD=ON
)

run_case(installed-payload-qml
    -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
    -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/installed-payload-qml
    -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
    -DTEST_QML=ON
    -DTEST_DEPLOY_QPA=ON
    -DTEST_INSTALLED_PAYLOAD=ON
)

if(FIXTURE_NINJA)
    run_case(single-config-generator
        -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
        -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/single-config-generator
        -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
        -DFIXTURE_GENERATOR=Ninja
        -DFIXTURE_MAKE_PROGRAM=${FIXTURE_NINJA}
        -DFIXTURE_BUILD_TYPE=Release
        -DFIXTURE_EXPECTED_CONFIG=Release
        -DTEST_QML=OFF
    )
    run_case(multi-config-generator
        -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
        -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/multi-config-generator
        -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
        "-DFIXTURE_GENERATOR=Ninja Multi-Config"
        -DFIXTURE_MAKE_PROGRAM=${FIXTURE_NINJA}
        -DFIXTURE_EXPECTED_CONFIG=Release
        -DTEST_QML=OFF
    )
else()
    message(STATUS
        "deploy-helper positive case SKIP: generator cases (ninja was not found)")
endif()
