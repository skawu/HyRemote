: <<'HYREMOTE_BATCH'
@echo off
setlocal
rem ============================================================================
rem  HyRemote build and install entry point.
rem
rem  Same file works as a POSIX shell script and as a Windows batch file. This is
rem  the only build authority; the option, build.yml, compiler/toolchain, test and
rem  install semantics live in cmake\HyRemoteBuild.cmake. This wrapper only
rem  locates the bootstrap tools and forwards every argument.
rem
rem  There is deliberately no `#!/bin/sh` first line. cmd.exe would try to run it
rem  as a command and print "'#!' is not recognized" on every Windows invocation.
rem  POSIX runs this file as `sh ./build.cmd`, the first line is a heredoc opener
rem  that sh uses to skip the batch half, and cmd reads that same line as a label.
rem
rem  Windows PowerShell:  .\build.cmd help
rem  Windows cmd:         build.cmd help
rem  POSIX:               sh ./build.cmd help
rem
rem    build.cmd              build (the default subcommand)
rem    build.cmd build        configure if required, then compile
rem    build.cmd install      build if required, then install into <build-dir>/install
rem    build.cmd test         configure/build tests if required, then run them
rem    build.cmd clean        remove the selected build tree
rem    build.cmd rebuild      clean, then build
rem    build.cmd help         this text, plus every build option
rem ============================================================================

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
