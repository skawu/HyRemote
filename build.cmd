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

set "QT_CHOICE_FILE=%~dp0build\.hyremote-qt-candidates"
set "QT_CHOICE_SENTINEL=%~dp0build\.hyremote-qt-choice-required"
if exist "%QT_CHOICE_FILE%" del /q "%QT_CHOICE_FILE%" >nul 2>&1
if exist "%QT_CHOICE_SENTINEL%" del /q "%QT_CHOICE_SENTINEL%" >nul 2>&1

cmake -P "%~dp0cmake\HyRemoteBuild.cmake" -- %*
set "HYB_EXIT=%ERRORLEVEL%"
if not exist "%QT_CHOICE_SENTINEL%" exit /b %HYB_EXIT%

rem CMake owns discovery/validation; this wrapper owns the terminal prompt.
rem A redirected/non-console stdin must fail closed rather than consuming a
rem scripted value or hanging on set /p.
mode con >nul 2>&1
if errorlevel 1 (
    echo Several Qt Kits require an interactive console, but stdin has no console.
    echo Pass --qt-prefix, build.yml qt.prefix, or --no-prompt. No Kit selected.
    del /q "%QT_CHOICE_FILE%" "%QT_CHOICE_SENTINEL%" >nul 2>&1
    exit /b 1
)
set "HYREMOTE_QT_CHOICE="
set /p "HYREMOTE_QT_CHOICE=Choose Qt Kit by number (or q to cancel): "
if /i "%HYREMOTE_QT_CHOICE%"=="q" (
    del /q "%QT_CHOICE_FILE%" "%QT_CHOICE_SENTINEL%" >nul 2>&1
    exit /b 1
)
set "HYREMOTE_QT_PREFIX="
for /f "usebackq tokens=1,* delims=|" %%A in ("%QT_CHOICE_FILE%") do (
    if "%%A"=="%HYREMOTE_QT_CHOICE%" set "HYREMOTE_QT_PREFIX=%%B"
)
if not defined HYREMOTE_QT_PREFIX (
    del /q "%QT_CHOICE_FILE%" "%QT_CHOICE_SENTINEL%" >nul 2>&1
    echo Invalid Qt Kit choice; no Kit selected.
    exit /b 1
)
del /q "%QT_CHOICE_FILE%" "%QT_CHOICE_SENTINEL%" >nul 2>&1
set "HYREMOTE_QT_SELECTION=%HYREMOTE_QT_PREFIX%"
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
