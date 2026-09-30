cmake_minimum_required(VERSION 3.21)

project(HyRemoteRfbDeliveryPreflight LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hyremote-rfb-delivery-preflight trle_preflight.cpp)
add_executable(hyremote-rfb-incremental-preflight incremental_preflight.cpp)

option(HYREMOTE_PREFLIGHT_WITH_ZRLE "Build the zlib-backed ZRLE comparison candidate" OFF)
if(HYREMOTE_PREFLIGHT_WITH_ZRLE)
    find_package(ZLIB REQUIRED)
    add_executable(hyremote-rfb-zrle-preflight zrle_preflight.cpp)
    target_link_libraries(hyremote-rfb-zrle-preflight PRIVATE ZLIB::ZLIB)
endif()

foreach(target hyremote-rfb-delivery-preflight hyremote-rfb-incremental-preflight)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

if(HYREMOTE_PREFLIGHT_WITH_ZRLE)
    if(MSVC)
        target_compile_options(hyremote-rfb-zrle-preflight PRIVATE /W4 /permissive-)
    else()
        target_compile_options(hyremote-rfb-zrle-preflight PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
