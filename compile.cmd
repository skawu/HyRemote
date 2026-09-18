#!/bin/sh
: <<'HYREMOTE_BATCH'
@echo off
rem ============================================================================
rem  HyRemote build entry point.
rem
rem  This single file is BOTH a POSIX shell script and a Windows batch file (the
rem  layout follows the 4diac-fbe build environment). On Windows, double-click it
rem  or run it from cmd; on Linux, run `sh compile.cmd` or `./compile.cmd`.
rem
rem  Default integration mode is QPA. Change HYREMOTE_DEFAULT_MODE below, or pass
rem  --mode=<qpa|cpp|qml|all|minimal> on the command line.
rem ============================================================================
setlocal enabledelayedexpansion
set "HYREMOTE_DEFAULT_MODE=qpa"
set "MODE=%HYREMOTE_DEFAULT_MODE%"
set "BUILD_TYPE=Release"
set "BUILD_ROOT=build"
set "QT_PREFIX="
set "TOOLCHAIN="
set "TESTS=OFF"
set "EXAMPLES=ON"
set "GENERATOR=Ninja"
set "JOBS="
set "VERBOSE=0"
set "CLEAN=0"

:parse
if "%~1"=="" goto :parsed
set "ARG=%~1"
if /i "%ARG%"=="--help" ( call :usage & exit /b 0 )
if /i "%ARG%"=="--tests" ( set "TESTS=ON" & shift & goto :parse )
if /i "%ARG%"=="--no-examples" ( set "EXAMPLES=OFF" & shift & goto :parse )
if /i "%ARG%"=="--clean" ( set "CLEAN=1" & shift & goto :parse )
if /i "%ARG%"=="-v" ( set "VERBOSE=1" & shift & goto :parse )
rem Space-separated form first: some Windows shells and wrappers split an argument at its '='.
if /i "%ARG%"=="--mode" ( set "MODE=%~2" & shift & shift & goto :parse )
if /i "%ARG%"=="--build-type" ( set "BUILD_TYPE=%~2" & shift & shift & goto :parse )
if /i "%ARG%"=="--qt-prefix" ( set "QT_PREFIX=%~2" & shift & shift & goto :parse )
if /i "%ARG%"=="--toolchain" ( set "TOOLCHAIN=%~2" & shift & shift & goto :parse )
if /i "%ARG%"=="-j" ( set "JOBS=%~2" & shift & shift & goto :parse )
if /i "%ARG:~0,12%"=="--qt-prefix=" ( set "QT_PREFIX=%ARG:~12%" & shift & goto :parse )
if /i "%ARG:~0,12%"=="--toolchain=" ( set "TOOLCHAIN=%ARG:~12%" & shift & goto :parse )
if /i "%ARG:~0,13%"=="--build-type=" ( set "BUILD_TYPE=%ARG:~13%" & shift & goto :parse )
if /i "%ARG:~0,7%"=="--mode=" ( set "MODE=%ARG:~7%" & shift & goto :parse )
if /i "%ARG:~0,2%"=="-j" ( set "JOBS=%ARG:~2%" & shift & goto :parse )
echo Unknown option: %ARG% & call :usage & exit /b 2

:parsed
rem Exactly one build directory. A second one silently mixes generators, compilers and stale caches, so
rem switching mode or reconfiguring requires an explicit clean instead of quietly reusing the tree.
set "BUILD_DIR=build"
set "PREVIOUS_MODE="
if exist "build\.hyremote-mode" for /f "usebackq delims=" %%M in ("build\.hyremote-mode") do set "PREVIOUS_MODE=%%M"
if not "!PREVIOUS_MODE!"=="" if /i not "!PREVIOUS_MODE!"=="!MODE!" (
    echo build\ already holds a !PREVIOUS_MODE! configuration; refusing to mix modes in one build directory.
    echo   compile.cmd --clean --mode !MODE!
    exit /b 2
)
if "%CLEAN%"=="1" if exist "build" rmdir /s /q "build"
if not exist "build" mkdir "build"
> "build\.hyremote-mode" echo %MODE%
set "CONFIGURE_LOG=build\configure.log"
set "BUILD_LOG=build\build.log"

where cmake >nul 2>&1
if errorlevel 1 if exist "C:\Qt\Tools\CMake_64\bin\cmake.exe" set "PATH=C:\Qt\Tools\CMake_64\bin;%PATH%"
where ninja >nul 2>&1
if errorlevel 1 if exist "C:\Qt\Tools\Ninja\ninja.exe" set "PATH=C:\Qt\Tools\Ninja;%PATH%"
where cmake >nul 2>&1
if errorlevel 1 echo cmake was not found. Install Qt's CMake or put cmake on PATH. & exit /b 2

rem The compiler must match the Qt kit. We deliberately do not guess between several installed
rem MinGW kits: choosing the wrong one fails inside Qt's own headers with a confusing message, which
rem is worse than being told what to do. An explicit --toolchain always wins.
set "HAVE_COMPILER=0"
where g++ >nul 2>&1 && set "HAVE_COMPILER=1"
if "%HAVE_COMPILER%"=="0" where cl >nul 2>&1 && set "HAVE_COMPILER=1"
echo %QT_PREFIX% | findstr /i "mingw" >nul 2>&1
if not errorlevel 1 (
    where g++ >nul 2>&1
    if errorlevel 1 if "%TOOLCHAIN%"=="" (
        echo --qt-prefix selects a MinGW Qt but no g++ is on PATH.
        echo   set PATH=C:\Qt\Tools\mingw1310_64\bin;%%PATH%%     ^(use the kit that matches your Qt^)
        exit /b 2
    )
)
if "%HAVE_COMPILER%"=="0" if "%TOOLCHAIN%"=="" (
    echo No C++ compiler found on PATH.
    echo   MinGW Qt kit: set PATH=C:\Qt\Tools\mingw1310_64\bin;%%PATH%%
    echo   MSVC Qt kit:  run this from a Developer Command Prompt ^(vcvars64^)
    echo   Cross build:  pass --toolchain=cmake/toolchains/^<file^>.cmake
    exit /b 2
)

call :mode_flags "%MODE%"
if errorlevel 1 exit /b 2
set "QT_ARG="
if not "%QT_PREFIX%"=="" set "QT_ARG=-DCMAKE_PREFIX_PATH=%QT_PREFIX%"
set "TC_ARG="
if not "%TOOLCHAIN%"=="" set "TC_ARG=-DCMAKE_TOOLCHAIN_FILE=%TOOLCHAIN%"
set "J_ARG="
if not "%JOBS%"=="" set "J_ARG=--parallel %JOBS%"

echo == HyRemote build ==
echo    mode        : %MODE%  (flags: %MODE_FLAGS%)
echo    build type  : %BUILD_TYPE%
echo    build dir   : %BUILD_DIR%
echo    generator   : %GENERATOR%
echo    qt prefix   : %QT_PREFIX%
echo    toolchain   : %TOOLCHAIN%
echo    tests       : %TESTS%
echo    examples    : %EXAMPLES%
echo.

if "%VERBOSE%"=="1" goto :configure_verbose
cmake -S . -B "%BUILD_DIR%" -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% %QT_ARG% %TC_ARG% %MODE_FLAGS% -DHYREMOTE_BUILD_TESTS=%TESTS% -DHYREMOTE_BUILD_EXAMPLES=%EXAMPLES% > "%CONFIGURE_LOG%" 2>&1
if errorlevel 1 echo Configure failed. See %CONFIGURE_LOG% & exit /b 3
if not exist "%BUILD_DIR%\CMakeCache.txt" echo Configure did not complete - see %CONFIGURE_LOG% & exit /b 3
cmake --build "%BUILD_DIR%" %J_ARG% > "%BUILD_LOG%" 2>&1
if errorlevel 1 echo Build failed. See %BUILD_LOG% & exit /b 4
echo Build succeeded. Logs: %CONFIGURE_LOG% , %BUILD_LOG%
if "%TESTS%"=="ON" (
    echo.
    echo To run the tests on Windows the Qt and MinGW runtime DLLs must be findable, otherwise every
    echo test dies with 0xc0000135 ^(DLL not found^). Use:
    echo   set "PATH=%%CD%%\%BUILD_DIR%\remoteaccess;%%CD%%\%BUILD_DIR%\qml\HyRemote;%QT_PREFIX%\bin;%%PATH%%"
    echo   set "QT_PLUGIN_PATH=%%CD%%\%BUILD_DIR%\plugins"
    echo   ctest --test-dir %BUILD_DIR% --output-on-failure
)
exit /b 0

:configure_verbose
cmake -S . -B "%BUILD_DIR%" -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% %QT_ARG% %TC_ARG% %MODE_FLAGS% -DHYREMOTE_BUILD_TESTS=%TESTS% -DHYREMOTE_BUILD_EXAMPLES=%EXAMPLES%
if errorlevel 1 echo Configure failed. & exit /b 3
cmake --build "%BUILD_DIR%" %J_ARG%
if errorlevel 1 echo Build failed. & exit /b 4
echo Build succeeded.
exit /b 0

:mode_flags
set "MODE_FLAGS="
if /i "%~1"=="qpa" set "MODE_FLAGS=-DHYREMOTE_WITH_QPA_PROXY=ON" & goto :eof
if /i "%~1"=="cpp" set "MODE_FLAGS=-DHYREMOTE_WITH_QPA_PROXY=OFF" & goto :eof
if /i "%~1"=="qml" set "MODE_FLAGS=-DHYREMOTE_BUILD_QML_API=ON -DHYREMOTE_WITH_QPA_PROXY=OFF" & goto :eof
if /i "%~1"=="all" set "MODE_FLAGS=-DHYREMOTE_BUILD_QML_API=ON -DHYREMOTE_WITH_QPA_PROXY=ON" & goto :eof
if /i "%~1"=="minimal" set "MODE_FLAGS=-DHYREMOTE_BUILD_EXAMPLES=OFF -DHYREMOTE_BUILD_QML_API=OFF -DHYREMOTE_WITH_QPA_PROXY=OFF -DHYREMOTE_BUILD_TESTS=OFF" & goto :eof
echo Unknown mode: %~1 (expected qpa, cpp, qml, all or minimal) & exit /b 1

:usage
echo Usage: compile.cmd [--mode=qpa^|cpp^|qml^|all^|minimal] [--build-type=Release^|Debug]
echo                    [--qt-prefix=PATH] [--toolchain=FILE.cmake]
echo                    [--tests] [--no-examples] [--clean] [-v] [-jN]
echo.
echo Defaults: mode=qpa (see HYREMOTE_DEFAULT_MODE in this file), Release, output in build\.
echo The project keeps exactly one build directory: use --clean before switching mode or rebuilding.
echo Cross-compilation: pass --toolchain=cmake/toolchains/^<file^>.cmake (see that directory).
exit /b 0
HYREMOTE_BATCH

# ---------------------------------------------------------------------------
#  POSIX shell half - same options, same defaults.  Reached only on Linux/macOS.
# ---------------------------------------------------------------------------
HYREMOTE_DEFAULT_MODE="qpa"
MODE="$HYREMOTE_DEFAULT_MODE"
BUILD_TYPE="Release"
QT_PREFIX=""
TOOLCHAIN=""
TESTS="OFF"
EXAMPLES="ON"
JOBS=""

usage() {
    cat <<'EOF'
Usage: sh compile.cmd [--mode=qpa|cpp|qml|all|minimal] [--build-type=Release|Debug]
                      [--qt-prefix=PATH] [--toolchain=FILE.cmake]
                      [--tests] [--no-examples] [--clean] [-v] [-jN]

Defaults: mode=qpa (see HYREMOTE_DEFAULT_MODE in this file), Release, output in build/.
The project keeps exactly one build directory: use --clean before switching mode or rebuilding.
Cross-compilation: pass --toolchain=cmake/toolchains/<file>.cmake (see that directory).
EOF
}

for arg in "$@"; do
    case "$arg" in
        --help) usage; exit 0 ;;
        --mode=*) MODE="${arg#--mode=}" ;;
        --build-type=*) BUILD_TYPE="${arg#--build-type=}" ;;
        --qt-prefix=*) QT_PREFIX="${arg#--qt-prefix=}" ;;
        --toolchain=*) TOOLCHAIN="${arg#--toolchain=}" ;;
        --tests) TESTS="ON" ;;
        --no-examples) EXAMPLES="OFF" ;;
        --clean) CLEAN=1 ;;
        -v) VERBOSE=1 ;;
        -j*) JOBS="${arg#-j}" ;;
        *) echo "Unknown option: $arg"; usage; exit 2 ;;
    esac
done

case "$MODE" in
    qpa) MODE_FLAGS="-DHYREMOTE_WITH_QPA_PROXY=ON" ;;
    cpp) MODE_FLAGS="-DHYREMOTE_WITH_QPA_PROXY=OFF" ;;
    qml) MODE_FLAGS="-DHYREMOTE_BUILD_QML_API=ON -DHYREMOTE_WITH_QPA_PROXY=OFF" ;;
    all) MODE_FLAGS="-DHYREMOTE_BUILD_QML_API=ON -DHYREMOTE_WITH_QPA_PROXY=ON" ;;
    minimal) MODE_FLAGS="-DHYREMOTE_BUILD_EXAMPLES=OFF -DHYREMOTE_BUILD_QML_API=OFF -DHYREMOTE_WITH_QPA_PROXY=OFF -DHYREMOTE_BUILD_TESTS=OFF" ;;
    *) echo "Unknown mode: $MODE (expected qpa, cpp, qml, all or minimal)"; exit 2 ;;
esac

# Exactly one build directory: refuse to mix modes in it instead of silently reusing a stale tree.
BUILD_DIR="build"
if [ -f "build/.hyremote-mode" ] && [ "$(cat build/.hyremote-mode)" != "$MODE" ]; then
    echo "build/ already holds a $(cat build/.hyremote-mode) configuration; refusing to mix modes in one build directory."
    echo "  sh compile.cmd --clean --mode $MODE"
    exit 2
fi
[ "${CLEAN:-0}" = "1" ] && rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
printf '%s\n' "$MODE" > "build/.hyremote-mode"
CONFIGURE_LOG="build/configure.log"
BUILD_LOG="build/build.log"

command -v cmake >/dev/null 2>&1 || { echo "cmake was not found on PATH."; exit 2; }
QT_ARG=""; [ -n "$QT_PREFIX" ] && QT_ARG="-DCMAKE_PREFIX_PATH=$QT_PREFIX"
TC_ARG=""; [ -n "$TOOLCHAIN" ] && TC_ARG="-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN"
J_ARG=""; [ -n "$JOBS" ] && J_ARG="--parallel $JOBS"

echo "== HyRemote build =="
echo "   mode        : $MODE  (flags: $MODE_FLAGS)"
echo "   build type  : $BUILD_TYPE"
echo "   build dir   : $BUILD_DIR"
echo "   qt prefix   : ${QT_PREFIX:-<from environment>}"
echo "   toolchain   : ${TOOLCHAIN:-<none: native build>}"
echo "   tests       : $TESTS"
echo "   examples    : $EXAMPLES"
echo

if [ "${VERBOSE:-0}" = "1" ]; then
    cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" $QT_ARG $TC_ARG $MODE_FLAGS \
        -DHYREMOTE_BUILD_TESTS="$TESTS" -DHYREMOTE_BUILD_EXAMPLES="$EXAMPLES" || { echo "Configure failed."; exit 3; }
    cmake --build "$BUILD_DIR" $J_ARG || { echo "Build failed."; exit 4; }
else
    cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" $QT_ARG $TC_ARG $MODE_FLAGS \
        -DHYREMOTE_BUILD_TESTS="$TESTS" -DHYREMOTE_BUILD_EXAMPLES="$EXAMPLES" > "$CONFIGURE_LOG" 2>&1 \
        || { echo "Configure failed. See $CONFIGURE_LOG"; exit 3; }
    [ -f "$BUILD_DIR/CMakeCache.txt" ] \
        || { echo "Configure did not complete - see $CONFIGURE_LOG"; exit 3; }
    cmake --build "$BUILD_DIR" $J_ARG > "$BUILD_LOG" 2>&1 \
        || { echo "Build failed. See $BUILD_LOG"; exit 4; }
    echo "Build succeeded. Logs: $CONFIGURE_LOG , $BUILD_LOG"
    if [ "$TESTS" = "ON" ]; then
        echo
        echo "To run the tests:"
        echo "  export PATH=\"\$PWD/$BUILD_DIR/remoteaccess:\$PWD/$BUILD_DIR/qml/HyRemote:${QT_PREFIX}/bin:\$PATH\""
        echo "  export QT_PLUGIN_PATH=\"\$PWD/$BUILD_DIR/plugins\""
        echo "  ctest --test-dir $BUILD_DIR --output-on-failure"
    fi
fi
exit 0
