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
if(UNIX AND NOT APPLE)
    set_property(TARGET hyremote-qml PROPERTY INSTALL_RPATH "$ORIGIN")
endif()

qt_query_qml_module(hyremote-qml
    PLUGIN_TARGET HYREMOTE_QML_PLUGIN_TARGET
    TARGET_PATH HYREMOTE_QML_TARGET_PATH
    QMLDIR HYREMOTE_QML_QMLDIR
    TYPEINFO HYREMOTE_QML_TYPEINFO
)

set(HYREMOTE_QML_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/qml/${HYREMOTE_QML_TARGET_PATH}")
set(HYREMOTE_QML_INSTALL_DIR "${HYREMOTE_QML_INSTALL_DIR}" CACHE INTERNAL
    "Installed HyRemote QML module directory" FORCE)

# The plugin lives under <lib>/qml/HyRemote in the SDK and under <prefix>/qml/HyRemote in a normal
# deployed Qt application. Cover both layouts with bounded origin-relative lookup entries; users do
# not need QML/SDK-specific runtime environment variables.
if(HYREMOTE_QML_PLUGIN_TARGET AND UNIX AND NOT APPLE)
    set_property(TARGET "${HYREMOTE_QML_PLUGIN_TARGET}" PROPERTY
        INSTALL_RPATH "$ORIGIN/../..;$ORIGIN/../../${CMAKE_INSTALL_LIBDIR}")
endif()

install(
    TARGETS hyremote-qml
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(HYREMOTE_QML_PLUGIN_TARGET)
    install(
        TARGETS "${HYREMOTE_QML_PLUGIN_TARGET}"
        LIBRARY DESTINATION "${HYREMOTE_QML_INSTALL_DIR}"
        RUNTIME DESTINATION "${HYREMOTE_QML_INSTALL_DIR}"
    )
endif()

install(FILES "${HYREMOTE_QML_QMLDIR}" DESTINATION "${HYREMOTE_QML_INSTALL_DIR}")
if(HYREMOTE_QML_TYPEINFO)
    install(FILES "${HYREMOTE_QML_TYPEINFO}" DESTINATION "${HYREMOTE_QML_INSTALL_DIR}")
endif()

if(HYREMOTE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
