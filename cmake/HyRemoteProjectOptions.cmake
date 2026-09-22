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
# provides both authenticated RFB profiles: VNC Authentication for the plaintext one, and the encrypted
# VeNCrypt 0.2 / X509Vnc 261 / TLS >= 1.2 profile with VNC Authentication inside the tunnel, which runs on Qt's
# OpenSSL TLS backend. The capability therefore depends on OpenSSL Crypto and SSL. The exact option and capability
# identifier names are kept stable deliberately - renaming them is a compatibility question of its own, while the
# help/status text below has to describe what the capability actually does today.
option(HYREMOTE_WITH_TRANSPORT_SECURITY "Enable the authenticated RFB transport security profiles using OpenSSL Crypto and SSL (VNC Authentication, and the VeNCrypt 0.2 / X509Vnc / TLS >= 1.2 encrypted profile with VNC Authentication inside TLS)" OFF)

set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE OFF)

if(HYREMOTE_WITH_TRANSPORT_SECURITY)
    # Both components are real dependencies of the shipped implementation: Crypto supplies the VNC authentication
    # primitive and the certificate/key match check, and SSL is what Qt's OpenSSL TLS backend is built on, so the
    # encrypted profile cannot exist without it. Requiring them together keeps the capability's claim and what the
    # runtime can actually do as one statement.
    find_package(OpenSSL QUIET COMPONENTS Crypto SSL)
    if(OpenSSL_FOUND)
        set(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE ON)

        # The encrypted profile's private runtime payload, resolved from the OpenSSL this build links against. It is
        # published as INTERNAL cache entries because the deploy helper runs from every consumer directory and long
        # before the install rules are processed; the install rules below then consume the same facts.
        #
        # Qt's OpenSSL TLS backend loads libssl/libcrypto at run time, Qt's own deployment tooling copies the TLS
        # plugin but not those libraries, and a Windows executable has no rpath - so a security-enabled deployment
        # must carry them, while a security-off build must carry none of it.
        set(HYREMOTE_PACKAGE_WITH_SECURITY_RUNTIME FALSE CACHE INTERNAL "HyRemote ships a private OpenSSL runtime payload")
        set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_MODE "NONE" CACHE INTERNAL "How the encrypted profile's runtime is provided")
        set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_SUBDIR "" CACHE INTERNAL "Package-relative directory of the runtime payload")
        set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_FILES "" CACHE INTERNAL "Package-relative names of the runtime payload files")
        set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_SOURCE_FILES "" CACHE INTERNAL "Absolute source locations of the same files")

        # FindOpenSSL resolves the artifact the *linker* uses, which on Windows is the import library, so the
        # runtime DLL is located from the same OpenSSL installation rather than assumed. Both the file CMake found
        # and the directory it lives in drive the search, so no distribution's version suffix is hard-coded here and
        # a missing runtime fails the configure instead of producing a tree that dies at the first TLS use.
        set(_hyremote_openssl_runtime "")
        set(_hyremote_openssl_runtime_names "")
        set(_hyremote_openssl_link_only TRUE)
        foreach(_hyremote_openssl_target OpenSSL::SSL OpenSSL::Crypto)
            set(_hyremote_openssl_location "")
            foreach(_hyremote_openssl_property IMPORTED_LOCATION IMPORTED_LOCATION_RELEASE IMPORTED_IMPLIB IMPORTED_IMPLIB_RELEASE)
                get_target_property(_hyremote_openssl_candidate "${_hyremote_openssl_target}" "${_hyremote_openssl_property}")
                if(NOT _hyremote_openssl_candidate MATCHES "-NOTFOUND$" AND NOT _hyremote_openssl_candidate STREQUAL "")
                    set(_hyremote_openssl_location "${_hyremote_openssl_candidate}")
                    break()
                endif()
            endforeach()

            if(_hyremote_openssl_location STREQUAL "")
                message(FATAL_ERROR
                    "HyRemote: the transport-security capability is enabled but ${_hyremote_openssl_target} has no "
                    "resolvable library, so the deployed tree cannot be described honestly. Configure with "
                    "HYREMOTE_WITH_TRANSPORT_SECURITY=OFF to build without the capability.")
            endif()

            if(_hyremote_openssl_location MATCHES "\\.dll$")
                list(APPEND _hyremote_openssl_runtime "${_hyremote_openssl_location}")
                set(_hyremote_openssl_link_only FALSE)
                continue()
            endif()
            if(_hyremote_openssl_location MATCHES "\\.(so|dylib)")
                set(_hyremote_openssl_link_only FALSE)
                continue()
            endif()
            if(NOT _hyremote_openssl_location MATCHES "\\.(a|lib)$")
                continue()
            endif()

            # An import library or a static archive. The static case needs no payload; the import case needs the
            # runtime library that carries the same base name inside the same OpenSSL installation.
            set(_hyremote_openssl_search_dirs "")
            get_filename_component(_hyremote_openssl_directory "${_hyremote_openssl_location}" DIRECTORY)
            if(DEFINED OPENSSL_ROOT_DIR AND NOT "${OPENSSL_ROOT_DIR}" STREQUAL "" AND IS_DIRECTORY "${OPENSSL_ROOT_DIR}")
                list(APPEND _hyremote_openssl_search_dirs "${OPENSSL_ROOT_DIR}/bin")
            endif()
            list(APPEND _hyremote_openssl_search_dirs "${_hyremote_openssl_directory}/../bin" "${_hyremote_openssl_directory}")

            # The link artifact is `libssl.lib` or `libcrypto.lib`; the runtime library carries the same stem with
            # the distribution's own version suffix, so the stem is taken from that artifact rather than assumed.
            get_filename_component(_hyremote_openssl_base "${_hyremote_openssl_location}" NAME_WE)
            if(_hyremote_openssl_base MATCHES "^[Ll]ib(.*)$")
                set(_hyremote_openssl_stem "${CMAKE_MATCH_1}")
            else()
                set(_hyremote_openssl_stem "${_hyremote_openssl_base}")
            endif()

            set(_hyremote_openssl_found "")
            foreach(_hyremote_openssl_search_dir IN LISTS _hyremote_openssl_search_dirs)
                if(NOT IS_DIRECTORY "${_hyremote_openssl_search_dir}")
                    continue()
                endif()
                file(GLOB _hyremote_openssl_candidates
                    "${_hyremote_openssl_search_dir}/${_hyremote_openssl_stem}*.dll"
                    "${_hyremote_openssl_search_dir}/lib${_hyremote_openssl_stem}*.dll")
                foreach(_hyremote_openssl_candidate_file IN LISTS _hyremote_openssl_candidates)
                    # A TLS or crypto runtime library carries the same stem as the artifact the linker used.
                    if(_hyremote_openssl_candidate_file MATCHES "${_hyremote_openssl_stem}[^/\\\\]*\\.dll$")
                        set(_hyremote_openssl_found "${_hyremote_openssl_candidate_file}")
                        break()
                    endif()
                endforeach()
                if(NOT _hyremote_openssl_found STREQUAL "")
                    break()
                endif()
            endforeach()

            if(_hyremote_openssl_found STREQUAL "")
                message(FATAL_ERROR
                    "HyRemote: the transport-security capability is enabled and ${_hyremote_openssl_target} links "
                    "against '${_hyremote_openssl_location}', but the OpenSSL runtime library that carries its name "
                    "was not found next to it. A security-enabled deployment must carry the OpenSSL runtime it was "
                    "linked against, because shipping a tree that starts and then fails at the first TLS use is not "
                    "an option. Set OPENSSL_ROOT_DIR to the OpenSSL installation to deploy from, or configure with "
                    "HYREMOTE_WITH_TRANSPORT_SECURITY=OFF.")
            endif()
            list(APPEND _hyremote_openssl_runtime "${_hyremote_openssl_found}")
        endforeach()

        if(_hyremote_openssl_runtime STREQUAL "")
            # A static OpenSSL is linked into the runtime itself, so there is no runtime payload to carry and nothing
            # a deployment could get wrong.
            set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_MODE "STATIC" CACHE INTERNAL
                "How the encrypted profile's runtime is provided" FORCE)
        elseif(WIN32)
            foreach(_hyremote_openssl_runtime_file IN LISTS _hyremote_openssl_runtime)
                get_filename_component(_hyremote_openssl_runtime_name "${_hyremote_openssl_runtime_file}" NAME)
                list(APPEND _hyremote_openssl_names "${_hyremote_openssl_runtime_name}")
            endforeach()
            list(REMOVE_DUPLICATES _hyremote_openssl_runtime)
            list(REMOVE_DUPLICATES _hyremote_openssl_names)
            set(HYREMOTE_PACKAGE_WITH_SECURITY_RUNTIME TRUE CACHE INTERNAL
                "HyRemote ships a private OpenSSL runtime payload" FORCE)
            set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_MODE "BUNDLED" CACHE INTERNAL
                "How the encrypted profile's runtime is provided" FORCE)
            set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_SUBDIR "bin" CACHE INTERNAL
                "Package-relative directory of the runtime payload" FORCE)
            set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_FILES "${_hyremote_openssl_names}" CACHE INTERNAL
                "Package-relative names of the runtime payload files" FORCE)
            set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_SOURCE_FILES "${_hyremote_openssl_runtime}" CACHE INTERNAL
                "Absolute source locations of the same files" FORCE)
            message(STATUS
                "HyRemote: the encrypted profile's OpenSSL runtime is carried as a private package payload: "
                "${_hyremote_openssl_names}")
        else()
            # On the Linux reference environment libssl/libcrypto are the distribution's runtime and every deployed
            # application resolves them through the standard loader paths. Copying them would ship a second copy of a
            # system library, so the platform keeps owning it and the package records that instead.
            set(HYREMOTE_PACKAGE_SECURITY_RUNTIME_MODE "SYSTEM" CACHE INTERNAL
                "How the encrypted profile's runtime is provided" FORCE)
        endif()
    else()
        message(FATAL_ERROR
            "HyRemote: HYREMOTE_WITH_TRANSPORT_SECURITY=ON requires OpenSSL Crypto and SSL, the components the "
            "authenticated RFB transport profiles use, but they were not both found. Provide them from your "
            "environment (for example with -DOPENSSL_ROOT_DIR=<prefix>) or configure with "
            "HYREMOTE_WITH_TRANSPORT_SECURITY=OFF. HyRemote never installs or bundles OpenSSL for you.")
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
