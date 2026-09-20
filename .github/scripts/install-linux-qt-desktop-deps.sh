#!/usr/bin/env bash
set -euo pipefail

# Reference Linux CI uses the official Qt desktop archive. Install only host packages
# that are actually missing from the ephemeral GitHub runner. Avoiding unconditional
# apt-get update/install saves time on warm images and keeps daily CI focused.
if [[ "$(uname -s)" != "Linux" ]]; then
  echo "install-linux-qt-desktop-deps.sh is Linux-only" >&2
  exit 2
fi

packages=(
  xvfb
  libx11-xcb1
  libxcb-cursor0
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
  libxkbcommon-x11-0
  libxkbcommon-dev
  libxkbcommon-x11-dev
  libgl1-mesa-dev
)

missing=()
for package in "${packages[@]}"; do
  if ! dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -q '^install ok installed$'; then
    missing+=("$package")
  fi
done

if (( ${#missing[@]} == 0 )); then
  echo "Linux Qt host dependencies already present; skipping apt."
  exit 0
fi

echo "Installing missing Linux Qt host dependencies: ${missing[*]}"
sudo apt-get update -qq
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${missing[@]}"
