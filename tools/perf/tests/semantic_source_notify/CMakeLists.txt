cmake_minimum_required(VERSION 3.16)
project(HyRemoteSemanticSourceNotifyContract LANGUAGES CXX)

if(DEFINED QT_FORCE_MAJOR)
    if(NOT QT_FORCE_MAJOR STREQUAL "5" AND NOT QT_FORCE_MAJOR STREQUAL "6")
        message(FATAL_ERROR "QT_FORCE_MAJOR must be 5 or 6")
    endif()
    set(QT_VERSION_MAJOR ${QT_FORCE_MAJOR})
else()
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
endif()
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_executable(hyremote-semantic-source-notify-contract main.cpp)
target_compile_features(hyremote-semantic-source-notify-contract PRIVATE cxx_std_17)
get_filename_component(HYREMOTE_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
target_include_directories(hyremote-semantic-source-notify-contract PRIVATE
    "${HYREMOTE_REPO_ROOT}/src/runtime/src")
target_link_libraries(hyremote-semantic-source-notify-contract
    PRIVATE Qt${QT_VERSION_MAJOR}::Core)
message(STATUS "Semantic notifier contract Qt major: ${QT_VERSION_MAJOR}")
