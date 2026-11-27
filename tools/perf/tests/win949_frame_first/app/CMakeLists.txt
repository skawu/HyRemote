# WIN949 NON-PRODUCT frame fixture (#949).
#

# Standalone Qt-only project: it must not link, include or deploy HyRemote. The
# product Generic plugin is supplied at run time through the installed build
# tree (-plugin hyremote:port=...). Both Qt5.15 and Qt6.8 are supported so the
# same fixture can drive the Qt5 LTS comparison.
#
# WIN949_WITH_CPP_API=ON additionally builds `win949-frame-fixture-cpp`, the one
# legal Qt5.15 integration route: the Generic plugin hard-requires Qt6, so that
# variant owns a HyRemote::RemoteAccess listener itself and links the product
# C++ API. Point -DHyRemote_DIR at the product build tree that exports
# HyRemoteConfig.cmake.
cmake_minimum_required(VERSION 3.21)
project(hyremote_win949_frame_fixture LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(WIN949_WITH_CPP_API "Also build the C++-API fixture variant (Qt5.15 route)" OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Quick)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Quick)

add_executable(win949-frame-fixture frame_fixture.cpp)
target_link_libraries(win949-frame-fixture PRIVATE Qt${QT_VERSION_MAJOR}::Quick)

if(MSVC)
    target_compile_options(win949-frame-fixture PRIVATE /utf-8)
endif()

if(WIN949_WITH_CPP_API)
    find_package(HyRemote CONFIG REQUIRED)
    add_executable(win949-frame-fixture-cpp frame_fixture_cpp.cpp)
    target_link_libraries(win949-frame-fixture-cpp PRIVATE
        Qt${QT_VERSION_MAJOR}::Quick
        HyRemote::RemoteAccess)
    if(MSVC)
        target_compile_options(win949-frame-fixture-cpp PRIVATE /utf-8)
    endif()
endif()

message(STATUS "win949 frame fixture Qt=${QT_VERSION_MAJOR} prefix=${Qt${QT_VERSION_MAJOR}_DIR} cpp-api=${WIN949_WITH_CPP_API}")
