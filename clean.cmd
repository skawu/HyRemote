#!/bin/sh
: <<'HYREMOTE_BATCH'
@echo off
setlocal
rem ============================================================================
rem  Deprecated compatibility shim.
rem
rem  clean.cmd no longer removes anything itself. build.cmd clean is the single
rem  implementation, so the two can never disagree about which tree is removed.
rem ============================================================================
call "%~dp0build.cmd" clean %*
exit /b %ERRORLEVEL%
HYREMOTE_BATCH

echo "HyRemote: clean.cmd now forwards to build.cmd clean." >&2
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec sh "$SCRIPT_DIR/build.cmd" clean "$@"
