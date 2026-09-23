# HyRemote cross-compilation toolchain file: 32-bit ARM hard-float embedded Linux.
#
# Usage:
#
#   sh build.cmd build --integrations=cpp \
#                  --qt-prefix=/opt/qt-6.8.3-armhf
#
# Same rule as the aarch64 file: `--qt-prefix` must be a Qt built for this target.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(HYREMOTE_TOOLCHAIN_TRIPLE "arm-linux-gnueabihf" CACHE STRING "GNU target triple")

if(NOT DEFINED HYREMOTE_TOOLCHAIN_PREFIX OR HYREMOTE_TOOLCHAIN_PREFIX STREQUAL "")
    set(HYREMOTE_TOOLCHAIN_PREFIX "${HYREMOTE_TOOLCHAIN_TRIPLE}-")
endif()

set(CMAKE_C_COMPILER   "${HYREMOTE_TOOLCHAIN_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${HYREMOTE_TOOLCHAIN_PREFIX}g++")
set(CMAKE_AR           "${HYREMOTE_TOOLCHAIN_PREFIX}ar")
set(CMAKE_RANLIB       "${HYREMOTE_TOOLCHAIN_PREFIX}ranlib")
set(CMAKE_STRIP        "${HYREMOTE_TOOLCHAIN_PREFIX}strip")

set(CMAKE_SYSROOT "/usr/${HYREMOTE_TOOLCHAIN_TRIPLE}" CACHE PATH "Target sysroot")
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(HYREMOTE_CROSS_TARGET "arm-linux-gnueabihf" CACHE STRING "Informational cross target name")
