cmake_minimum_required(VERSION 3.25)
project(HyRemoteThirdPartyQBittorrentQpa LANGUAGES CXX)

if(NOT HYREMOTE_THIRD_PARTY_SOURCE OR NOT IS_DIRECTORY "${HYREMOTE_THIRD_PARTY_SOURCE}")
    message(FATAL_ERROR "HYREMOTE_THIRD_PARTY_SOURCE must point to a disposable qBittorrent source tree")
endif()

# Resolve the exact V1 Qt first so HyRemote's QPA deploy contract and the
# upstream application's Qt package resolution share one SDK/private ABI.
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS
    Core Gui Network Quick Sql Svg Widgets Xml LinguistTools)
find_package(HyRemote CONFIG REQUIRED)

# Keep the complete desktop GUI surface while disabling services unrelated to
# the remote-UI product-fit question. These are ordinary upstream build options;
# no qBittorrent source file is patched.
set(GUI ON CACHE BOOL "Build qBittorrent desktop GUI" FORCE)
set(WEBUI OFF CACHE BOOL "WebUI is outside this UI-fit lane" FORCE)
set(PLUGINS OFF CACHE BOOL "Lua plugins are outside this UI-fit lane" FORCE)
set(STACKTRACE OFF CACHE BOOL "Stacktrace packaging is outside this UI-fit lane" FORCE)
set(TESTING OFF CACHE BOOL "Upstream unit tests are outside this product-fit lane" FORCE)
set(VERBOSE_CONFIGURE ON CACHE BOOL "Keep dependency evidence in CI logs" FORCE)
if(UNIX AND NOT APPLE)
    set(DBUS OFF CACHE BOOL "D-Bus integration is outside this QPA-fit lane" FORCE)
    set(SYSTEMD OFF CACHE BOOL "System service packaging is outside this GUI lane" FORCE)
endif()
if(MSVC)
    set(MSVC_RUNTIME_DYNAMIC ON CACHE BOOL "Match qBittorrent's Windows CI runtime contract" FORCE)
endif()

add_subdirectory("${HYREMOTE_THIRD_PARTY_SOURCE}" upstream)

if(NOT TARGET qbt_app)
    message(FATAL_ERROR "Expected pristine upstream target 'qbt_app'")
endif()

# qBittorrent's Linux install is distribution-oriented. The disposable test
# wrapper turns its installed tree into a private relocatable Qt bundle, so the
# executable needs a bundle-local RPATH. This is harness packaging glue, not a
# HyRemote product behavior and not an upstream source edit.
if(UNIX AND NOT APPLE)
    set_property(TARGET qbt_app PROPERTY INSTALL_RPATH "$ORIGIN/../lib")
endif()

# The application stays Qt/qBittorrent-only. Transparent QPA is attached solely
# through the public HyRemote deployment entry point.
hyremote_deploy(TARGET qbt_app QPA)
