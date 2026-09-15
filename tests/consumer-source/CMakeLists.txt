cmake_minimum_required(VERSION 3.21)

project(HyRemoteSourceConsumer LANGUAGES CXX)

if(NOT HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "Configure with -DHYREMOTE_SOURCE_DIR=<HyRemote source tree>")
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)

set(HYREMOTE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(HYREMOTE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(HYREMOTE_BUILD_SPIKES OFF CACHE BOOL "" FORCE)
set(HYREMOTE_BUILD_CORE ON CACHE BOOL "" FORCE)
set(HYREMOTE_BUILD_REMOTE_ACCESS ON CACHE BOOL "" FORCE)

add_subdirectory("${HYREMOTE_SOURCE_DIR}" hyremote)

add_executable(hyremote-source-consumer main.cpp)
target_compile_features(hyremote-source-consumer PRIVATE cxx_std_17)
target_link_libraries(hyremote-source-consumer PRIVATE HyRemote::RemoteAccess)
