include_guard(GLOBAL)

# A normal source build should produce the product, not the repository's development harnesses.
# Maintainers/CI opt into tests and examples explicitly. This also keeps add_subdirectory() clean
# without making top-level and vendored consumption behave like two different products.
option(HYREMOTE_BUILD_TESTS "Build HyRemote tests" OFF)
option(HYREMOTE_BUILD_EXAMPLES "Build HyRemote examples" OFF)

# Shared implementation layers. Core and Runtime are product implementation, not integration
# frontends. Frontend selection below must never change this ownership relationship.
option(HYREMOTE_BUILD_CORE "Build the internal hyremote-core session/frame/dispatch library" ON)
option(HYREMOTE_BUILD_REMOTE_ACCESS "Build the shared HyRemote common runtime when Qt is available" ON)
option(HYREMOTE_BUILD_WIDGETS_ADAPTER "Build the Qt Widgets target adapter when Qt Widgets is available" ON)
option(HYREMOTE_BUILD_QUICK_ADAPTER "Build the Qt Quick target adapter when Qt Quick is available" ON)
option(HYREMOTE_WITH_VNC "Enable the VNC/RFB correctness transport backend" ON)

# Four peer application integration frontends over the same Common Runtime.
# Embedded C++ supports both Widgets and Qt Quick targets; Widgets/Quick are Runtime adapter
# dimensions and are deliberately not represented as frontend choices.
option(HYREMOTE_BUILD_CPP_API "Build the Embedded C++ HyRemote::RemoteAccess frontend" ON)
option(HYREMOTE_BUILD_QML_API "Build the 'import HyRemote' QML frontend when Qt Qml is available" OFF)
option(HYREMOTE_WITH_GENERIC_PLUGIN "Enable the QGenericPlugin zero-code frontend" OFF)
option(HYREMOTE_WITH_QPA_PROXY "Enable the QPA Factory-Trampoline zero-code frontend" OFF)

# Transport security is a Common Runtime capability shared by every frontend. In its current implementation it
# provides VNC Authentication for the RFB stream and nothing more: the stream is authenticated, not encrypted, and
# the capability therefore depends on OpenSSL Crypto only. The exact option and capability identifier names are kept
# stable deliberately - renaming them is a compatibility question of its own, while the help/status text below has to
# describe what the capability actually does today.
option(HYREMOTE_WITH_TRANSPORT_SECURITY "Enable VNC Authentication support for the RFB transport using OpenSSL Crypto (authenticates the stream; does not encrypt it - VeNCrypt/TLS is not implemented)" OFF)

set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE OFF)

if(HYREMOTE_WITH_TRANSPORT_SECURITY)
    # Crypto is the whole dependency of the current implementation. Requiring SSL here would advertise an encrypted
    # transport that does not exist and would link a library the runtime never calls.
    find_package(OpenSSL QUIET COMPONENTS Crypto)
    if(OpenSSL_FOUND)
        set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE ON)
        message(STATUS
            "HyRemote: VNC Authentication support available (OpenSSL Crypto ${OPENSSL_VERSION}); the RFB stream is "
            "authenticated, not encrypted")
    else()
        message(FATAL_ERROR
            "HyRemote: HYREMOTE_WITH_TRANSPORT_SECURITY=ON requires OpenSSL Crypto, the component the current VNC "
            "Authentication implementation uses, but none was found. Provide it from your environment (for example "
            "with -DOPENSSL_ROOT_DIR=<prefix>) or configure with HYREMOTE_WITH_TRANSPORT_SECURITY=OFF. "
            "HyRemote never installs or bundles OpenSSL for you.")
    endif()
endif()

# The default listener port is shared by every integration frontend. C++ and QML configure the same
# AccessInstance runtime directly; Generic and QPA bootstrap the same automatic controller/runtime.
set(HYREMOTE_DEFAULT_PORT 5921 CACHE STRING "Default loopback listener port shared by all integration modes")
if(NOT HYREMOTE_DEFAULT_PORT MATCHES "^[0-9]+$"
   OR HYREMOTE_DEFAULT_PORT LESS 1
   OR HYREMOTE_DEFAULT_PORT GREATER 65535)
    message(FATAL_ERROR
        "HYREMOTE_DEFAULT_PORT must be a TCP port between 1 and 65535, got '${HYREMOTE_DEFAULT_PORT}'")
endif()
add_compile_definitions(HYREMOTE_DEFAULT_PORT=${HYREMOTE_DEFAULT_PORT})

# Platform/hardware optimization work is outside this desktop V1 foundation split unless a measured
# release blocker deliberately promotes it.
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
