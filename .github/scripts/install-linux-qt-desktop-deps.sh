#!/usr/bin/env bash
set -euo pipefail

# Install only host packages missing from the ephemeral GitHub runner. Public Qt deployment carries the
# application's native qxcb platform plugin, so the public profile owns that plugin's non-Qt runtime closure
# as well as the configure-time OpenGL support needed by clean installed/example consumers. The qpa profile
# adds only the development dependencies needed to qualify the private-QPA path. ubuntu-24.04 already provides
# xvfb, so it is intentionally not an apt dependency here.
if [[ "$(uname -s)" != "Linux" ]]; then
  echo "install-linux-qt-desktop-deps.sh is Linux-only" >&2
  exit 2
fi

profile="${1:-public}"
if [[ "$profile" != "public" && "$profile" != "qpa" ]]; then
  echo "usage: $0 [public|qpa]" >&2
  exit 2
fi

# Runtime and configure-time support used by the official Qt desktop archive for normal public-Qt GUI lanes.
# Installed/example consumers run their own find_package(Qt6 Gui), so WrapOpenGL must resolve independently of
# the runner image. hyremote_deploy() also carries the consumer's native qxcb platform plugin, so its runtime
# XCB dependencies are explicit public-lane prerequisites instead of accidental runner-image dependencies.
packages=(
  libx11-xcb1
  libxcb-cursor0
  libxkbcommon-x11-0
  libgl1
  libgl1-mesa-dev
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
)

# QPA qualification additionally compiles against native/private platform integration surfaces.
if [[ "$profile" == "qpa" ]]; then
  packages+=(
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