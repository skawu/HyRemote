cmake_minimum_required(VERSION 3.21)

project(
    HyRemote
    VERSION 0.0.0
    DESCRIPTION "Qt Embedded Remote Access Framework"
    LANGUAGES C CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HyRemoteProjectOptions)
include(HyRemoteReleaseProfile)
include(HyRemoteDeploy)

# Development integrates all V1 modes behind explicit options, but a milestone tag is a release fact:
# an earlier tagged product must not let users opt into a later, not-yet-accepted integration mode
# merely because its source already exists on develop. No extra user-facing release-profile option is
# introduced; the four-part project version itself defines the allowed released product surface.
hyremote_validate_release_profile(
    VERSION "${PROJECT_VERSION}"
    QML_ENABLED "${HYREMOTE_BUILD_QML_API}"
    QPA_ENABLED "${HYREMOTE_WITH_QPA_PROXY}"
)

# A normal add_subdirectory(HyRemote) consumer must not acquire CTest/BUILD_TESTING side effects just
# by using the library. HyRemote's own tests are a developer concern and are explicitly opt-in.
if(HYREMOTE_BUILD_TESTS)
    include(CTest)
endif()

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
# transparent mode composes the same product RemoteAccess runtime after native semantics are preserved;
# it must not grow a parallel Session/transport/input implementation merely because its launch
# mechanism is a platform plugin. Run exact package qualification unconditionally so earlier public-Qt
# discovery cannot bypass the private-ABI gate merely because Qt6::Gui exists.
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

# The V1 installed SDK exists only when the product facade exists. Core remains a source/internal
# static composition target and is deliberately not exported as a second normal SDK product.
if(TARGET hyremote-remoteaccess)
    include(HyRemoteInstall)
endif()

# Release metadata, simple consumer behavior and milestone product surfaces are repository-owned and
# must stay valid on development and release-candidate trees. Branch/version authorization is enforced
# separately by the Git Flow policy workflow.
if(HYREMOTE_BUILD_TESTS AND PROJECT_IS_TOP_LEVEL)
    add_test(
        NAME hyremote-release-readiness-metadata
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DHYREMOTE_PROJECT_VERSION=${PROJECT_VERSION}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_release_metadata.cmake"
    )
    add_test(
        NAME hyremote-release-readiness-runtime-contract
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_v1_runtime_contract.cmake"
    )
    add_test(
        NAME hyremote-release-readiness-deployment-relocation
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_deployment_relocation.cmake"
    )
    add_test(
        NAME hyremote-release-readiness-deploy-helper-contract
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_deploy_helper_contract.cmake"
    )
    add_test(
        NAME hyremote-release-readiness-release-authority-policy
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_release_authority_policy.cmake"
    )
    add_test(
        NAME hyremote-release-readiness-consumer-simplicity
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_consumer_simplicity.cmake"
    )

    function(hyremote_add_release_profile_test test_name version qml_enabled qpa_enabled expect_reject)
        add_test(
            NAME "${test_name}"
            COMMAND "${CMAKE_COMMAND}"
                -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DHYREMOTE_TEST_VERSION=${version}
                -DHYREMOTE_TEST_QML=${qml_enabled}
                -DHYREMOTE_TEST_QPA=${qpa_enabled}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_release_profile.cmake"
        )
        if(expect_reject)
            set_tests_properties("${test_name}" PROPERTIES WILL_FAIL TRUE)
        endif()
    endfunction()

    hyremote_add_release_profile_test(hyremote-release-profile-develop-all-modes 0.0.0 ON ON FALSE)
    hyremote_add_release_profile_test(hyremote-release-profile-v001-cpp-only 0.0.1.0 OFF OFF FALSE)
    hyremote_add_release_profile_test(hyremote-release-profile-v001-reject-qml 0.0.1.0 ON OFF TRUE)
    hyremote_add_release_profile_test(hyremote-release-profile-v001-reject-qpa 0.0.1.0 OFF ON TRUE)
    hyremote_add_release_profile_test(hyremote-release-profile-v002-qml 0.0.2.0 ON OFF FALSE)
    hyremote_add_release_profile_test(hyremote-release-profile-v002-reject-qpa 0.0.2.0 ON ON TRUE)
    hyremote_add_release_profile_test(hyremote-release-profile-v003-all-modes 0.0.3.0 ON ON FALSE)
    hyremote_add_release_profile_test(hyremote-release-profile-v100-all-modes 1.0.0.0 ON ON FALSE)
endif()
