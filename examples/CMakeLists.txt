# Product examples intentionally consume only the public HyRemote facade. They are part of release
# acceptance, not internal architecture harnesses.

if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS AND TARGET Qt6::Widgets)
    add_subdirectory(widgets-basic)
else()
    message(STATUS "  widgets-basic example skipped: Widgets adapter is unavailable")
endif()

if(HYREMOTE_REMOTEACCESS_WITH_QUICK AND TARGET Qt6::Quick)
    if(NOT TARGET Qt6::Qml)
        find_package(Qt6 6.8 QUIET COMPONENTS Qml)
    endif()
    if(TARGET Qt6::Qml)
        add_subdirectory(quick-basic)
    else()
        message(STATUS "  quick-basic example skipped: Qt Qml is unavailable")
    endif()
else()
    message(STATUS "  quick-basic example skipped: Quick adapter is unavailable")
endif()
