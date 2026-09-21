# HyRemote examples.
#
# V0.1 canonical adoption paths live under examples/learning: 01 Widgets + C++, 02 Quick + C++, 03 Generic zero-code.
# They are the paths a new application developer is expected to follow, and they use only the public product surface.
#
# The remaining directories are earlier milestone examples and product showcases. They stay built so their evidence
# keeps working, but they are not the V0.1 teaching authority.

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

# ---------------------------------------------------------------- earlier milestone examples

if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS AND TARGET Qt6::Widgets)
    add_subdirectory(remote-support-showcase)
else()
    message(STATUS "  remote-support-showcase example skipped: Widgets adapter/Qt Widgets unavailable")
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
