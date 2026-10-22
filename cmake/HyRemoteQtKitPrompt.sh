#!/bin/sh
# Input-only adapter for cmake/HyRemoteBuild.cmake; never selects a kit itself.
# Read directly from the controlling terminal, not CMake's captured stdin.
set -eu
count="${1:-}"
case "$count" in ""|*[!0-9]*) exit 2;; esac
if [ ! -r /dev/tty ]; then
  exit 3
fi
printf 'Choose Qt Kit [1-%s; q=cancel] (no default): ' "$count" >/dev/tty || exit 3
IFS= read -r choice </dev/tty || exit 3
printf 'HYREMOTE_QT_KIT_CHOICE=%s\n' "$choice"
