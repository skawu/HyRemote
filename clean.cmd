#!/bin/sh
: <<'HYREMOTE_BATCH'
@echo off
rem ============================================================================
rem  HyRemote build cleanup.
rem
rem  Same file works as a POSIX shell script and as a Windows batch file, like
rem  compile.cmd. The project keeps exactly one build directory, so this removes
rem  it entirely; use it before switching --mode or rebuilding from scratch.
rem ============================================================================
setlocal
if not "%~1"=="" if /i not "%~1"=="--help" echo Unknown option: %~1 & echo Usage: clean.cmd & exit /b 2
if /i "%~1"=="--help" echo Usage: clean.cmd   removes the single build directory & exit /b 0
if exist "build" rmdir /s /q "build"
if exist "build" (
    echo Could not remove build - close anything using it and retry.
    exit /b 3
)
echo Removed build
exit /b 0
HYREMOTE_BATCH

# ---------------------------------------------------------------------------
#  POSIX shell half.
# ---------------------------------------------------------------------------
for arg in "$@"; do
    case "$arg" in
        --help) echo "Usage: sh clean.cmd   removes the single build directory"; exit 0 ;;
        *) echo "Unknown option: $arg"; exit 2 ;;
    esac
done

rm -rf build
if [ -d build ]; then
    echo "Could not remove build - close anything using it and retry."
    exit 3
fi
echo "Removed build"
exit 0
