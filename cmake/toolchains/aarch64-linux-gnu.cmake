# HyRemote cross-compilation toolchain file: 64-bit ARM embedded Linux (aarch64).
#
# Usage (see cmake/toolchains/README.md and docs/guide/cross-compilation.md):
#
#   sh build.cmd build --integrations=qpa \
#                  --qt-prefix=/opt/qt-6.8.3-aarch64
#
# `--qt-prefix` must point at a Qt 6.8.3 installation **built for aarch64**, not at the host
# Qt: a host Qt provides host binaries and host libraries and cannot be linked into target code.
#
# Override the compiler prefix when your cross toolchain is not on PATH under its default name:
#
#   -DHYREMOTE_TOOLCHAIN_PREFIX=/opt/gcc-arm-13/bin/aarch64-none-linux-gnu-

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(HYREMOTE_TOOLCHAIN_TRIPLE "aarch64-linux-gnu" CACHE STRING "GNU target triple")

if(NOT DEFINED HYREMOTE_TOOLCHAIN_PREFIX OR HYREMOTE_TOOLCHAIN_PREFIX STREQUAL "")
    set(HYREMOTE_TOOLCHAIN_PREFIX "${HYREMOTE_TOOLCHAIN_TRIPLE}-")
endif()

set(CMAKE_C_COMPILER   "${HYREMOTE_TOOLCHAIN_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${HYREMOTE_TOOLCHAIN_PREFIX}g++")
set(CMAKE_AR           "${HYREMOTE_TOOLCHAIN_PREFIX}ar")
set(CMAKE_RANLIB       "${HYREMOTE_TOOLCHAIN_PREFIX}ranlib")
set(CMAKE_STRIP        "${HYREMOTE_TOOLCHAIN_PREFIX}strip")

# Search headers, libraries and packages inside the target sysroot only, while still running
# host tools (the compiler, moc, rcc, ninja, cmake itself) from the host.
set(CMAKE_SYSROOT "/usr/${HYREMOTE_TOOLCHAIN_TRIPLE}" CACHE PATH "Target sysroot")
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# HyRemote specifics for an embedded target:
#   * the QPA payload uses Qt's private QPA ABI, so the target Qt must be the exact same
#     version as the host-side expectations (6.8.3) - see docs/compatibility.md;
#   * EGLFS/embedded Linux is an unverified platform family today: cross-compiling proves the
#     build, not the runtime behaviour. Runtime evidence requires the target hardware.
set(HYREMOTE_CROSS_TARGET "aarch64-linux-gnu" CACHE STRING "Informational cross target name")
