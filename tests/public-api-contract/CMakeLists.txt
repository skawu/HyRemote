cmake_minimum_required(VERSION 3.21)
project(HyRemotePublicApiContract LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)
find_package(HyRemote CONFIG REQUIRED)

if(NOT TARGET HyRemote::RemoteAccess)
    message(FATAL_ERROR "V1 public contract requires HyRemote::RemoteAccess")
endif()

# Core remains a source/internal architecture boundary in V1. QPA is package payload reached through
# hyremote_deploy(... QPA), not a C++ link target. Re-exporting either would recreate extra SDK
# choices that ordinary applications should never need to understand.
if(TARGET HyRemote::Core)
    message(FATAL_ERROR "V1 installed package must not export HyRemote::Core")
endif()
if(TARGET HyRemote::QpaPlatform)
    message(FATAL_ERROR "V1 installed package must not export HyRemote::QpaPlatform")
endif()

# Freeze the installed facade's transitive CMake surface as well as its name. Normal consumers may
# inherit only dependencies required by the public header/API. Widgets, Quick, QML, Core and QPA are
# implementation/integration details and must never become mandatory just because this SDK was built
# with all V1 modes enabled.
get_target_property(_hyremote_facade_interface HyRemote::RemoteAccess INTERFACE_LINK_LIBRARIES)
if(NOT _hyremote_facade_interface)
    message(FATAL_ERROR "HyRemote::RemoteAccess exported no public Qt dependency interface")
endif()
foreach(_forbidden_link IN ITEMS
        "HyRemote::Core"
        "HyRemote::QpaPlatform"
        "Qt6::Widgets"
        "Qt6::Quick"
        "Qt6::Qml")
    list(FIND _hyremote_facade_interface "${_forbidden_link}" _forbidden_index)
    if(NOT _forbidden_index EQUAL -1)
        message(FATAL_ERROR
            "V1 RemoteAccess imported interface leaked non-public dependency ${_forbidden_link}: "
            "${_hyremote_facade_interface}")
    endif()
endforeach()
foreach(_required_link IN ITEMS "Qt6::Core" "Qt6::Network")
    list(FIND _hyremote_facade_interface "${_required_link}" _required_index)
    if(_required_index EQUAL -1)
        message(FATAL_ERROR
            "V1 RemoteAccess imported interface lost required public dependency ${_required_link}: "
            "${_hyremote_facade_interface}")
    endif()
endforeach()

add_executable(hyremote-public-api-contract main.cpp)
target_compile_features(hyremote-public-api-contract PRIVATE cxx_std_17)
target_link_libraries(hyremote-public-api-contract PRIVATE HyRemote::RemoteAccess)

get_target_property(_hyremote_contract_links hyremote-public-api-contract LINK_LIBRARIES)
foreach(_link IN LISTS _hyremote_contract_links)
    if(_link MATCHES "HyRemote::(Core|QpaPlatform)")
        message(FATAL_ERROR "public contract consumer leaked low-level target into direct application links: ${_link}")
    endif()
endforeach()
