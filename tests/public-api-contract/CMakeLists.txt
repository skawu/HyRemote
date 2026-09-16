cmake_minimum_required(VERSION 3.21)
project(HyRemotePublicApiContract LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)
find_package(HyRemote CONFIG REQUIRED)

if(NOT TARGET HyRemote::RemoteAccess)
    message(FATAL_ERROR "V1 public contract requires HyRemote::RemoteAccess")
endif()

add_executable(hyremote-public-api-contract main.cpp)
target_compile_features(hyremote-public-api-contract PRIVATE cxx_std_17)
target_link_libraries(hyremote-public-api-contract PRIVATE HyRemote::RemoteAccess)

# Normal application acceptance must not need direct Core/backend/QPA linkage.
get_target_property(_hyremote_contract_links hyremote-public-api-contract LINK_LIBRARIES)
foreach(_link IN LISTS _hyremote_contract_links)
    if(_link MATCHES "HyRemote::(Core|QpaPlatform)")
        message(FATAL_ERROR "public contract consumer leaked low-level target into direct application links: ${_link}")
    endif()
endforeach()
