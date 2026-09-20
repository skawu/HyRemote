include_guard(GLOBAL)

# A normal source build should produce the product, not the repository's development harnesses.
# Maintainers/CI opt into tests and examples explicitly. This also keeps add_subdirectory() clean
# without making top-level and vendored consumption behave like two different products.
option(HYREMOTE_BUILD_TESTS "Build HyRemote tests" OFF)
option(HYREMOTE_BUILD_EXAMPLES "Build HyRemote examples" OFF)

# Normal product path. These defaults intentionally produce the single shared
# HyRemote::RemoteAccess runtime with both public Qt UI-family adapters and the bounded RFB transport
# when a suitable Qt SDK is available. Applications do not select internal Core/adapters/backends
# merely to get the standard C++ API.
option(HYREMOTE_BUILD_CORE "Build the internal hyremote-core session/frame/dispatch library" ON)
option(HYREMOTE_BUILD_REMOTE_ACCESS "Build the shared HyRemote runtime and public C++ RemoteAccess facade when Qt is available" ON)
option(HYREMOTE_BUILD_WIDGETS_ADAPTER "Build the Qt Widgets target adapter when Qt Widgets is available" ON)
option(HYREMOTE_BUILD_QUICK_ADAPTER "Build the Qt Quick target adapter when Qt Quick is available" ON)
option(HYREMOTE_WITH_VNC "Enable the VNC/RFB correctness transport backend" ON)

# QML is one application integration frontend over the same runtime. Keep it opt-in so the default
# C++ path has no QtQml requirement.
option(HYREMOTE_BUILD_QML_API "Build the 'import HyRemote' QML API when Qt Qml is available" OFF)

# Generic Plugin is the public-Qt zero-code integration frontend introduced by the final V1
# technical route. It must preserve the application's normal native QPA/platform selection and must
# not acquire Qt private/QPA APIs. It is opt-in because it installs an additional Qt plugin payload.
option(HYREMOTE_WITH_GENERIC_PLUGIN "Enable the QGenericPlugin zero-code integration frontend" OFF)

# QPA is the exact-private-ABI zero-code frontend. It remains separately opt-in because it installs
# a platform plugin payload and must be rebuilt/qualified for the exact Qt private ABI.
option(HYREMOTE_WITH_QPA_PROXY "Enable the QPA zero-code integration frontend" OFF)

# Transport security is used only when the consumer explicitly asks for authenticated/encrypted access, and
# it uses the OpenSSL already supplied by that environment: no provider selector, no bundled crypto toolchain,
# and no hidden downgrade. A build that does not request the capability acquires nothing.
option(HYREMOTE_WITH_TRANSPORT_SECURITY "Enable authenticated and encrypted transport (uses the OpenSSL from your environment)" OFF)

set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE OFF)

if(HYREMOTE_WITH_TRANSPORT_SECURITY)
    # No provider choice is exposed. The exact provider/version used for an official release is recorded by the
    # release manifest, while source/SDK consumers may point CMake at their qualified installation through the
    # normal FindOpenSSL inputs such as OPENSSL_ROOT_DIR.
    find_package(OpenSSL QUIET COMPONENTS Crypto SSL)
    if(OpenSSL_FOUND)
        set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE ON)
        message(STATUS "HyRemote: authenticated/encrypted transport available (OpenSSL ${OPENSSL_VERSION})")
    else()
        # Explicit capability requests fail closed at configure time. Succeeding here would create a build whose
        # requested product profile and compiled capability set disagree, which is exactly the ambiguity V1 avoids.
        message(FATAL_ERROR
            "HyRemote: HYREMOTE_WITH_TRANSPORT_SECURITY=ON requires OpenSSL with the Crypto and SSL components, "
            "but none was found. Provide one from your environment (for example with "
            "-DOPENSSL_ROOT_DIR=<prefix>) or configure with HYREMOTE_WITH_TRANSPORT_SECURITY=OFF. "
            "HyRemote never installs or bundles OpenSSL for you.")
    endif()
endif()

# The default listener port is shared by every integration frontend. C++ and QML configure the same
# AccessInstance runtime directly; Generic and QPA bootstrap the same automatic controller/runtime and
# therefore inherit the same default when no frontend-specific startup parameter overrides it.
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