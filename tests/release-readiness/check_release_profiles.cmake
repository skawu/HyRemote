cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

# One table for every release-profile case. Each row is
# name|version|cpp|qml|generic|qpa|security|expect_reject.
# Keeping one CTest process avoids ten nearly identical CMake children while
# preserving each case's accept/reject meaning and a named PASS/FAIL line.
set(profile_cases
    "develop-all|0.0.0|ON|ON|ON|ON|ON|FALSE"
    "develop-runtime-only|0.0.0|OFF|OFF|OFF|OFF|OFF|FALSE"
    "retire-v001|0.0.1.0|ON|OFF|OFF|OFF|OFF|TRUE"
    "retire-v002|0.0.2.0|ON|ON|OFF|OFF|OFF|TRUE"
    "retire-v003|0.0.3.0|ON|ON|OFF|ON|OFF|TRUE"
    "v010-cpp-only|0.1.0.0|ON|OFF|OFF|OFF|OFF|FALSE"
    "v020-runtime|0.2.0.0|ON|OFF|OFF|OFF|OFF|FALSE"
    "v030-all|0.3.0.0|ON|ON|ON|ON|ON|FALSE"
    "v100-all|1.0.0.0|ON|ON|ON|ON|ON|FALSE"
    "v100-cpp-only|1.0.0.0|ON|OFF|OFF|OFF|OFF|FALSE"
    "v100-generic-only|1.0.0.0|OFF|OFF|ON|OFF|OFF|FALSE"
)

foreach(case IN LISTS profile_cases)
    string(REPLACE "|" ";" fields "${case}")
    list(GET fields 0 case_name)
    list(GET fields 1 version)
    list(GET fields 2 cpp_enabled)
    list(GET fields 3 qml_enabled)
    list(GET fields 4 generic_enabled)
    list(GET fields 5 qpa_enabled)
    list(GET fields 6 security_enabled)
    list(GET fields 7 expect_reject)

    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}
            -DHYREMOTE_TEST_VERSION=${version}
            -DHYREMOTE_TEST_CPP=${cpp_enabled}
            -DHYREMOTE_TEST_QML=${qml_enabled}
            -DHYREMOTE_TEST_GENERIC=${generic_enabled}
            -DHYREMOTE_TEST_QPA=${qpa_enabled}
            -DHYREMOTE_TEST_SECURITY=${security_enabled}
            -P "${HYREMOTE_SOURCE_DIR}/tests/release-readiness/check_release_profile.cmake"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )

    if(expect_reject)
        if(result EQUAL 0)
            message(FATAL_ERROR
                "release-profile case '${case_name}' was expected to be rejected but passed: ${output}")
        endif()
    else()
        if(NOT result EQUAL 0)
            message(FATAL_ERROR
                "release-profile case '${case_name}' was expected to pass but failed: ${output}${error}")
        endif()
    endif()
    message(STATUS "release-profile case PASS: ${case_name}")
endforeach()

message(STATUS "release-profile table PASS: ${profile_cases}")
