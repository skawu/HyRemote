#!/bin/sh
: <<'HYREMOTE_BATCH'
@echo off
setlocal
rem ============================================================================
rem  Deprecated compatibility shim.
rem
rem  compile.cmd no longer has build logic of its own. build.cmd is the single
rem  canonical entry point, so every historical invocation simply becomes
rem  "build.cmd build" and keeps working. Migrate to build.cmd.
rem ============================================================================
echo HyRemote: compile.cmd is deprecated - use build.cmd instead. 1>&2
echo HyRemote: forwarding to "build.cmd build %*" 1>&2
call "%~dp0build.cmd" build %*
exit /b %ERRORLEVEL%
HYREMOTE_BATCH

# POSIX half: the same deprecation notice, then the same forwarding. build.cmd is
# not expected to carry an executable bit (compile.cmd never did), so it is run
# through sh rather than invoked directly.
echo "HyRemote: compile.cmd is deprecated - use build.cmd instead." >&2
printf 'HyRemote: forwarding to "build.cmd build %s"\n' "$*" >&2
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec sh "$SCRIPT_DIR/build.cmd" build "$@"
