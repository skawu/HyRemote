# V1 product examples consume only the intended public surface for their integration mode.

if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS AND TARGET Qt6::Widgets)
    add_subdirectory(widgets-basic)
    add_subdirectory(remote-support-showcase)
else()
    message(STATUS "  Widgets examples skipped: Widgets adapter/Qt Widgets unavailable")
endif()

if(HYREMOTE_REMOTEACCESS_WITH_QUICK AND TARGET Qt6::Quick)
    if(NOT TARGET Qt6::Qml)
        find_package(Qt6 6.8 QUIET COMPONENTS Qml)
    endif()
    if(TARGET Qt6::Qml)
        add_subdirectory(quick-basic)
    else()
        message(STATUS "  quick-basic example skipped: Qt Qml unavailable")
    endif()
else()
    message(STATUS "  quick-basic example skipped: Quick adapter unavailable")
endif()

if(HYREMOTE_BUILD_QML_API AND TARGET hyremote-qml AND TARGET Qt6::Qml AND TARGET Qt6::Quick)
    add_subdirectory(qml-basic)
else()
    message(STATUS "  qml-basic example skipped: declarative HyRemote module unavailable")
endif()

# E4 belongs to the Transparent QPA milestone and intentionally carries the exact Qt 6.8.3
# qualification boundary. Do not let that private-ABI version coupling narrow Embedded C++ or QML
# example builds when the QPA mode is not selected. The ordinary Qt-only example remains independently
# buildable from its own directory for source inspection/demo purposes.
if(HYREMOTE_WITH_QPA_PROXY AND TARGET Qt6::Widgets)
    add_subdirectory(qpa-proxy-existing-app)
else()
    message(STATUS "  qpa-proxy-existing-app example skipped: Transparent QPA mode unavailable")
endif()
