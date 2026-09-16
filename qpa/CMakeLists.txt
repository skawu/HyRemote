include(GNUInstallDirs)

# Transparent QPA Proxy — exact Qt 6.8.3 private-ABI package.
# Private QPA types and interception/composition remain isolated here while the plugin reuses the
# same shared HyRemote::RemoteAccess runtime as the C++ and QML modes.
get_filename_component(_hyremote_root_dir "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

add_library(hyremote-qpa-platform MODULE
    application_surface_model.cpp
    composite_target.cpp
    composite_target_routing.cpp
    interactive_composite_target.cpp
    hyremote_platform_plugin.cpp
    hyremote_qpa_interception.cpp
    hyremote_qpa_remote_controller.cpp
)
# Build-tree/internal alias only. The installed V1 CMake package deliberately does not export a
# HyRemote::QpaPlatform link target; installed consumers use `hyremote_deploy(... QPA)`.
add_library(HyRemote::QpaPlatform ALIAS hyremote-qpa-platform)

set_target_properties(hyremote-qpa-platform PROPERTIES
    AUTOMOC ON
    OUTPUT_NAME qhyremote
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/platforms"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/platforms"
)

# QPA is the zero-source-change artifact. The SDK stores qhyremote below
# <libdir>/HyRemote/plugins/platforms and RemoteAccess in <libdir>. The same platform MODULE can also
# be deployed directly from a source/add_subdirectory() build. On Linux, reserve the SDK-layout
# origin entry in both build and install RPATHs as the stable relocation anchor consumed by
# HyRemoteDeploy.cmake. The trailing `/.` is semantically the same location while deliberately
# reserving enough ELF RUNPATH string capacity for the deployed `$ORIGIN/../../<libdir>` replacement.
# CMake may append other build/toolchain lookup entries; the deployment helper rewrites only this
# known non-toolchain anchor. Relative build-tree paths are preferred so source builds do not bake
# the HyRemote build directory into qhyremote.
if(UNIX AND NOT APPLE)
    set_property(TARGET hyremote-qpa-platform PROPERTY BUILD_RPATH "$ORIGIN/../../../.")
    set_property(TARGET hyremote-qpa-platform PROPERTY BUILD_RPATH_USE_ORIGIN TRUE)
    set_property(TARGET hyremote-qpa-platform PROPERTY INSTALL_RPATH "$ORIGIN/../../../.")
endif()

target_compile_features(hyremote-qpa-platform PRIVATE cxx_std_17)
target_include_directories(hyremote-qpa-platform
    PRIVATE
        "${_hyremote_root_dir}/remoteaccess/src"
)
target_link_libraries(hyremote-qpa-platform
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::GuiPrivate
        HyRemote::RemoteAccess
)

if(TARGET Qt6::Widgets)
    target_link_libraries(hyremote-qpa-platform PRIVATE Qt6::Widgets)
    target_compile_definitions(hyremote-qpa-platform PRIVATE HYREMOTE_QPA_HAS_WIDGETS=1)
endif()

if(TARGET Qt6::Quick)
    target_link_libraries(hyremote-qpa-platform PRIVATE Qt6::Quick)
    target_compile_definitions(hyremote-qpa-platform PRIVATE HYREMOTE_QPA_HAS_QUICK=1)
endif()

target_compile_definitions(hyremote-qpa-platform PRIVATE
    HYREMOTE_QPA_QT_VERSION_MAJOR=6
    HYREMOTE_QPA_QT_VERSION_MINOR=8
    HYREMOTE_QPA_QT_VERSION_PATCH=3
)

# Install the module as a package-owned payload, not as a consumer C++ target. The package config
# publishes its exact installed path to the single HyRemote deployment helper.
install(TARGETS hyremote-qpa-platform
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/platforms"
    RUNTIME DESTINATION "${CMAKE_INSTALL_LIBDIR}/HyRemote/plugins/platforms"
)

if(HYREMOTE_BUILD_TESTS)
    add_subdirectory(tests)

    set(_hyremote_qpa_deploy_fixture "${CMAKE_CURRENT_SOURCE_DIR}/tests/deploy_helper_fixture")
    set(_hyremote_qpa_deploy_runner "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_deploy_helper_fixture.cmake")

    # Source-tree usage resolves the internal module target directly.
    add_test(
        NAME hyremote-qpa-deploy-helper-ordinary
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-ordinary
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QML=OFF
            -P ${_hyremote_qpa_deploy_runner}
    )

    # QML-only is a distinct public deploy shape from QML+QPA. Exercise the normal shared-runtime
    # supplemental script without letting QPA payload or RPATH relocation leak into it.
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

    # Installed-package usage deliberately has no HyRemote::QpaPlatform target. The public helper
    # must resolve the package-owned plugin through HyRemote_QPA_PLUGIN_FILE instead.
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
        NAME hyremote-qpa-deploy-helper-reject-qt-mismatch
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-qt-mismatch
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QT_VERSION=6.8.2
            -DEXPECT_CONFIGURE_FAILURE=ON
            -P ${_hyremote_qpa_deploy_runner}
    )

    add_test(
        NAME hyremote-qpa-deploy-helper-reject-missing-package
        COMMAND ${CMAKE_COMMAND}
            -DFIXTURE_SOURCE_DIR=${_hyremote_qpa_deploy_fixture}
            -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-no-qpa
            -DHYREMOTE_SOURCE_DIR=${_hyremote_root_dir}
            -DTEST_QPA_AVAILABLE=OFF
            -DEXPECT_CONFIGURE_FAILURE=ON
            -P ${_hyremote_qpa_deploy_runner}
    )

    # Unlike the configure-only helper fixtures above, this Linux test exercises the relocation on
    # the actual qhyremote build artifact. It catches a source-tree module that lost the stable
    # rewrite anchor before an installed consumer ever reaches the release gate.
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
