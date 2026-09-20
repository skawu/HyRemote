# V1 product examples consume only the intended public surface for their integration mode.

if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS AND TARGET Qt6::Widgets)
    add_subdirectory(learning/02-widgets-control)
    add_subdirectory(learning/07-production-showcase)
else()
    message(STATUS "  Widgets learning examples skipped: Widgets adapter/Qt Widgets unavailable")
endif()

if(HYREMOTE_REMOTEACCESS_WITH_QUICK AND TARGET Qt6::Quick)
    if(NOT TARGET Qt6::Qml)
        find_package(Qt6 6.8 QUIET COMPONENTS Qml)
    endif()
    if(TARGET Qt6::Qml)
        add_subdirectory(learning/03-quick-cpp)
    else()
        message(STATUS "  03-quick-cpp skipped: Qt Qml unavailable")
    endif()
else()
    message(STATUS "  03-quick-cpp skipped: Quick adapter unavailable")
endif()

if(HYREMOTE_BUILD_QML_API AND TARGET hyremote-qml AND TARGET Qt6::Qml AND TARGET Qt6::Quick)
    add_subdirectory(learning/04-quick-qml)
else()
    message(STATUS "  04-quick-qml skipped: HyRemote QML API unavailable")
endif()

# QPA remains exact-Qt/private-ABI qualified. The fixture is an ordinary Qt-only application.
if(HYREMOTE_WITH_QPA_PROXY AND TARGET Qt6::Widgets)
    add_subdirectory(learning/05-qpa-existing-app/widgets-app)
else()
    message(STATUS "  05 QPA Widgets fixture skipped: QPA mode unavailable")
endif()
