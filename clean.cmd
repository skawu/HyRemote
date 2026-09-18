#!/bin/sh
: <<'HYREMOTE_BATCH'
@echo off
rem ============================================================================
rem  HyRemote build cleanup. Same file works as a POSIX shell script and as a
rem  Windows batch file, like compile.cmd.
rem
rem  clean.cmd            remove build\<default-mode>
rem  clean.cmd --all      remove the entire build\ tree
rem  clean.cmd --mode=X   remove build\X
rem ============================================================================
setlocal enabledelayedexpansion
set "HYREMOTE_DEFAULT_MODE=qpa"
set "MODE=%HYREMOTE_DEFAULT_MODE%"
set "BUILD_ROOT=build"
set "ALL=0"

:parse
if "%~1"=="" goto :parsed
set "ARG=%~1"
if /i "%ARG%"=="--all" ( set "ALL=1" & shift & goto :parse )
if /i "%ARG%"=="--mode" ( set "MODE=%~2" & shift & shift & goto :parse )
if /i "%ARG%"=="--build-dir" ( set "BUILD_ROOT=%~2" & shift & shift & goto :parse )
if /i "%ARG:~0,7%"=="--mode=" ( set "MODE=%ARG:~7%" & shift & goto :parse )
if /i "%ARG:~0,12%"=="--build-dir=" ( set "BUILD_ROOT=%ARG:~12%" & shift & goto :parse )
echo Unknown option: %ARG% & exit /b 2

:parsed
if "%ALL%"=="1" (
    if exist "%BUILD_ROOT%" rmdir /s /q "%BUILD_ROOT%"
    echo Removed %BUILD_ROOT%
) else (
    if exist "%BUILD_ROOT%\%MODE%" rmdir /s /q "%BUILD_ROOT%\%MODE%"
    if exist "%BUILD_ROOT%.%MODE%.configure.log" del /q "%BUILD_ROOT%.%MODE%.configure.log"
    if exist "%BUILD_ROOT%.%MODE%.build.log" del /q "%BUILD_ROOT%.%MODE%.build.log"
    echo Removed %BUILD_ROOT%\%MODE%
)
exit /b 0
HYREMOTE_BATCH

# ---------------------------------------------------------------------------
#  POSIX shell half.
# ---------------------------------------------------------------------------
HYREMOTE_DEFAULT_MODE="qpa"
MODE="$HYREMOTE_DEFAULT_MODE"
BUILD_ROOT="build"
ALL=0

for arg in "$@"; do
    case "$arg" in
        --all) ALL=1 ;;
        --mode=*) MODE="${arg#--mode=}" ;;
        --build-dir=*) BUILD_ROOT="${arg#--build-dir=}" ;;
        *) echo "Unknown option: $arg"; exit 2 ;;
    esac
done

if [ "$ALL" = "1" ]; then
    rm -rf "$BUILD_ROOT"
    echo "Removed $BUILD_ROOT"
else
    rm -rf "$BUILD_ROOT/$MODE" "$BUILD_ROOT.$MODE.configure.log" "$BUILD_ROOT.$MODE.build.log"
    echo "Removed $BUILD_ROOT/$MODE"
fi
exit 0
