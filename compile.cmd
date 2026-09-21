#!/bin/sh
: <<'HYREMOTE_BATCH'
@echo off
setlocal
rem HyRemote has one build entry point on Windows and POSIX. The actual option,
rem build.yml, compiler/toolchain and test semantics live in cmake\HyRemoteBuild.cmake.
rem This wrapper only locates the bootstrap tools and forwards every argument.

where cmake >nul 2>&1
if errorlevel 1 if exist "C:\Qt\Tools\CMake_64\bin\cmake.exe" set "PATH=C:\Qt\Tools\CMake_64\bin;%PATH%"
where ninja >nul 2>&1
if errorlevel 1 if exist "C:\Qt\Tools\Ninja\ninja.exe" set "PATH=C:\Qt\Tools\Ninja;%PATH%"
where cmake >nul 2>&1
if errorlevel 1 (
    echo cmake was not found. Install CMake or use the CMake shipped with Qt.
    exit /b 2
)

cmake -P "%~dp0cmake\HyRemoteBuild.cmake" -- %*
exit /b %ERRORLEVEL%
HYREMOTE_BATCH

set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake was not found on PATH." >&2
    exit 2
fi
exec cmake -P "$SCRIPT_DIR/cmake/HyRemoteBuild.cmake" -- "$@"
