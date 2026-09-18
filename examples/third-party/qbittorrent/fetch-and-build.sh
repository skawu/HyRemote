#!/usr/bin/env bash
# Fetch and build the pinned qBittorrent release **without modifying its sources**.
#
# Everything lands under build/third-party/qbittorrent (an ignored build directory). The only inputs are the
# pinned commit below and the Qt kit you pass in, which must be the same 6.8.3 kit that built the HyRemote QPA
# payload - the payload is qualified against one exact Qt private ABI.
#
# Usage: fetch-and-build.sh <QtRoot> [work-dir]
set -euo pipefail

QBT_TAG="release-5.2.3"
QBT_COMMIT="70e16de46cd559a7db3e6d7faace5e40b86f63dc"
QT_ROOT="${1:-}"
WORK_DIR="${2:-build/third-party/qbittorrent}"

if [ -z "${QT_ROOT}" ]; then
    echo "usage: $0 <QtRoot> [work-dir]   (QtRoot must be the exact Qt 6.8.3 kit used to build HyRemote)" >&2
    exit 2
fi

mkdir -p "${WORK_DIR}"

if [ ! -d "${WORK_DIR}/src/.git" ]; then
    echo "== cloning qBittorrent ${QBT_TAG} =="
    git clone --branch "${QBT_TAG}" https://github.com/qbittorrent/qBittorrent.git "${WORK_DIR}/src"
fi

echo "== checking out pinned commit ${QBT_COMMIT} =="
git -C "${WORK_DIR}/src" fetch --tags --quiet || true
git -C "${WORK_DIR}/src" checkout --quiet "${QBT_COMMIT}"

echo "== configuring against ${QT_ROOT} =="
cmake -S "${WORK_DIR}/src" -B "${WORK_DIR}/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${QT_ROOT}"

echo "== building =="
cmake --build "${WORK_DIR}/build"

echo
echo "Build finished. Locate the built application under ${WORK_DIR}/build (the exact path depends on the"
echo "generator), assemble the deployment described in README.md, and launch it with -platform hyremote."
find "${WORK_DIR}/build" -maxdepth 3 -type f -name 'qbittorrent*' -executable 2>/dev/null || true
