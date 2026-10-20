# HyRemote examples.
#
# Canonical adoption paths live under examples/learning: 01 C++ Widgets, 02 C++ Quick, 03 Generic zero-code,
# 04 QML API and 05 QPA proxy. They are the paths a new application developer is expected to follow, and they use
# only the public product surface.
#
# The remaining directory is the companion HyRemoteTool, not a tutorial example.

# ---------------------------------------------------------------- V0.1 canonical adoption paths

if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS AND TARGET Qt6::Widgets)
    add_subdirectory(learning/01-widgets-cpp)
else()
    message(STATUS "  learning/01-widgets-cpp skipped: Widgets adapter/Qt Widgets unavailable")
endif()

if(HYREMOTE_REMOTEACCESS_WITH_QUICK AND TARGET Qt6::Quick)
    if(NOT TARGET Qt6::Qml)
        find_package(Qt6 6.8 QUIET COMPONENTS Qml)
    endif()
    if(TARGET Qt6::Qml)
        add_subdirectory(learning/02-quick-cpp)
    else()
        message(STATUS "  learning/02-quick-cpp skipped: Qt Qml unavailable")
    endif()
else()
    message(STATUS "  learning/02-quick-cpp skipped: Quick adapter unavailable")
endif()

# 03 is two ordinary Qt applications that link no HyRemote target at all, so they build whenever Qt does. Whether
# their deployment helper can also deploy the Generic payload is settled inside each of them: an SDK built without
# Generic support still builds and runs these applications as plain Qt applications.
if(TARGET Qt6::Widgets)
    add_subdirectory(learning/03-zero-code-generic/widgets-app)
else()
    message(STATUS "  learning/03-zero-code-generic/widgets-app skipped: Qt Widgets unavailable")
endif()

if(TARGET Qt6::Quick)
    if(NOT TARGET Qt6::Qml)
        find_package(Qt6 6.8 QUIET COMPONENTS Qml)
    endif()
    if(TARGET Qt6::Qml)
        add_subdirectory(learning/03-zero-code-generic/quick-app)
    else()
        message(STATUS "  learning/03-zero-code-generic/quick-app skipped: Qt Qml unavailable")
    endif()
else()
    message(STATUS "  learning/03-zero-code-generic/quick-app skipped: Quick adapter unavailable")
endif()

# ---------------------------------------------------------------- promoted learning routes

if(HYREMOTE_BUILD_QML_API AND TARGET hyremote-qml AND TARGET Qt6::Qml AND TARGET Qt6::Quick)
    add_subdirectory(learning/04-qml)
else()
    message(STATUS "  learning/04-qml skipped: declarative HyRemote module unavailable")
endif()

# 05 QPA intentionally carries the exact Qt 6.8.3 qualification boundary. Do not let that
# private-ABI version coupling narrow Embedded C++ or QML example builds when QPA is not selected.
if(HYREMOTE_WITH_QPA_PROXY AND TARGET Qt6::Widgets)
    add_subdirectory(learning/05-qpa)
else()
    message(STATUS "  learning/05-qpa skipped: Transparent QPA mode unavailable")
endif()
