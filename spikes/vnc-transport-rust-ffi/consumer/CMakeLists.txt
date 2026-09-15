cmake_minimum_required(VERSION 3.20)

project(hyremote_vnc_ffi_smoke LANGUAGES CXX)

add_executable(hyremote_vnc_ffi_smoke main.cpp)
target_compile_features(hyremote_vnc_ffi_smoke PRIVATE cxx_std_17)

if(UNIX AND NOT APPLE)
    target_link_libraries(hyremote_vnc_ffi_smoke PRIVATE dl)
endif()
