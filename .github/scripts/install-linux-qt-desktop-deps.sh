#!/usr/bin/env bash
set -euo pipefail

# Reference Linux CI uses the official Qt desktop archive. Qt6::Gui from that archive still resolves
# host OpenGL/GLX and XKB development interfaces through CMake package discovery, while qxcb/product
# E2E needs the bounded X11/XCB runtime helpers below. Keep this list repository-owned so focused and
# integrated jobs cannot silently qualify different Linux host environments.
if [[ "$(uname -s)" != "Linux" ]]; then
  echo "install-linux-qt-desktop-deps.sh is Linux-only" >&2
  exit 2
fi

sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  xvfb \
  libx11-xcb1 \
  libxcb-cursor0 \
  libxcb-icccm4 \
  libxcb-image0 \
  libxcb-keysyms1 \
  libxcb-randr0 \
  libxcb-render-util0 \
  libxcb-shape0 \
  libxcb-shm0 \
  libxcb-sync1 \
  libxcb-util1 \
  libxcb-xfixes0 \
  libxcb-xkb1 \
  libxkbcommon-x11-0 \
  libxkbcommon-dev \
  libxkbcommon-x11-dev \
  libgl1-mesa-dev
