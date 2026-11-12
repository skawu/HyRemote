cmake_minimum_required(VERSION 3.21)
project(HyRemoteQt68GlPboSourceR4Native LANGUAGES CXX)
option(HYREMOTE_R4_QT5 "Use Qt 5.15.2 native OpenGL for the R4 fixture" OFF)
if(HYREMOTE_R4_QT5)
    find_package(Qt5 5.15.2 REQUIRED COMPONENTS Core Gui Quick)
    set(HYREMOTE_R4_QT Qt5)
else()
    find_package(Qt6 6.8.3 REQUIRED COMPONENTS Core Gui Quick)
    set(HYREMOTE_R4_QT Qt6)
endif()
set(HYREMOTE_BUILD_TESTS OFF CACHE BOOL "No nested Core tests" FORCE)
get_filename_component(HYREMOTE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
add_subdirectory("${HYREMOTE_ROOT}/src/core" "${CMAKE_BINARY_DIR}/core")
add_executable(hyremote-qt68-gl-pbo-source-r4
    main.cpp
    "${HYREMOTE_ROOT}/src/runtime/src/quick/quick_gl_pbo_capture_source.cpp")
target_compile_features(hyremote-qt68-gl-pbo-source-r4 PRIVATE cxx_std_17)
target_include_directories(hyremote-qt68-gl-pbo-source-r4 PRIVATE
    "${HYREMOTE_ROOT}/src/runtime/src")
target_link_libraries(hyremote-qt68-gl-pbo-source-r4 PRIVATE
    hyremote-core ${HYREMOTE_R4_QT}::Core ${HYREMOTE_R4_QT}::Gui ${HYREMOTE_R4_QT}::Quick)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hyremote-qt68-gl-pbo-source-r4 PRIVATE -Wall -Wextra -Werror)
endif()

# Windows provider reuses the exact same real QQuickWindow/QSG four-quadrant
# CaptureSource contract fixture. No new product Runtime or duplicate harness.
if(WIN32 AND NOT HYREMOTE_R4_QT5)
    add_executable(hyremote-qt68-d3d11-staging-source-r4
        main.cpp
        "${HYREMOTE_ROOT}/src/runtime/src/quick/quick_d3d11_staging_capture_source.cpp")
    target_compile_definitions(hyremote-qt68-d3d11-staging-source-r4 PRIVATE
        HYREMOTE_R4_D3D11=1)
    target_compile_features(hyremote-qt68-d3d11-staging-source-r4 PRIVATE cxx_std_17)
    target_include_directories(hyremote-qt68-d3d11-staging-source-r4 PRIVATE
        "${HYREMOTE_ROOT}/src/runtime/src")
    target_link_libraries(hyremote-qt68-d3d11-staging-source-r4 PRIVATE
        hyremote-core Qt6::Core Qt6::Gui Qt6::Quick d3d11 dxgi)
endif()
