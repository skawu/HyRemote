include(GNUInstallDirs)

# qt_add_qml_module() extracts QObject metadata while configuring the module. Enable AUTOMOC before
# the target is created so type registration is identical on all supported host toolchains.
set(CMAKE_AUTOMOC ON)
qt_policy(SET QTP0001 NEW)

qt_add_qml_module(hyremote-qml
    URI HyRemote
    VERSION 1.0
    SHARED
    SOURCES
        QmlRemoteAccess.h
        QmlRemoteAccess.cpp
)

# This backing library is implementation of the declarative `import HyRemote` module, not a second
# C++ SDK target. Consumers use the QML URI/type and the deployment helper; they do not link it.

target_compile_features(hyremote-qml PRIVATE cxx_std_17)
target_link_libraries(hyremote-qml
    PRIVATE
        Qt6::Core
        Qt6::Qml
        HyRemote::RemoteAccess
)

# The QML backing library is deployed beside HyRemoteRemoteAccess in the runtime lib directory.
# Source/add_subdirectory deployment may copy the build artifact directly, so reserve the same
# origin-local lookup in BUILD_RPATH as in the installed artifact. CMake still appends whatever
# build-time Qt lookup it needs; paths that stay inside the build tree are kept origin-relative.
if(UNIX AND NOT APPLE)
    set_property(TARGET hyremote-qml PROPERTY BUILD_RPATH "$ORIGIN")
    set_property(TARGET hyremote-qml PROPERTY BUILD_RPATH_USE_ORIGIN TRUE)
    set_property(TARGET hyremote-qml PROPERTY INSTALL_RPATH "$ORIGIN")
endif()

qt_query_qml_module(hyremote-qml
    PLUGIN_TARGET HYREMOTE_QML_PLUGIN_TARGET
    TARGET_PATH HYREMOTE_QML_TARGET_PATH
    QMLDIR HYREMOTE_QML_QMLDIR
    TYPEINFO HYREMOTE_QML_TYPEINFO
)

# V1's declarative package is a real runtime QML module. Fail closed if Qt stops producing the plugin
# target expected by deployment instead of silently accepting a partial build that only works from a
# developer tree.
if(NOT HYREMOTE_QML_PLUGIN_TARGET OR NOT TARGET "${HYREMOTE_QML_PLUGIN_TARGET}")
    message(FATAL_ERROR "HyRemote QML module did not produce its required plugin target")
endif()

# Source/add_subdirectory consumers normally add HyRemote with EXCLUDE_FROM_ALL. Merely referencing a
# target file from an install script does not make that target part of the application's build graph.
# Publish the concrete QML payload targets as an internal global property so hyremote_deploy() can
# add build-only dependencies without making QML a C++ link target or installed SDK target.
set_property(GLOBAL PROPERTY HYREMOTE_QML_SOURCE_DEPLOY_TARGETS
    "hyremote-qml;${HYREMOTE_QML_PLUGIN_TARGET}")

# Publish the same abstract "QML import root" metadata for source and installed acquisition. In a
# source/add_subdirectory build the module lives below <build>/qml/HyRemote, so <build>/qml is the
# import root. HyRemoteConfig.cmake publishes the installed <prefix>/<libdir>/qml root instead.
# hyremote_deploy() consumes this variable in either case; applications never copy or name module
# files directly.
get_filename_component(_hyremote_qml_build_import_root "${CMAKE_CURRENT_BINARY_DIR}/.." ABSOLUTE)
set(HyRemote_QML_IMPORT_PATH "${_hyremote_qml_build_import_root}" CACHE INTERNAL
    "HyRemote QML import root for source-tree deployment" FORCE)

set(HYREMOTE_QML_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/qml/${HYREMOTE_QML_TARGET_PATH}")
set(HYREMOTE_QML_INSTALL_DIR "${HYREMOTE_QML_INSTALL_DIR}" CACHE INTERNAL
    "Installed HyRemote QML module directory" FORCE)

# The plugin lives under <lib>/qml/HyRemote in the SDK and under <prefix>/qml/HyRemote in a normal
# deployed Qt application. Cover both layouts with bounded origin-relative lookup entries; users do
# not need QML/SDK-specific runtime environment variables. Mirror these entries in BUILD_RPATH so a
# source-built module copied by Qt's QML deployment machinery remains relocatable too.
if(UNIX AND NOT APPLE)
    set_property(TARGET "${HYREMOTE_QML_PLUGIN_TARGET}" PROPERTY
        BUILD_RPATH "$ORIGIN/../..;$ORIGIN/../../${CMAKE_INSTALL_LIBDIR}")
    set_property(TARGET "${HYREMOTE_QML_PLUGIN_TARGET}" PROPERTY BUILD_RPATH_USE_ORIGIN TRUE)
    set_property(TARGET "${HYREMOTE_QML_PLUGIN_TARGET}" PROPERTY
        INSTALL_RPATH "$ORIGIN/../..;$ORIGIN/../../${CMAKE_INSTALL_LIBDIR}")
endif()

install(
    TARGETS hyremote-qml
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
    TARGETS "${HYREMOTE_QML_PLUGIN_TARGET}"
    LIBRARY DESTINATION "${HYREMOTE_QML_INSTALL_DIR}"
    RUNTIME DESTINATION "${HYREMOTE_QML_INSTALL_DIR}"
)

install(FILES "${HYREMOTE_QML_QMLDIR}" DESTINATION "${HYREMOTE_QML_INSTALL_DIR}")
if(HYREMOTE_QML_TYPEINFO)
    install(FILES "${HYREMOTE_QML_TYPEINFO}" DESTINATION "${HYREMOTE_QML_INSTALL_DIR}")
endif()

if(HYREMOTE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
