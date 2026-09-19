include_guard(GLOBAL)

# A normal source build should produce the product, not the repository's development harnesses.
# Maintainers/CI opt into tests and examples explicitly. This also keeps add_subdirectory() clean
# without making top-level and vendored consumption behave like two different products.
option(HYREMOTE_BUILD_TESTS "Build HyRemote tests" OFF)
option(HYREMOTE_BUILD_EXAMPLES "Build HyRemote examples" OFF)

# Normal C++ product path. These defaults intentionally produce the single shared
# HyRemote::RemoteAccess facade with both public Qt UI families and the bounded RFB correctness
# transport when a suitable Qt SDK is available. Users do not select internal Core/adapters/backends
# merely to get the standard C++ library.
option(HYREMOTE_BUILD_CORE "Build the internal hyremote-core session/frame/dispatch library" ON)
option(HYREMOTE_BUILD_REMOTE_ACCESS "Build the public HyRemote::RemoteAccess C++ facade when Qt is available" ON)
option(HYREMOTE_BUILD_WIDGETS_ADAPTER "Build the Qt Widgets target adapter when Qt Widgets is available" ON)
option(HYREMOTE_BUILD_QUICK_ADAPTER "Build the Qt Quick target adapter when Qt Quick is available" ON)
option(HYREMOTE_WITH_VNC "Enable the VNC/RFB correctness transport backend" ON)

# QML is a product integration mode, not an implicit dependency of every C++ consumer. Keep it
# opt-in so the default C++ path remains one shared library with no QtQml requirement.
option(HYREMOTE_BUILD_QML_API "Build the declarative 'import HyRemote' QML API when Qt Qml is available" OFF)

# Transparent QPA is an exact-private-ABI package and therefore remains explicitly opt-in. Enabling
# it does not change the application's C++ link contract; it produces the qhyremote plugin payload.
option(HYREMOTE_WITH_QPA_PROXY "Enable the Transparent QPA Proxy integration mode" OFF)

# Transport security is a V1.0.0.0 product requirement, and its dependency comes from the user's own environment:
# **this project never installs OpenSSL itself**. The search below is what implements that rule.
#
#   * OpenSSL found  -> the authenticated/encrypted transport may use it. The capability itself is developed under
#                       #143; this option only decides whether the dependency is available.
#   * OpenSSL absent -> the project builds *without* any OpenSSL-backed feature, and says so once, with the exact
#                       way to provide it: through the Qt SDK (the Qt Maintenance Tool offers an
#                       "OpenSSL Toolkit" component), or by pointing CMake at an existing installation with
#                       -DOPENSSL_ROOT_DIR=<prefix>.
#
# What must never happen is a silent downgrade: the absence is reported, and the capability is recorded as
# unavailable rather than assumed. Turning the option off is the sanctioned way to acknowledge building without it.
option(HYREMOTE_WITH_TRANSPORT_SECURITY "Enable authenticated and encrypted transport (requires OpenSSL 3)" ON)

if(HYREMOTE_WITH_TRANSPORT_SECURITY)
    find_package(OpenSSL 3 QUIET)
    if(OpenSSL_FOUND)
        set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE ON)
        add_compile_definitions(HYREMOTE_HAS_TRANSPORT_SECURITY=1)
    else()
        set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE OFF)
        message(WARNING
            "HyRemote: the authenticated/encrypted transport is enabled but no OpenSSL 3 development files were "
            "found, so this build provides no OpenSSL-backed security capability. Provide OpenSSL from your own "
            "environment: with the Qt SDK, install the \"OpenSSL Toolkit\" component through the Qt Maintenance "
            "Tool, or configure with -DOPENSSL_ROOT_DIR=<prefix>. HyRemote does not install OpenSSL for you. "
            "Configure with -DHYREMOTE_WITH_TRANSPORT_SECURITY=OFF to acknowledge building without it.")
    endif()
else()
    set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE OFF)
endif()

# Development/architecture assets are never part of a normal product build unless explicitly asked.
option(HYREMOTE_BUILD_SPIKES "Build throwaway architecture spike harnesses (non-production)" OFF)

# Platform/hardware optimization work is post-V1 unless a real release blocker promotes it.
option(HYREMOTE_WITH_GBM "Enable GBM/DMA-BUF-oriented experimental backends" OFF)
option(HYREMOTE_WITH_RKMPP "Enable Rockchip MPP experimental encoder backend" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

if(HYREMOTE_WITH_RKMPP AND NOT HYREMOTE_WITH_GBM)
    message(STATUS "HYREMOTE_WITH_RKMPP enabled without GBM; the final buffer path is not yet frozen")
endif()
