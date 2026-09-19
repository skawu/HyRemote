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

# A source build of a cryptography provider is long, so it is parallelised on request. 1 is the safe default:
# a provider built by an unrelated job count is a worse trade than a slower first build.
set(HYREMOTE_OPENSSL_BUILD_JOBS "1" CACHE STRING
    "Parallel make jobs for the project's own OpenSSL source build")

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
        # The bundled provider is the project's own pinned source tree, compiled during this build. OpenSSL 4
        # ships its own Perl-driven build system - `Configure`, `config`, `build.info`, `Configurations/` - and
        # not a CMake build, and it generates `include/openssl/opensslv.h` at configure time, so the tree is
        # recognised by that build system rather than by a CMakeLists.txt it does not have.
        if(EXISTS "${HYREMOTE_OPENSSL_BUNDLED_DIR}/Configure"
           AND EXISTS "${HYREMOTE_OPENSSL_BUNDLED_DIR}/build.info")
            set(HYREMOTE_OPENSSL_BUNDLED_PRESENT ON)

            # Perl and make are prerequisites of this provider and of nothing else, so they are looked for only
            # when it is actually selected. nmake is deliberately not accepted: it drives a Visual C++ build,
            # which cannot produce libraries for the MinGW build this project is configured for.
            find_package(Perl QUIET)
            find_program(HYREMOTE_OPENSSL_MAKE_EXECUTABLE NAMES make mingw32-make gmake
                DOC "make used to build the project's own OpenSSL source tree")

            set(_hyremote_openssl_missing)
            if(NOT PERL_FOUND)
                list(APPEND _hyremote_openssl_missing "Perl")
            endif()
            if(NOT HYREMOTE_OPENSSL_MAKE_EXECUTABLE)
                list(APPEND _hyremote_openssl_missing "GNU make")
            endif()
            set(HYREMOTE_OPENSSL_BUNDLED_MISSING "${_hyremote_openssl_missing}")

            if(NOT _hyremote_openssl_missing)
                include(ExternalProject)
                set(_hyremote_openssl_build "${CMAKE_CURRENT_BINARY_DIR}/hyremote-openssl")
                set(_hyremote_openssl_install "${_hyremote_openssl_build}/install")

                # no-shared keeps the provider static, so nothing has to be deployed beside the runtime; no-asm
                # keeps an assembler out of the prerequisite list; no-tests/no-apps/no-docs keep the build to
                # the two libraries this capability links. --libdir=lib pins the install layout the imported
                # targets below name, because OpenSSL's own default differs between its targets.
                if(WIN32)
                    set(_hyremote_openssl_target "mingw64")
                else()
                    set(_hyremote_openssl_target "linux-x86_64")
                endif()

                ExternalProject_Add(hyremote-openssl-bundled
                    SOURCE_DIR "${HYREMOTE_OPENSSL_BUNDLED_DIR}"
                    BINARY_DIR "${_hyremote_openssl_build}/build"
                    # The Configure script is run by absolute path because ExternalProject's configure step
                    # executes in BINARY_DIR, which is deliberately outside the source tree.
                    CONFIGURE_COMMAND "${PERL_EXECUTABLE}" "${HYREMOTE_OPENSSL_BUNDLED_DIR}/Configure"
                        "${_hyremote_openssl_target}" no-shared no-asm no-tests no-apps no-docs
                        "--prefix=${_hyremote_openssl_install}"
                        "--openssldir=${_hyremote_openssl_install}/ssl"
                        "--libdir=lib"
                    BUILD_COMMAND "${HYREMOTE_OPENSSL_MAKE_EXECUTABLE}" "-j${HYREMOTE_OPENSSL_BUILD_JOBS}"
                    INSTALL_COMMAND "${HYREMOTE_OPENSSL_MAKE_EXECUTABLE}" install_sw
                    BUILD_BYPRODUCTS
                        "${_hyremote_openssl_install}/lib/libcrypto.a"
                        "${_hyremote_openssl_install}/lib/libssl.a"
                    UPDATE_COMMAND "")

                # Imported targets for the libraries the external project will produce. The include directory has
                # to exist at configure time because CMake validates interface include directories then, even
                # though the headers themselves only arrive during the build.
                file(MAKE_DIRECTORY "${_hyremote_openssl_install}/include")
                foreach(_hyremote_openssl_component IN ITEMS Crypto SSL)
                    string(TOLOWER "${_hyremote_openssl_component}" _hyremote_openssl_lower)
                    add_library(HyRemoteOpenSSL::${_hyremote_openssl_component} STATIC IMPORTED GLOBAL)
                    set_target_properties(HyRemoteOpenSSL::${_hyremote_openssl_component} PROPERTIES
                        IMPORTED_LOCATION "${_hyremote_openssl_install}/lib/lib${_hyremote_openssl_lower}.a"
                        INTERFACE_INCLUDE_DIRECTORIES "${_hyremote_openssl_install}/include")
                endforeach()

                set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE ON)
                set(HYREMOTE_OPENSSL_PROVIDER_USED "bundled")
                set(HYREMOTE_OPENSSL_LINK_TARGETS HyRemoteOpenSSL::Crypto HyRemoteOpenSSL::SSL)
                # A consumer linking those imported libraries has to wait for the external build, and
                # add_dependencies cannot express that transitively through an imported target.
                set(HYREMOTE_OPENSSL_BUILD_TARGET hyremote-openssl-bundled)
            endif()
        else()
            set(HYREMOTE_OPENSSL_BUNDLED_PRESENT OFF)
        endif()
    endif()

    if(NOT HYREMOTE_TRANSPORT_SECURITY_AVAILABLE)
        # One actionable message, and no hard failure: a consumer who asked for the capability and cannot
        # provide it still gets a valid build, with the capability reported unavailable. The runtime refuses
        # to start with authentication enabled in that build (see RemoteAccess), so this is not a silent
        # downgrade either - it is a build that says what it does not have.
        if(NOT HYREMOTE_OPENSSL_BUNDLED_PRESENT)
            set(_hyremote_openssl_bundled_state
                "the project source tree at ${HYREMOTE_OPENSSL_BUNDLED_DIR} is not checked out; `git submodule update --init third_party/openssl` fetches it")
        elseif(HYREMOTE_OPENSSL_BUNDLED_MISSING)
            set(_hyremote_openssl_bundled_state
                "the project source tree at ${HYREMOTE_OPENSSL_BUNDLED_DIR} is present, but building it needs ${HYREMOTE_OPENSSL_BUNDLED_MISSING}, which was not found")
        else()
            set(_hyremote_openssl_bundled_state
                "the project source tree at ${HYREMOTE_OPENSSL_BUNDLED_DIR} is present and Perl and make were found, but that does not guarantee the build succeeds: OpenSSL's Configure needs a complete Perl distribution, and the minimal one bundled with Git for Windows is not enough (it lacks Locale::Maketext::Simple). Their own output names the reason if it fails")
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
        # FindOpenSSL publishes the detected version as OPENSSL_VERSION (upper case). It is only known for the
        # system provider, so the version clause is omitted rather than printed empty elsewhere - a version is
        # reported when one was actually detected, which matters because none is enforced.
        if(HYREMOTE_OPENSSL_PROVIDER_USED STREQUAL "system")
            message(STATUS
                "HyRemote: authenticated/encrypted transport available from the system provider "
                "(OpenSSL ${OPENSSL_VERSION})")
        else()
            message(STATUS
                "HyRemote: authenticated/encrypted transport available from the ${HYREMOTE_OPENSSL_PROVIDER_USED} "
                "provider (the version follows that source tree)")
        endif()
    endif()
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
