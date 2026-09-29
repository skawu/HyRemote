cmake_minimum_required(VERSION 3.21)

project(HyRemoteRfbDeliveryPreflight LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hyremote-rfb-delivery-preflight trle_preflight.cpp)

if(MSVC)
    target_compile_options(hyremote-rfb-delivery-preflight PRIVATE /W4 /permissive-)
else()
    target_compile_options(hyremote-rfb-delivery-preflight PRIVATE -Wall -Wextra -Wpedantic)
endif()
