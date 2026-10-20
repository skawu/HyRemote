include(GNUInstallDirs)

# Transparent QPA — exact Qt 6.8.3 private-ABI package.
# Private QPA types remain isolated here while the payload reuses the common HyRemote runtime shared
# with the C++/QML/Generic integration frontends.
get_filename_component(_hyremote_root_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

# Application surface discovery/composition/input routing is owned by src/runtime. The QPA payload
# contains only QPA-specific Factory Trampoline bootstrap/configuration code and links the one shared runtime.
add_library(hyremote-qpa-platform MODULE
    hyremote_platform_plugin.cpp
    qpa_config.cpp
)
add_library(HyRemote::QpaPlatform ALIAS hyremote-qpa-platform)

set_target_properties(hyremote-qpa-platform PROPERTIES
    AUTOMOC ON
    OUTPUT_NAME qhyremote
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/plugins/platforms"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/plugins/platforms"
)

if(UNIX AND NOT APPLE)
    set_property(TARGET hyremote-qpa-platform PROPERTY BUILD_RPATH "$ORIGIN/../../../.")
    set_property(TARGET hyremote-qpa-platform PROPERTY BUILD_RPATH_USE_ORIGIN TRUE)
    set_property(TARGET hyremote-qpa-platform PROPERTY INSTALL_RPATH "$ORIGIN/../../../.")
endif()

target_compile_features(hyremote-qpa-platform PRIVATE cxx_std_17)
target_include_directories(hyremote-qpa-platform PRIVATE
    "${_hyremote_root_dir}/src/runtime/src"
)
target_link_libraries(hyremote-qpa-platform
    PRIVATE Qt6::Core Qt6::Gui Qt6::GuiPrivate HyRemote::RemoteAccess
)

target_compile_definitions(hyremote-qpa-platform PRIVATE
    HYREMOTE_QPA_QT_VERSION_MAJOR=6
    HYREMOTE_QPA_QT_VERSION_MINOR=8
    HYREMOTE_QPA_QT_VERSION_PATCH=3
)

install(TARGETS hyremote-qpa-platform
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/platforms"
    RUNTIME DESTINATION "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/platforms"
)

if(HYREMOTE_BUILD_TESTS)
    add_subdirectory(tests)

    set(_hyremote_qpa_deploy_fixture "${CMAKE_CURRENT_SOURCE_DIR}/tests/deploy_helper_fixture")
    set(_hyremote_qpa_deploy_runner "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_deploy_helper_fixture.cmake")

    # The deploy-helper matrix used to register fifteen near-identical CTests.
    # The case data now lives in two scripts (positive acceptances and
    # fail-closed rejections); each case still runs the real fixture runner with
    # its own binary tree and names its PASS/FAIL line. This keeps one CTest
    # process per group without weakening a rejection contract.
    set(_hyremote_qpa_deploy_common
        -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
        -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
        -DFIXTURE_BINARY_ROOT=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-cases
        -DFIXTURE_RUNNER=${_hyremote_qpa_deploy_runner}
    )

    find_program(HYREMOTE_DEPLOY_FIXTURE_NINJA ninja)
    if(HYREMOTE_DEPLOY_FIXTURE_NINJA)
        set(_hyremote_qpa_deploy_ninja "${HYREMOTE_DEPLOY_FIXTURE_NINJA}")
    else()
        set(_hyremote_qpa_deploy_ninja "")
        message(STATUS
            "ninja was not found: the explicit single-config/multi-config deploy-helper generator "
            "coverage is provided by the hosted jobs' platform default generator")
    endif()

    add_test(
        NAME hyremote-qpa-deploy-helper-positive
        COMMAND ${CMAKE_COMMAND} ${_hyremote_qpa_deploy_common}
            -DFIXTURE_NINJA=${_hyremote_qpa_deploy_ninja}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_deploy_helper_positive.cmake
    )
    set_tests_properties(hyremote-qpa-deploy-helper-positive PROPERTIES TIMEOUT 600)

    add_test(
        NAME hyremote-qpa-deploy-helper-negative
        COMMAND ${CMAKE_COMMAND} ${_hyremote_qpa_deploy_common}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_deploy_helper_negative.cmake
    )
    set_tests_properties(hyremote-qpa-deploy-helper-negative PROPERTIES TIMEOUT 600)

    if(UNIX AND NOT APPLE)
        add_test(
            NAME hyremote-qpa-source-payload-relocation
            COMMAND ${CMAKE_COMMAND}
                -DQPA_FILE=$<TARGET_FILE:hyremote-qpa-platform>
                -DREMOTEACCESS_FILE=$<TARGET_FILE:hyremote-remoteaccess>
                -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/source-payload-relocation
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_source_payload_relocation.cmake
        )
    endif()
endif()
