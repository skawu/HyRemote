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

# Negative deploy-helper matrix. The fixture runner asserts the expected
# configure failure and its message fragment, so a zero exit is the pass case.
function(run_reject_case name)
    set(args ${ARGN})
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${args} -P "${FIXTURE_RUNNER}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "deploy-helper rejection case '${name}' did not observe its expected failure (${result}):\n${output}\n${error}")
    endif()
    message(STATUS "deploy-helper rejection case PASS: ${name}")
endfunction()

run_reject_case(reject-missing-qml
    -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
    -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/reject-missing-qml
    -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
    -DTEST_QML=ON
    -DTEST_QML_AVAILABLE=OFF
    -DEXPECT_CONFIGURE_FAILURE=ON
    -DEXPECT_FAILURE_FRAGMENT=requires_a_HyRemote_QML_payload
)

run_reject_case(reject-stale-qml-metadata
    -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
    -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/reject-stale-qml-metadata
    -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
    -DTEST_QML=ON
    -DTEST_QML_AVAILABLE=OFF
    -DTEST_STALE_QML_METADATA=ON
    -DEXPECT_CONFIGURE_FAILURE=ON
    -DEXPECT_FAILURE_FRAGMENT=requires_a_HyRemote_QML_payload
)

run_reject_case(reject-missing-qml-root
    -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
    -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/reject-missing-qml-root
    -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
    -DTEST_QML=ON
    -DTEST_INSTALLED_PAYLOAD=ON
    -DTEST_QML_IMPORT_PATH_EXISTS=OFF
    -DEXPECT_CONFIGURE_FAILURE=ON
    -DEXPECT_FAILURE_FRAGMENT=QML_import_root_does_not_exist
)

run_reject_case(reject-missing-qml-module-dir
    -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
    -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/reject-missing-qml-module-dir
    -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
    -DTEST_QML=ON
    -DTEST_INSTALLED_PAYLOAD=ON
    -DTEST_QML_MODULE_DIR_EXISTS=OFF
    -DEXPECT_CONFIGURE_FAILURE=ON
    -DEXPECT_FAILURE_FRAGMENT=QML_module_directory_does_not_exist
)

run_reject_case(reject-stale-qpa-metadata
    -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
    -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/reject-stale-qpa-metadata
    -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
    -DTEST_QML=OFF
    -DTEST_STALE_QPA_METADATA=ON
    -DEXPECT_CONFIGURE_FAILURE=ON
    -DEXPECT_FAILURE_FRAGMENT=requires_the_current_HyRemote_source_build
)

run_reject_case(reject-qt-mismatch
    -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
    -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/reject-qt-mismatch
    -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
    -DTEST_QML=OFF
    -DTEST_QT_VERSION=6.8.2
    -DEXPECT_CONFIGURE_FAILURE=ON
    -DEXPECT_FAILURE_FRAGMENT=requires_exact_Qt_6.8.3
)

run_reject_case(reject-missing-package
    -DFIXTURE_SOURCE_DIR=${FIXTURE_SOURCE_DIR}
    -DFIXTURE_BINARY_DIR=${FIXTURE_BINARY_ROOT}/reject-missing-package
    -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
    -DTEST_QML=OFF
    -DTEST_INSTALLED_PAYLOAD=ON
    -DTEST_QPA_AVAILABLE=OFF
    -DEXPECT_CONFIGURE_FAILURE=ON
    -DEXPECT_FAILURE_FRAGMENT=requested_an_SDK_that_was_built_without_Transparent_QPA
)
