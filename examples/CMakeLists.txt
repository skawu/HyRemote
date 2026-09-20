# V1 product examples consume only the intended public surface for their integration mode.
# Source directory names, target names and build output paths follow the same progressive taxonomy.
# Do not preserve legacy binary-directory aliases during the V1 foundation freeze.

if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS AND (TARGET Qt6::Widgets OR TARGET Qt5::Widgets))
    add_subdirectory(learning/01-widgets-basic)
    add_subdirectory(learning/02-widgets-control)
else()
    message(STATUS "  Widgets learning examples skipped: Widgets adapter/Qt Widgets unavailable")
endif()

if(HYREMOTE_REMOTEACCESS_WITH_QUICK AND (TARGET Qt6::Quick OR TARGET Qt5::Quick))
    if(TARGET Qt6::Quick AND NOT TARGET Qt6::Qml)
        find_package(Qt6 6.8 QUIET COMPONENTS Qml)
    elseif(TARGET Qt5::Quick AND NOT TARGET Qt5::Qml)
        find_package(Qt5 5.15 QUIET COMPONENTS Qml)
    endif()

    if((TARGET Qt6::Qml) OR (TARGET Qt5::Qml))
        add_subdirectory(learning/03-quick-cpp)
    else()
        message(STATUS "  03-quick-cpp skipped: Qt Qml unavailable")
    endif()
else()
    message(STATUS "  03-quick-cpp skipped: Quick adapter unavailable")
endif()

if(HYREMOTE_BUILD_QML_API AND TARGET hyremote-qml
   AND ((TARGET Qt6::Qml AND TARGET Qt6::Quick) OR (TARGET Qt5::Qml AND TARGET Qt5::Quick)))
    add_subdirectory(learning/04-quick-qml)
    add_subdirectory(learning/07-production-showcase)
else()
    message(STATUS "  QML API examples skipped: HyRemote QML API unavailable")
endif()

# QPA is exact-patch/private-ABI qualified. The fixtures themselves remain ordinary Qt-only apps.
if(HYREMOTE_WITH_QPA_PROXY)
    add_subdirectory(learning/05-qpa-existing-app)
else()
    message(STATUS "  05 QPA fixtures skipped: QPA mode unavailable")
endif()

# 06-session-security is enabled once the #170 public session surface exists. Do not ship a fake
# example against APIs that are not yet part of the product header.
