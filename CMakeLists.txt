cmake_minimum_required(VERSION 3.21)

# The declaration is kept on one line on purpose: the release-readiness metadata gate
# (tests/release-readiness/check_release_metadata.cmake) matches it with a regex whose character classes
# cover spaces and tabs but not newlines, so the previous multi-line form was rejected as
# "project(VERSION ...) is missing or malformed" and the gate could never pass. This is the only change to
# the declaration besides the #111 description.
project(HyRemote VERSION 0.0.0 DESCRIPTION "Qt Remote Access Framework" LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HyRemoteProjectOptions)
include(HyRemoteReleaseProfile)
include(HyRemoteDeploy)

# Source/add_subdirectory QML deployment metadata is configure-local truth even though the import
# root is carried through CMake's internal cache so a child module can publish it back to the
# consumer directory. Clear any value left by an earlier configure before deciding whether the
# current source build actually provides the optional QML payload.
unset(HyRemote_QML_IMPORT_PATH CACHE)
set_property(GLOBAL PROPERTY HYREMOTE_QML_SOURCE_DEPLOY_TARGETS "")

# Development integrates all V1 modes behind explicit options, but a milestone tag is a release fact:
# an earlier tagged product must not let users opt into a later, not-yet-accepted integration mode
# merely because its source already exists on develop. No extra user-facing release-profile option is
# introduced; the four-part project version itself defines the allowed released product surface.
hyremote_validate_release_profile(
    VERSION "${PROJECT_VERSION}"
    QML_ENABLED "${HYREMOTE_BUILD_QML_API}"
    QPA_ENABLED "${HYREMOTE_WITH_QPA_PROXY}"
    SECURITY_ENABLED "${HYREMOTE_WITH_TRANSPORT_SECURITY}"
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
message(STATUS "  Transport security:  ${HYREMOTE_WITH_TRANSPORT_SECURITY}")

# Repository layout is product-architecture aligned. Explicit binary directories deliberately retain
# the historical build-tree layout so this source-tree refactor does not change deployment/test
# artifact locations or create a second product personality.
if(HYREMOTE_BUILD_CORE)
    add_subdirectory(src/core core)
endif()

# A top-level configure that was told where Qt lives, or that was asked for the repository's own harnesses,
# must not quietly succeed with the Qt product missing. Measured on the current head: configuring against a
# Qt 5.15.2 prefix reported "Build succeeded" while `Qt6_DIR` was `Qt6_DIR-NOTFOUND` (nothing looks for Qt5)
# and the build had silently degraded to the Qt-free Core - 8 targets, no library, **zero examples**. A false
# green in a qualification attempt is worse than a failure, so this gate makes the intent explicit.
#
# It is deliberately scoped to PROJECT_IS_TOP_LEVEL: an add_subdirectory() consumer that only wants
# HyRemote::Core is a legitimate Qt-less build and must keep working, and so must building Core alone here.
if(PROJECT_IS_TOP_LEVEL
   AND (CMAKE_PREFIX_PATH OR HYREMOTE_BUILD_EXAMPLES OR HYREMOTE_BUILD_TESTS))
    if(NOT TARGET Qt6::Core)
        find_package(Qt6 QUIET COMPONENTS Core)
    endif()
    if(NOT TARGET Qt6::Core)
        message(FATAL_ERROR
            "This top-level configure expects Qt and did not find it.\n"
            "  CMAKE_PREFIX_PATH          : '${CMAKE_PREFIX_PATH}'\n"
            "  HYREMOTE_BUILD_EXAMPLES    : ${HYREMOTE_BUILD_EXAMPLES}\n"
            "  HYREMOTE_BUILD_TESTS       : ${HYREMOTE_BUILD_TESTS}\n"
            "Qt 6.8+ is the only qualified line today; Qt 5.15 qualification is tracked by issue #57 and is not\n"
            "implemented, so a Qt 5.15 prefix cannot configure this product yet.\n"
            "Pass a Qt 6.8.3 prefix, or build Core alone on purpose with -DHYREMOTE_BUILD_REMOTE_ACCESS=OFF\n"
            "-DHYREMOTE_BUILD_EXAMPLES=OFF -DHYREMOTE_BUILD_TESTS=OFF.")
    endif()
endif()

# The project targets Qt LTS lines. The reference line is 6.8 LTS, the public C++/QML path accepts newer 6.8+
# releases, and the Transparent QPA payload stays exactly 6.8.3 (see docs/architecture.md and docs/compatibility.md).
#
# A non-LTS Qt is not a reason to refuse to build - the user must not be blocked - but it must never be silent
# either. The build reports the line, and proceeds on the **6.8 API baseline** the rest of the configure already
# enforces (every feature search below asks for Qt 6.8), which is the most compatible configuration available for a
# line this project has not qualified.
if(TARGET Qt6::Core)
    set(HYREMOTE_QT_LTS_LINES 5.15 6.2 6.5 6.8)
    set(HYREMOTE_QT_LINE "${Qt6_VERSION_MAJOR}.${Qt6_VERSION_MINOR}")
    if(HYREMOTE_QT_LINE IN_LIST HYREMOTE_QT_LTS_LINES)
        message(STATUS "  Qt ${Qt6_VERSION} detected (LTS line ${HYREMOTE_QT_LINE})")
    else()
        message(WARNING
            "HyRemote: Qt ${Qt6_VERSION} is not an LTS line. This project targets Qt LTS lines "
            "(${HYREMOTE_QT_LTS_LINES}) and the qualified reference line is 6.8.3.\n"
            "  The build continues against the 6.8 API baseline and the public C++/QML surface, so the user is not "
            "blocked, but this Qt version is not qualified by this project.\n"
            "  The Transparent QPA payload is unaffected by this choice: it still requires exactly Qt 6.8.3 private "
            "ABI, and is skipped unless that exact SDK is present.\n"
            "  Use an LTS Qt (6.8.x recommended) for anything that has to be supported.")
    endif()
    unset(HYREMOTE_QT_LINE)
endif()

if(HYREMOTE_BUILD_REMOTE_ACCESS)
    if(NOT TARGET HyRemote::Core)
        message(FATAL_ERROR "HyRemote::RemoteAccess requires HYREMOTE_BUILD_CORE=ON")
    endif()

    if(NOT TARGET Qt6::Core OR NOT TARGET Qt6::Network)
        find_package(Qt6 6.8 QUIET COMPONENTS Core Network)
    endif()

    if(TARGET Qt6::Core AND TARGET Qt6::Network)
        add_subdirectory(src/remoteaccess remoteaccess)
    elseif(PROJECT_IS_TOP_LEVEL)
        message(STATUS "  RemoteAccess target skipped: Qt 6.8+ Core/Network not found")
    else()
        message(FATAL_ERROR
            "HYREMOTE_BUILD_REMOTE_ACCESS=ON requires Qt 6.8+ Core and Network. "
            "Find/provide the target Qt SDK before add_subdirectory(HyRemote), or disable the product target explicitly.")
    endif()
endif()

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
            add_subdirectory(src/qml/HyRemote qml/HyRemote)
        elseif(PROJECT_IS_TOP_LEVEL)
            message(STATUS "  QML API skipped: Qt 6.8+ Qml not found")
        else()
            message(FATAL_ERROR
                "HYREMOTE_BUILD_QML_API=ON requires Qt 6.8+ Qml. "
                "Provide the target Qt SDK before add_subdirectory(HyRemote), or disable the QML API explicitly.")
        endif()
    endif()
endif()

if(HYREMOTE_WITH_QPA_PROXY)
    if(NOT TARGET HyRemote::RemoteAccess)
        message(FATAL_ERROR
            "HYREMOTE_WITH_QPA_PROXY=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON and a usable HyRemote::RemoteAccess target")

    if(HYREMOTE_WITH_TRANSPORT_SECURITY AND NOT HYREMOTE_BUILD_REMOTE_ACCESS)
        message(FATAL_ERROR
            "HYREMOTE_WITH_TRANSPORT_SECURITY=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON: the security mode is a "
            "capability of the public RemoteAccess facade, not a standalone artifact.")
    endif()
    endif()

    # Transparent QPA is itself responsible for discovering both qualified V1 application surface
    # families. Imported Qt targets created while building RemoteAccess live in that child directory
    # scope and are not reliable sibling-scope inputs here. Re-resolve the exact Qt package with
    # Widgets + Quick so qhyremote never changes capability merely because examples/QML happened to
    # be enabled first. This keeps Widgets and Quick equal first-class surfaces in every QPA build.
    find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Widgets Quick)
    if(NOT TARGET Qt6::GuiPrivate)
        message(FATAL_ERROR
            "HYREMOTE_WITH_QPA_PROXY=ON requires the exact Qt 6.8.3 private Gui development target (Qt6::GuiPrivate)")
    endif()
    add_subdirectory(src/qpa qpa)
endif()

if(HYREMOTE_BUILD_EXAMPLES)
    # examples/CMakeLists.txt selects each example with directory-scoped TARGET checks such as
    # `if(TARGET Qt6::Widgets)`. Those imported targets are created inside src/remoteaccess and
    # src/qml, so they are not visible from this sibling scope: without resolving the same Qt
    # components here, a configure with HYREMOTE_BUILD_EXAMPLES=ON silently produces no example target at
    # all (verified on the current head: build/examples contained no example directory). Resolving them
    # here is what makes the enable flag mean what it says; it does not enable any Qt component by itself.
    find_package(Qt6 6.8 QUIET COMPONENTS Widgets Quick Qml)
    if(TARGET HyRemote::RemoteAccess)
        add_subdirectory(examples)
    elseif(PROJECT_IS_TOP_LEVEL)
        message(STATUS "  Product examples skipped: HyRemote::RemoteAccess is unavailable")
    else()
        message(FATAL_ERROR
            "HYREMOTE_BUILD_EXAMPLES=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON and a supported Qt SDK")
    endif()
endif()

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
    add_test(
        NAME hyremote-release-readiness-repository-layout
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_repository_layout.cmake"
    )
    add_test(
        NAME hyremote-release-readiness-release-documentation-layout
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_release_documentation_layout.cmake"
    )
    add_test(
        NAME hyremote-release-readiness-package-acquisition-isolation
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_package_acquisition_isolation.cmake"
    )
    add_test(
        NAME hyremote-release-readiness-ci-environment-baseline
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_ci_environment_baseline.cmake"
    )

    # Source deployment truth must come from a source graph that actually provides qhyremote. Focused
    # C++/Widgets/Quick/QML test graphs intentionally leave QPA disabled and must not manufacture a
    # false failure for metadata that cannot participate in that configure. The integrated/QPA jobs
    # create HyRemote::QpaPlatform and therefore execute this authority check.
    if(TARGET HyRemote::QpaPlatform)
        add_test(
            NAME hyremote-release-readiness-source-qpa-authority
            COMMAND "${CMAKE_COMMAND}"
                -DFIXTURE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/src/qpa/tests/deploy_helper_fixture
                -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/release-readiness-source-qpa-authority
                -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DTEST_QML=OFF
                -DTEST_STALE_QPA_NEGATIVE_METADATA=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/src/qpa/tests/run_deploy_helper_fixture.cmake"
        )
    endif()

    function(hyremote_add_release_profile_test test_name version qml_enabled qpa_enabled security_enabled expect_reject)
        add_test(
            NAME "${test_name}"
            COMMAND "${CMAKE_COMMAND}"
                -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DHYREMOTE_TEST_VERSION=${version}
                -DHYREMOTE_TEST_QML=${qml_enabled}
                -DHYREMOTE_TEST_QPA=${qpa_enabled}
                -DHYREMOTE_TEST_SECURITY=${security_enabled}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_release_profile.cmake"
        )
        if(expect_reject)
            set_tests_properties("${test_name}" PROPERTIES WILL_FAIL TRUE)
        endif()
    endfunction()

    hyremote_add_release_profile_test(hyremote-release-profile-develop-all-modes 0.0.0 ON ON ON FALSE)
hyremote_add_release_profile_test(hyremote-release-profile-v001-cpp-only 0.0.1.0 OFF OFF OFF FALSE)
hyremote_add_release_profile_test(hyremote-release-profile-v001-reject-qml 0.0.1.0 ON OFF OFF TRUE)
hyremote_add_release_profile_test(hyremote-release-profile-v001-reject-qpa 0.0.1.0 OFF ON OFF TRUE)
hyremote_add_release_profile_test(hyremote-release-profile-v001-reject-security 0.0.1.0 OFF OFF ON TRUE)
hyremote_add_release_profile_test(hyremote-release-profile-v002-qml 0.0.2.0 ON OFF OFF FALSE)
hyremote_add_release_profile_test(hyremote-release-profile-v002-reject-qpa 0.0.2.0 ON ON OFF TRUE)
hyremote_add_release_profile_test(hyremote-release-profile-v002-reject-security 0.0.2.0 ON OFF ON TRUE)
hyremote_add_release_profile_test(hyremote-release-profile-v003-all-modes 0.0.3.0 ON ON OFF FALSE)
hyremote_add_release_profile_test(hyremote-release-profile-v003-reject-security 0.0.3.0 ON ON ON TRUE)
hyremote_add_release_profile_test(hyremote-release-profile-v100-all-modes 1.0.0.0 ON ON ON FALSE)
endif()