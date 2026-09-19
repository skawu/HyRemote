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

# Transport security is used only when the consumer declares that they need authenticated/encrypted access. A
# build that does not ask for the capability never acquires the dependency, which also matches the release
# profile: the authenticated/encrypted transport mode is not released before v1.0.0.0, so a pre-1.0 milestone
# build that turns this on is rejected as an unreleased mode rather than silently accepted.
option(HYREMOTE_WITH_TRANSPORT_SECURITY "Enable authenticated and encrypted transport (OpenSSL from your environment, or the project source tree)" OFF)

# Where that OpenSSL comes from, in the owner's third-party order: the environment the user already has first,
# the project's own pinned source tree last, and - when neither is there - no capability at all rather than a
# hard failure or a silent downgrade. These three settings are how the dependency is enabled, trimmed, or left
# out entirely, and they are a consumer-facing choice rather than an internal detail.
set(HYREMOTE_OPENSSL_PROVIDER "AUTO" CACHE STRING
    "OpenSSL source for the transport-security capability: AUTO (user environment first, project source last), SYSTEM (only the user's environment), BUNDLED (only the project's pinned source tree)")
set_property(CACHE HYREMOTE_OPENSSL_PROVIDER PROPERTY STRINGS AUTO SYSTEM BUNDLED)

# The project's own copy of OpenSSL, when the repository carries it as a submodule. It stays last in the order
# on purpose: it is the heaviest path, and an OpenSSL the user's environment already qualified should win.
set(HYREMOTE_OPENSSL_BUNDLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/openssl" CACHE PATH
    "Directory of the project's own OpenSSL source tree, used when HYREMOTE_OPENSSL_PROVIDER selects it")

set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE OFF)
set(HYREMOTE_OPENSSL_PROVIDER_USED "none")

if(HYREMOTE_WITH_TRANSPORT_SECURITY)
    if(HYREMOTE_OPENSSL_PROVIDER STREQUAL "AUTO" OR HYREMOTE_OPENSSL_PROVIDER STREQUAL "SYSTEM")
        # The owner's ruling (2026-09-19): the provider's version is **not enforced**. Whatever the user's
        # environment provides is used as it is - 1.1.1x, 3.x or 4.x - because dictating a line would leave a
        # consumer who already has a working OpenSSL unable to build the capability at all. 4.0.2 is the version
        # this tree is verified against, recorded as information rather than as a gate, and the project's own
        # source tree pins the newest line when that provider is the one in use.
        find_package(OpenSSL QUIET COMPONENTS Crypto SSL)
        if(OpenSSL_FOUND)
            set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE ON)
            set(HYREMOTE_OPENSSL_PROVIDER_USED "system")
            set(HYREMOTE_OPENSSL_LINK_TARGETS OpenSSL::Crypto OpenSSL::SSL)
        endif()
    endif()

    if(NOT HYREMOTE_TRANSPORT_SECURITY_AVAILABLE
       AND (HYREMOTE_OPENSSL_PROVIDER STREQUAL "AUTO" OR HYREMOTE_OPENSSL_PROVIDER STREQUAL "BUNDLED"))
        # The bundled provider is the project's own source tree, built as part of this configure. Compiling a
        # third-party cryptography provider from source is its own increment, so selecting it is honoured -
        # and reported as not yet available - rather than silently ignored. The preset facts are checked here
        # so the message can say which of the two situations applies.
        if(EXISTS "${HYREMOTE_OPENSSL_BUNDLED_DIR}/CMakeLists.txt")
            set(HYREMOTE_OPENSSL_BUNDLED_PRESENT ON)
        else()
            set(HYREMOTE_OPENSSL_BUNDLED_PRESENT OFF)
        endif()
    endif()

    if(NOT HYREMOTE_TRANSPORT_SECURITY_AVAILABLE)
        # One actionable message, and no hard failure: a consumer who asked for the capability and cannot
        # provide it still gets a valid build, with the capability reported unavailable. The runtime refuses
        # to start with authentication enabled in that build (see RemoteAccess), so this is not a silent
        # downgrade either - it is a build that says what it does not have.
        if(HYREMOTE_OPENSSL_BUNDLED_PRESENT)
            set(_hyremote_openssl_bundled_state
                "the project source tree at ${HYREMOTE_OPENSSL_BUNDLED_DIR} is present, but building OpenSSL from source is not implemented yet")
        else()
            set(_hyremote_openssl_bundled_state
                "the project source tree at ${HYREMOTE_OPENSSL_BUNDLED_DIR} is not present")
        endif()
        message(WARNING
            "HyRemote: HYREMOTE_WITH_TRANSPORT_SECURITY=ON asks for authenticated/encrypted transport, but no "
            "OpenSSL with the Crypto and SSL components was found for provider '${HYREMOTE_OPENSSL_PROVIDER}', so "
            "this build does not include the security capability. No version is required: whatever OpenSSL your "
            "environment provides is used as it is, including one that ships with the Qt SDK. Provide it with "
            "-DOPENSSL_ROOT_DIR=<prefix>, or select the project's own source tree with "
            "-DHYREMOTE_OPENSSL_PROVIDER=BUNDLED (${_hyremote_openssl_bundled_state}). Everything else in "
            "HyRemote builds normally.")
        unset(_hyremote_openssl_bundled_state)
    else()
        # FindOpenSSL publishes the detected version as OPENSSL_VERSION (upper case); naming it here makes the
        # configure report which provider version was accepted, which matters because no version is enforced.
        message(STATUS
            "HyRemote: authenticated/encrypted transport available from the ${HYREMOTE_OPENSSL_PROVIDER_USED} "
            "provider (OpenSSL ${OPENSSL_VERSION})")
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
