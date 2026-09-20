# Ordinary Qt-only applications used to demonstrate Transparent QPA integration.
# Neither fixture links the HyRemote C++ or QML application API.

add_subdirectory(widgets-app)

if(TARGET Qt6::Quick AND TARGET Qt6::Qml)
    add_subdirectory(quick-app)
else()
    message(STATUS "  05 QPA Quick fixture skipped: Qt Quick/Qml unavailable")
endif()
