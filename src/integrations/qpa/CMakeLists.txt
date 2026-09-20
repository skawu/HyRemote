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

    add_test(
        NAME hyremote-qpa-deploy-helper-ordinary
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-ordinary
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=OFF
            -P ${_hyremote_qpa_deploy_runner}
    )

    add_test(
        NAME hyremote-qpa-deploy-helper-qml-only
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-qml-only
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=ON
            -DTEST_DEPLOY_QPA=OFF
            -P ${_hyremote_qpa_deploy_runner}
    )

    add_test(
        NAME hyremote-qpa-deploy-helper-qml-composed
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-qml
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=ON
            -DTEST_DEPLOY_QPA=ON
            -P ${_hyremote_qpa_deploy_runner}
    )

    add_test(
        NAME hyremote-qpa-deploy-helper-installed-payload
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-installed-payload
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=OFF
            -DTEST_INSTALLED_PAYLOAD=ON
            -P ${_hyremote_qpa_deploy_runner}
    )

    add_test(
        NAME hyremote-qpa-deploy-helper-installed-payload-qml-only
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-installed-payload-qml-only
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=ON
            -DTEST_DEPLOY_QPA=OFF
            -DTEST_INSTALLED_PAYLOAD=ON
            -P ${_hyremote_qpa_deploy_runner}
    )

    add_test(
        NAME hyremote-qpa-deploy-helper-installed-payload-qml
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-installed-payload-qml
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=ON
            -DTEST_DEPLOY_QPA=ON
            -DTEST_INSTALLED_PAYLOAD=ON
            -P ${_hyremote_qpa_deploy_runner}
    )

    add_test(
        NAME hyremote-qpa-deploy-helper-reject-missing-qml
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-reject-missing-qml
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=ON
            -DTEST_QML_AVAILABLE=OFF
            -DEXPECT_CONFIGURE_FAILURE=ON
            -DEXPECT_FAILURE_FRAGMENT=requires_a_HyRemote_QML_payload
            -P ${_hyremote_qpa_deploy_runner}
    )

    add_test(
        NAME hyremote-qpa-deploy-helper-reject-stale-qml-metadata
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-reject-stale-qml-metadata
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=ON
            -DTEST_QML_AVAILABLE=OFF
            -DTEST_STALE_QML_METADATA=ON
            -DEXPECT_CONFIGURE_FAILURE=ON
            -DEXPECT_FAILURE_FRAGMENT=requires_a_HyRemote_QML_payload
            -P ${_hyremote_qpa_deploy_runner}
    )

    add_test(
        NAME hyremote-qpa-deploy-helper-reject-missing-qml-root
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-reject-missing-qml-root
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=ON
            -DTEST_INSTALLED_PAYLOAD=ON
            -DTEST_QML_IMPORT_PATH_EXISTS=OFF
            -DEXPECT_CONFIGURE_FAILURE=ON
            -DEXPECT_FAILURE_FRAGMENT=QML_import_root_does_not_exist
            -P ${_hyremote_qpa_deploy_runner}
    )

    add_test(
        NAME hyremote-qpa-deploy-helper-reject-missing-qml-module-dir
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-reject-missing-qml-module-dir
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=ON
            -DTEST_INSTALLED_PAYLOAD=ON
            -DTEST_QML_MODULE_DIR_EXISTS=OFF
            -DEXPECT_CONFIGURE_FAILURE=ON
            -DEXPECT_FAILURE_FRAGMENT=QML_module_directory_does_not_exist
            -P ${_hyremote_qpa_deploy_runner}
    )

    add_test(
        NAME hyremote-qpa-deploy-helper-reject-stale-qpa-metadata
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-reject-stale-qpa-metadata
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=OFF
            -DTEST_STALE_QPA_METADATA=ON
            -DEXPECT_CONFIGURE_FAILURE=ON
            -DEXPECT_FAILURE_FRAGMENT=requires_the_current_HyRemote_source_build
            -P ${_hyremote_qpa_deploy_runner}
    )

    add_test(
        NAME hyremote-qpa-deploy-helper-reject-qt-mismatch
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-reject-qt-mismatch
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=OFF
            -DTEST_QT_VERSION=6.8.2
            -DEXPECT_CONFIGURE_FAILURE=ON
            -DEXPECT_FAILURE_FRAGMENT=requires_exact_Qt_6.8.3
            -P ${_hyremote_qpa_deploy_runner}
    )

    add_test(
        NAME hyremote-qpa-deploy-helper-reject-missing-package
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-reject-missing-package
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=OFF
            -DTEST_INSTALLED_PAYLOAD=ON
            -DTEST_QPA_AVAILABLE=OFF
            -DEXPECT_CONFIGURE_FAILURE=ON
            -DEXPECT_FAILURE_FRAGMENT=requested_an_SDK_that_was_built_without_Transparent_QPA
            -P ${_hyremote_qpa_deploy_runner}
    )

    find_program(HYREMOTE_DEPLOY_FIXTURE_NINJA ninja)
    if(HYREMOTE_DEPLOY_FIXTURE_NINJA)
        add_test(
            NAME hyremote-qpa-deploy-helper-single-config-generator
            COMMAND ${CMAKE_COMMAND}
                -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
                -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-single-config-generator
                -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
                -DFIXTURE_GENERATOR=Ninja
                -DFIXTURE_MAKE_PROGRAM=${HYREMOTE_DEPLOY_FIXTURE_NINJA}
                -DFIXTURE_BUILD_TYPE=Release
                -DFIXTURE_EXPECTED_CONFIG=Release
                -DTEST_QML=OFF
                -P ${_hyremote_qpa_deploy_runner}
        )
        add_test(
            NAME hyremote-qpa-deploy-helper-multi-config-generator
            COMMAND ${CMAKE_COMMAND}
                -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
                -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-multi-config-generator
                -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
                "-DFIXTURE_GENERATOR=Ninja Multi-Config"
                -DFIXTURE_MAKE_PROGRAM=${HYREMOTE_DEPLOY_FIXTURE_NINJA}
                -DFIXTURE_EXPECTED_CONFIG=Release
                -DTEST_QML=OFF
                -P ${_hyremote_qpa_deploy_runner}
        )
    else()
        message(STATUS
            "ninja was not found: the explicit single-config/multi-config deploy-helper generator "
            "coverage is provided by the hosted jobs' platform default generator")
    endif()

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
