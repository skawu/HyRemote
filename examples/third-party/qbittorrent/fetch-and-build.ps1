# Fetch and build the pinned qBittorrent release **without modifying its sources**.
#
# Everything lands under build/third-party/qbittorrent (an ignored build directory). The only inputs are the
# pinned commit below and the Qt kit you pass in, which must be the same 6.8.3 kit that built the HyRemote QPA
# payload - the payload is qualified against one exact Qt private ABI.
#
# Run from a Developer PowerShell so that the MSVC toolchain is initialized.
# Usage: .\fetch-and-build.ps1 -QtRoot <QtRoot> [-WorkDir <dir>]
param(
    [Parameter(Mandatory = $true)][string]$QtRoot,
    [string]$WorkDir = "build/third-party/qbittorrent"
)

$ErrorActionPreference = "Stop"

$QbtTag = "release-5.2.3"
$QbtCommit = "70e16de46cd559a7db3e6d7faace5e40b86f63dc"

New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

if (-not (Test-Path "$WorkDir/src/.git")) {
    Write-Output "== cloning qBittorrent $QbtTag =="
    git clone --branch $QbtTag https://github.com/qbittorrent/qBittorrent.git "$WorkDir/src"
}

Write-Output "== checking out pinned commit $QbtCommit =="
git -C "$WorkDir/src" fetch --tags --quiet
git -C "$WorkDir/src" checkout --quiet $QbtCommit

Write-Output "== configuring against $QtRoot =="
# The default Visual Studio generator is used deliberately: it finds the MSVC toolchain without a shell setup.
# Pass extra prefixes for Boost/OpenSSL/libtorrent through CMAKE_PREFIX_PATH if your environment needs them.
cmake -S "$WorkDir/src" -B "$WorkDir/build" `
    -DCMAKE_PREFIX_PATH="$QtRoot"

Write-Output "== building (Release) =="
cmake --build "$WorkDir/build" --config Release

Write-Output ""
Write-Output "Build finished. Locate the built application under $WorkDir/build (the exact path depends on the"
Write-Output "generator), assemble the deployment described in README.md, and launch it with -platform hyremote."
Get-ChildItem -Path "$WorkDir/build" -Recurse -Filter "qbittorrent*.exe" -ErrorAction SilentlyContinue |
    Select-Object -First 5 -ExpandProperty FullName
