cmake_minimum_required(VERSION 3.21)

project(
    HyRemote
    VERSION 0.0.0
    DESCRIPTION "Qt Embedded Remote Access Framework"
    LANGUAGES C CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HyRemoteProjectOptions)
include(HyRemoteDeploy)

include(CTest)

message(STATUS "Configuring HyRemote ${PROJECT_VERSION}")
message(STATUS "  Build core:          ${HYREMOTE_BUILD_CORE}")
message(STATUS "  Build RemoteAccess:  ${HYREMOTE_BUILD_REMOTE_ACCESS}")
message(STATUS "  Build QML API:       ${HYREMOTE_BUILD_QML_API}")
message(STATUS "  Build tests:         ${HYREMOTE_BUILD_TESTS}")
message(STATUS "  Build examples:      ${HYREMOTE_BUILD_EXAMPLES}")
message(STATUS "  VNC backend:         ${HYREMOTE_WITH_VNC}")
message(STATUS "  QPA Proxy:           ${HYREMOTE_WITH_QPA_PROXY}")
message(STATUS "  Build spikes:        ${HYREMOTE_BUILD_SPIKES}")

# hyremote-core: the production session/frame/dispatch contract frozen by ARCH-01.
# UI-neutral and free of Qt/protocol/platform types.
if(HYREMOTE_BUILD_CORE)
    add_subdirectory(core)
endif()

# Product-facing C++ facade. Keep bare Core builds usable on hosts without Qt: when HyRemote is
# configured as the top-level project and Qt is unavailable, the product target is skipped with an
# explicit status message. A source-consumer subproject, however, is expected to provide its Qt SDK
# and fails if the product target was requested but Qt Core/Network cannot be resolved.
if(HYREMOTE_BUILD_REMOTE_ACCESS)
    if(NOT TARGET HyRemote::Core)
        message(FATAL_ERROR "HyRemote::RemoteAccess requires HYREMOTE_BUILD_CORE=ON")
    endif()

    if(NOT TARGET Qt6::Core OR NOT TARGET Qt6::Network)
        find_package(Qt6 6.8 QUIET COMPONENTS Core Network)
    endif()

    if(TARGET Qt6::Core AND TARGET Qt6::Network)
        add_subdirectory(remoteaccess)
    elseif(PROJECT_IS_TOP_LEVEL)
        message(STATUS "  RemoteAccess target skipped: Qt 6.8+ Core/Network not found")
    else()
        message(FATAL_ERROR
            "HYREMOTE_BUILD_REMOTE_ACCESS=ON requires Qt 6.8+ Core and Network. "
            "Find/provide the target Qt SDK before add_subdirectory(HyRemote), or disable the product target explicitly.")
    endif()
endif()

# Declarative QML integration is a thin wrapper over the same public RemoteAccess runtime. It must
# never create a second capture/transport/input stack. Keep the module optional for bare Core builds
# and source consumers that intentionally disable the product-facing QML mode.
if(HYREMOTE_BUILD_QML_API)
    if(NOT TARGET hyremote-remoteaccess)
        if(PROJECT_IS_TOP_LEVEL)
            message(STATUS "  QML API skipped: HyRemote::RemoteAccess is unavailable")
        else()
            message(FATAL_ERROR "HYREMOTE_BUILD_QML_API=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON")
        endif()
    else()
        if(NOT TARGET Qt6::Qml)
            find_package(Qt6 6.8 QUIET COMPONENTS Qml)
        endif()

        if(TARGET Qt6::Qml)
            add_subdirectory(qml/HyRemote)
        elseif(PROJECT_IS_TOP_LEVEL)
            message(STATUS "  QML API skipped: Qt 6.8+ Qml not found")
        else()
            message(FATAL_ERROR
                "HYREMOTE_BUILD_QML_API=ON requires Qt 6.8+ Qml. "
                "Provide the target Qt SDK before add_subdirectory(HyRemote), or disable the QML API explicitly.")
        endif()
    endif()
endif()

# Transparent QPA Proxy is an explicitly version-coupled integration package. It is never part of
# hyremote-core and is opt-in because Qt does not guarantee QPA source/binary compatibility. The
# transparent mode composes the same product RemoteAccess runtime after the QPA-02 native semantics
# are preserved; it must not grow a parallel Session/transport/input implementation merely because
# its launch mechanism is a platform plugin. Run exact package qualification unconditionally so
# earlier public-Qt discovery cannot bypass the private-ABI gate merely because Qt6::Gui exists.
if(HYREMOTE_WITH_QPA_PROXY)
    if(NOT TARGET HyRemote::RemoteAccess)
        message(FATAL_ERROR
            "HYREMOTE_WITH_QPA_PROXY=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON and a usable HyRemote::RemoteAccess target")
    endif()
    find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui)
    if(NOT TARGET Qt6::GuiPrivate)
        message(FATAL_ERROR
            "HYREMOTE_WITH_QPA_PROXY=ON requires the exact Qt 6.8.3 private Gui development target (Qt6::GuiPrivate)")
    endif()
    add_subdirectory(qpa)
endif()

# V1 examples are release evidence for the three frozen integration modes. They consume the public
# product surface for their mode and must not create a second Session/transport/input architecture.
# E4 remains an ordinary Qt-only application; its QPA packaging path is opt-in inside that example.
if(HYREMOTE_BUILD_EXAMPLES)
    if(TARGET HyRemote::RemoteAccess)
        add_subdirectory(examples)
    elseif(PROJECT_IS_TOP_LEVEL)
        message(STATUS "  Product examples skipped: HyRemote::RemoteAccess is unavailable")
    else()
        message(FATAL_ERROR
            "HYREMOTE_BUILD_EXAMPLES=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON and a supported Qt SDK")
    endif()
endif()

# Throwaway architecture spike harnesses. They are not part of the production build graph.
if(HYREMOTE_BUILD_SPIKES)
    add_subdirectory(spikes/capture)
    add_subdirectory(spikes/async-capture)
endif()

# Install/export is part of the SDK contract. A bare Core-only package remains valid; when the Qt
# product facade is present it is exported from the same HyRemote package and namespace.
if(TARGET hyremote-core)
    include(HyRemoteInstall)
endif()

# Release metadata is repository-owned and must stay valid on both development and release-candidate
# trees. Branch/version authorization is enforced separately by the Git Flow policy workflow so the
# same full CTest suite remains runnable after release/vX.Y.Z.W sets the exact milestone version.
if(HYREMOTE_BUILD_TESTS AND PROJECT_IS_TOP_LEVEL)
    add_test(
        NAME hyremote-release-readiness-metadata
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DHYREMOTE_PROJECT_VERSION=${PROJECT_VERSION}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_release_metadata.cmake"
    )
endif()
