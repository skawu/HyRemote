#!/usr/bin/env bash
set -euo pipefail

# Install only host packages missing from the ephemeral GitHub runner. Public-Qt lanes must not pay
# the QPA/XCB dependency cost. ubuntu-24.04 already provides xvfb, so it is intentionally not an apt
# dependency here.
if [[ "$(uname -s)" != "Linux" ]]; then
  echo "install-linux-qt-desktop-deps.sh is Linux-only" >&2
  exit 2
fi

profile="${1:-public}"
if [[ "$profile" != "public" && "$profile" != "qpa" ]]; then
  echo "usage: $0 [public|qpa]" >&2
  exit 2
fi

# Minimal runtime and configure-time support used by the official Qt desktop archive for normal public-Qt GUI lanes.
# Qt6GuiConfig.cmake resolves WrapOpenGL while clean consumers configure, so the public profile needs the OpenGL
# development closure as well as the runtime library. QPA uses the same base and only adds its private-XCB needs.
packages=(
  libx11-xcb1
  libxcb-cursor0
  libxkbcommon-x11-0
  libgl1
  libgl1-mesa-dev
)

# QPA qualification additionally exercises the native XCB delegate/private-QPA path.
if [[ "$profile" == "qpa" ]]; then
  packages+=(
    libxcb-icccm4
    libxcb-image0
    libxcb-keysyms1
    libxcb-randr0
    libxcb-render-util0
    libxcb-shape0
    libxcb-shm0
    libxcb-sync1
    libxcb-util1
    libxcb-xfixes0
    libxcb-xkb1
    libxkbcommon-dev
    libxkbcommon-x11-dev
  )
fi

missing=()
for package in "${packages[@]}"; do
  if ! dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -q '^install ok installed$'; then
    missing+=("$package")
  fi
done

if (( ${#missing[@]} == 0 )); then
  echo "Linux Qt host dependencies already present for profile '$profile'; skipping apt."
  exit 0
fi

echo "Installing missing Linux Qt host dependencies for profile '$profile': ${missing[*]}"

# GitHub's hosted Ubuntu image is produced with a usable package index. Avoid an unconditional
# apt-get update on every QPA PR: it was a measured 5-7 second fixed cost. If the image index is
# stale or a mirror has rotated, fail over to one refresh and retry rather than weakening the
# dependency contract.
if sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${missing[@]}"; then
  exit 0
fi

echo "Initial apt install failed; refreshing package indexes once and retrying." >&2
sudo apt-get update -qq
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${missing[@]}"
