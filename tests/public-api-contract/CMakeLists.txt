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

add_executable(hyremote-public-api-contract main.cpp)
target_compile_features(hyremote-public-api-contract PRIVATE cxx_std_17)
target_link_libraries(hyremote-public-api-contract PRIVATE HyRemote::RemoteAccess)

get_target_property(_hyremote_contract_links hyremote-public-api-contract LINK_LIBRARIES)
foreach(_link IN LISTS _hyremote_contract_links)
    if(_link MATCHES "HyRemote::(Core|QpaPlatform)")
        message(FATAL_ERROR "public contract consumer leaked low-level target into direct application links: ${_link}")
    endif()
endforeach()
