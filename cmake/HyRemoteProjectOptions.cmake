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

# Transport security is used only when the consumer declares that they need authenticated/encrypted access, and
# it uses whatever OpenSSL their environment already provides: any version, no provider choice, and nothing
# installed or vendored by this project. A build that does not ask for the capability acquires nothing, which
# also matches the release profile - the mode is not released before v1.0.0.0, so a pre-1.0 milestone build that
# turns this on is rejected as an unreleased mode rather than silently accepted.
option(HYREMOTE_WITH_TRANSPORT_SECURITY "Enable authenticated and encrypted transport (uses the OpenSSL from your environment)" OFF)

# The project never provides OpenSSL itself, so there is exactly one dependency question: is one there? V1 keeps
# the consumer-facing surface to that one option on purpose.
set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE OFF)

if(HYREMOTE_WITH_TRANSPORT_SECURITY)
    # No version is required: whatever the environment provides is used as it is, including an OpenSSL that
    # ships with the Qt SDK. 4.0.2 is the version this tree is verified against, kept as information, not a gate.
    find_package(OpenSSL QUIET COMPONENTS Crypto SSL)
    if(OpenSSL_FOUND)
        set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE ON)
        message(STATUS "HyRemote: authenticated/encrypted transport available (OpenSSL ${OPENSSL_VERSION})")
    else()
        # One actionable message and no hard failure: the consumer still gets a valid build, with the capability
        # reported unavailable. The runtime refuses to start with authentication enabled in that build (see
        # RemoteAccess), so it is not a silent downgrade either.
        message(WARNING
            "HyRemote: HYREMOTE_WITH_TRANSPORT_SECURITY=ON asks for authenticated/encrypted transport, but no "
            "OpenSSL with the Crypto and SSL components was found. Provide one from your own environment with "
            "-DOPENSSL_ROOT_DIR=<prefix>; this project never installs one for you. Everything else in HyRemote "
            "builds normally, and this build simply does not include the security capability.")
    endif()
endif()

# The default listener port, shared by every integration mode: the Core default is what the Embedded C++
# API and the declarative QML API start from, and the QPA proxy uses the same number when the platform
# string carries no port. An integrator that needs a different default sets it at configure time
# (-DHYREMOTE_DEFAULT_PORT=<port>) instead of patching the library. Tests and examples inherit the same
# value through the definition below, so such a build stays self-consistent. At run time the port is still
# overridable per process: RemoteAccess::setPort(), the QML `port` property, and the QPA `hyremote-port`
# platform parameter.
set(HYREMOTE_DEFAULT_PORT 5921 CACHE STRING "Default loopback listener port shared by all integration modes")
if(NOT HYREMOTE_DEFAULT_PORT MATCHES "^[0-9]+$"
   OR HYREMOTE_DEFAULT_PORT LESS 1
   OR HYREMOTE_DEFAULT_PORT GREATER 65535)
    message(FATAL_ERROR
        "HYREMOTE_DEFAULT_PORT must be a TCP port between 1 and 65535, got '${HYREMOTE_DEFAULT_PORT}'")
endif()
add_compile_definitions(HYREMOTE_DEFAULT_PORT=${HYREMOTE_DEFAULT_PORT})

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
