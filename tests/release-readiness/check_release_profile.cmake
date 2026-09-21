cmake_minimum_required(VERSION 3.21)

foreach(required_var
        HYREMOTE_SOURCE_DIR
        HYREMOTE_TEST_VERSION
        HYREMOTE_TEST_CPP
        HYREMOTE_TEST_QML
        HYREMOTE_TEST_GENERIC
        HYREMOTE_TEST_QPA
        HYREMOTE_TEST_SECURITY)
    if(NOT DEFINED ${required_var})
        message(FATAL_ERROR "${required_var} is required")
    endif()
endforeach()

list(APPEND CMAKE_MODULE_PATH "${HYREMOTE_SOURCE_DIR}/cmake")
include(HyRemoteReleaseProfile)

hyremote_validate_release_profile(
    VERSION "${HYREMOTE_TEST_VERSION}"
    CPP_ENABLED "${HYREMOTE_TEST_CPP}"
    QML_ENABLED "${HYREMOTE_TEST_QML}"
    GENERIC_ENABLED "${HYREMOTE_TEST_GENERIC}"
    QPA_ENABLED "${HYREMOTE_TEST_QPA}"
    SECURITY_ENABLED "${HYREMOTE_TEST_SECURITY}"
)

message(STATUS
    "HyRemote release-profile check passed: version=${HYREMOTE_TEST_VERSION}, "
    "cpp=${HYREMOTE_TEST_CPP}, qml=${HYREMOTE_TEST_QML}, generic=${HYREMOTE_TEST_GENERIC}, "
    "qpa=${HYREMOTE_TEST_QPA}, security=${HYREMOTE_TEST_SECURITY}")
