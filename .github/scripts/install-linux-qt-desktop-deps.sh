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

# Minimal runtime support used by the official Qt desktop archive for normal public-Qt GUI lanes.
packages=(
  libx11-xcb1
  libxcb-cursor0
  libxkbcommon-x11-0
  libgl1
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
    libgl1-mesa-dev
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
sudo apt-get update -qq
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${missing[@]}"
