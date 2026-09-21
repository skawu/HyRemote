cmake_minimum_required(VERSION 3.21)

# The declaration is kept on one line on purpose: the release-readiness metadata gate matches it.
project(HyRemote VERSION 0.0.0 DESCRIPTION "Qt Remote Access Framework" LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(HyRemoteProjectOptions)
include(HyRemoteReleaseProfile)
include(HyRemoteDeploy)

# Source/add_subdirectory QML deployment metadata is configure-local truth even though the import
# root is carried through CMake's internal cache so a child module can publish it back to the
# consumer directory.
unset(HyRemote_QML_IMPORT_PATH CACHE)
set_property(GLOBAL PROPERTY HYREMOTE_QML_SOURCE_DEPLOY_TARGETS "")

# Release versions identify coherent product states. Frontends are peer capability dimensions and
# must never be sequenced through version digits.
hyremote_validate_release_profile(
    VERSION "${PROJECT_VERSION}"
    CPP_ENABLED "${HYREMOTE_BUILD_CPP_API}"
    QML_ENABLED "${HYREMOTE_BUILD_QML_API}"
    GENERIC_ENABLED "${HYREMOTE_WITH_GENERIC_PLUGIN}"
    QPA_ENABLED "${HYREMOTE_WITH_QPA_PROXY}"
    SECURITY_ENABLED "${HYREMOTE_WITH_TRANSPORT_SECURITY}"
)

if(HYREMOTE_BUILD_TESTS)
    include(CTest)
endif()

message(STATUS "Configuring HyRemote ${PROJECT_VERSION}")
message(STATUS "  Build core:          ${HYREMOTE_BUILD_CORE}")
message(STATUS "  Build runtime:       ${HYREMOTE_BUILD_REMOTE_ACCESS}")
message(STATUS "  C++ API frontend:    ${HYREMOTE_BUILD_CPP_API}")
message(STATUS "  QML API frontend:    ${HYREMOTE_BUILD_QML_API}")
message(STATUS "  Generic Plugin:      ${HYREMOTE_WITH_GENERIC_PLUGIN}")
message(STATUS "  QPA frontend:        ${HYREMOTE_WITH_QPA_PROXY}")
message(STATUS "  Build tests:         ${HYREMOTE_BUILD_TESTS}")
message(STATUS "  Build examples:      ${HYREMOTE_BUILD_EXAMPLES}")
message(STATUS "  VNC backend:         ${HYREMOTE_WITH_VNC}")
message(STATUS "  Transport security:  ${HYREMOTE_WITH_TRANSPORT_SECURITY}")

# Shared implementation stays in src/core and src/runtime; application-facing frontends are peer
# siblings under src/integrations.
if(HYREMOTE_BUILD_CORE)
    add_subdirectory(src/core core)
endif()

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

if(TARGET Qt6::Core)
    set(HYREMOTE_QT_LTS_LINES 5.15 6.2 6.5 6.8)
    set(HYREMOTE_QT_LINE "${Qt6_VERSION_MAJOR}.${Qt6_VERSION_MINOR}")
    if(HYREMOTE_QT_LINE IN_LIST HYREMOTE_QT_LTS_LINES)
        message(STATUS "  Qt ${Qt6_VERSION} detected (LTS line ${HYREMOTE_QT_LINE})")
    else()
        message(WARNING
            "HyRemote: Qt ${Qt6_VERSION} is not an LTS line. This project targets Qt LTS lines "
            "(${HYREMOTE_QT_LTS_LINES}) and the qualified reference line is 6.8.3.\n"
            "  The build continues against the public-Qt API baseline but is not a qualified product combination.\n"
            "  The QPA payload remains separately exact-private-ABI qualified.")
    endif()
    unset(HYREMOTE_QT_LINE)
endif()

if(HYREMOTE_BUILD_REMOTE_ACCESS)
    if(NOT TARGET HyRemote::Core)
        message(FATAL_ERROR "HyRemote Common Runtime requires HYREMOTE_BUILD_CORE=ON")
    endif()

    if(NOT TARGET Qt6::Core OR NOT TARGET Qt6::Network)
        find_package(Qt6 6.8 QUIET COMPONENTS Core Network)
    endif()

    if(TARGET Qt6::Core AND TARGET Qt6::Network)
        add_subdirectory(src/runtime runtime)
        if(HYREMOTE_BUILD_CPP_API)
            add_subdirectory(src/integrations/cpp integrations/cpp)
        endif()
    elseif(PROJECT_IS_TOP_LEVEL)
        message(STATUS "  Common Runtime skipped: Qt 6.8+ Core/Network not found")
    else()
        message(FATAL_ERROR
            "HYREMOTE_BUILD_REMOTE_ACCESS=ON requires Qt 6.8+ Core and Network. "
            "Find/provide the target Qt SDK before add_subdirectory(HyRemote), or disable the runtime explicitly.")
    endif()
endif()

if(HYREMOTE_BUILD_QML_API)
    if(NOT TARGET hyremote-remoteaccess)
        if(PROJECT_IS_TOP_LEVEL)
            message(STATUS "  QML API skipped: Common Runtime is unavailable")
        else()
            message(FATAL_ERROR "HYREMOTE_BUILD_QML_API=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON")
        endif()
    else()
        if(NOT TARGET Qt6::Qml)
            find_package(Qt6 6.8 QUIET COMPONENTS Qml)
        endif()

        if(TARGET Qt6::Qml)
            add_subdirectory(src/integrations/qml qml/HyRemote)
        elseif(PROJECT_IS_TOP_LEVEL)
            message(STATUS "  QML API skipped: Qt 6.8+ Qml not found")
        else()
            message(FATAL_ERROR
                "HYREMOTE_BUILD_QML_API=ON requires Qt 6.8+ Qml. "
                "Provide the target Qt SDK before add_subdirectory(HyRemote), or disable the QML API explicitly.")
        endif()
    endif()
endif()

if(HYREMOTE_WITH_GENERIC_PLUGIN)
    if(NOT TARGET hyremote-remoteaccess)
        message(FATAL_ERROR
            "HYREMOTE_WITH_GENERIC_PLUGIN=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON and the Common Runtime")
    endif()
    if(NOT HYREMOTE_AUTOMATIC_RUNTIME_AVAILABLE)
        message(FATAL_ERROR
            "HYREMOTE_WITH_GENERIC_PLUGIN=ON requires the common automatic-access runtime")
    endif()

    # Generic preserves the application's native QPA and uses public Qt APIs only.
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Quick)
    add_subdirectory(src/integrations/generic generic)
endif()

if(HYREMOTE_WITH_QPA_PROXY)
    if(NOT TARGET hyremote-remoteaccess)
        message(FATAL_ERROR
            "HYREMOTE_WITH_QPA_PROXY=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON and the Common Runtime")
    endif()

    find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Widgets Quick)
    if(NOT TARGET Qt6::GuiPrivate)
        message(FATAL_ERROR
            "HYREMOTE_WITH_QPA_PROXY=ON requires the exact Qt 6.8.3 private Gui development target (Qt6::GuiPrivate)")
    endif()
    add_subdirectory(src/integrations/qpa qpa)
endif()

if(HYREMOTE_BUILD_EXAMPLES)
    find_package(Qt6 6.8 QUIET COMPONENTS Widgets Quick Qml)
    if(TARGET hyremote-remoteaccess)
        add_subdirectory(examples)
    elseif(PROJECT_IS_TOP_LEVEL)
        message(STATUS "  Product examples skipped: Common Runtime is unavailable")
    else()
        message(FATAL_ERROR
            "HYREMOTE_BUILD_EXAMPLES=ON requires HYREMOTE_BUILD_REMOTE_ACCESS=ON and a supported Qt SDK")
    endif()
endif()

if(TARGET hyremote-remoteaccess)
    include(HyRemoteInstall)
endif()

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
            -DHYREMOTE_GATE_GENERATOR=${CMAKE_GENERATOR}
            -DHYREMOTE_GATE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
            -DHYREMOTE_GATE_C_COMPILER=${CMAKE_C_COMPILER}
            -DHYREMOTE_GATE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
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
        NAME hyremote-release-readiness-documentation-paths
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_documentation_paths.cmake"
    )
    add_test(
        NAME hyremote-release-readiness-licensing-boundary
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_licensing_boundary.cmake"
    )
    add_test(
        NAME hyremote-release-readiness-package-acquisition-isolation
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DHYREMOTE_GATE_SCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/gate-scratch
            -DHYREMOTE_GATE_GENERATOR=${CMAKE_GENERATOR}
            -DHYREMOTE_GATE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
            -DHYREMOTE_GATE_C_COMPILER=${CMAKE_C_COMPILER}
            -DHYREMOTE_GATE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_package_acquisition_isolation.cmake"
    )
    add_test(
        NAME hyremote-release-readiness-ci-environment-baseline
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_ci_environment_baseline.cmake"
    )

    if(TARGET hyremote-generic-plugin)
        # The installed Generic consumers run through the release evidence runner's own cell mechanism, so the
        # hosted lanes exercise the same clean-install/deploy/run path the release evidence uses - as a normal
        # product integration test through the canonical build authority, not as a CI-only command.
        add_test(
            NAME hyremote-generic-installed-consumers
            COMMAND "${CMAKE_COMMAND}"
                -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DHYREMOTE_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
                "-DEVIDENCE_CELLS=installed-generic-widgets;installed-generic-quick"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/run_release_evidence.cmake"
        )
        set_tests_properties(hyremote-generic-installed-consumers PROPERTIES TIMEOUT 900)
    endif()

    # Release-train authority selection and its acceptance matrix, registered as an ordinary product test so the
    # canonical build authority runs it: a release scope must be selected by the requested version, fail closed on
    # anything it cannot classify, and never treat a representable train as a blocker for an earlier one.
    add_test(
        NAME hyremote-release-scope-self-test
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DHYREMOTE_SCOPE_SELF_TEST=ON
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/release_scope.cmake"
    )
    set_tests_properties(hyremote-release-scope-self-test PROPERTIES TIMEOUT 300)

    if(TARGET HyRemote::QpaPlatform)
        add_test(
            NAME hyremote-release-readiness-source-qpa-authority
            COMMAND "${CMAKE_COMMAND}"
                -DFIXTURE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/src/integrations/qpa/tests/deploy_helper_fixture
                -DFIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/release-readiness-source-qpa-authority
                -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DTEST_QML=OFF
                -DTEST_STALE_QPA_NEGATIVE_METADATA=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/src/integrations/qpa/tests/run_deploy_helper_fixture.cmake"
        )
    endif()

    function(hyremote_add_release_profile_test
             test_name version cpp_enabled qml_enabled generic_enabled qpa_enabled security_enabled expect_reject)
        add_test(
            NAME "${test_name}"
            COMMAND "${CMAKE_COMMAND}"
                -DHYREMOTE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DHYREMOTE_TEST_VERSION=${version}
                -DHYREMOTE_TEST_CPP=${cpp_enabled}
                -DHYREMOTE_TEST_QML=${qml_enabled}
                -DHYREMOTE_TEST_GENERIC=${generic_enabled}
                -DHYREMOTE_TEST_QPA=${qpa_enabled}
                -DHYREMOTE_TEST_SECURITY=${security_enabled}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/release-readiness/check_release_profile.cmake"
        )
        if(expect_reject)
            set_tests_properties("${test_name}" PROPERTIES WILL_FAIL TRUE)
        endif()
    endfunction()

    # Development sentinel accepts arbitrary capability combinations while V1 converges.
    hyremote_add_release_profile_test(
        hyremote-release-profile-develop-all 0.0.0 ON ON ON ON ON FALSE)
    hyremote_add_release_profile_test(
        hyremote-release-profile-develop-runtime-only 0.0.0 OFF OFF OFF OFF OFF FALSE)

    # Historical V0 labels are planning references only and must never become release profiles again.
    hyremote_add_release_profile_test(
        hyremote-release-profile-retire-v001 0.0.1.0 ON OFF OFF OFF OFF TRUE)
    hyremote_add_release_profile_test(
        hyremote-release-profile-retire-v002 0.0.2.0 ON ON OFF OFF OFF TRUE)
    hyremote_add_release_profile_test(
        hyremote-release-profile-retire-v003 0.0.3.0 ON ON OFF ON OFF TRUE)

    # Progressive trains are real releases, so they must be accepted whatever frontends a build enables. A
    # frontend subset is a capability choice, never a version meaning: these cases would have failed under the
    # retired rule that rejected every value below the first GA.
    hyremote_add_release_profile_test(
        hyremote-release-profile-v010-cpp-only 0.1.0.0 ON OFF OFF OFF OFF FALSE)
    hyremote_add_release_profile_test(
        hyremote-release-profile-v020-runtime 0.2.0.0 ON OFF OFF OFF OFF FALSE)
    hyremote_add_release_profile_test(
        hyremote-release-profile-v030-all 0.3.0.0 ON ON ON ON ON FALSE)
    hyremote_add_release_profile_test(
        hyremote-release-profile-v040-all 0.4.0.0 ON ON ON ON ON FALSE)
    # V0.4.0.x maintenance releases stay inside the V0.4 line.
    hyremote_add_release_profile_test(
        hyremote-release-profile-v040-maintenance 0.4.0.5 ON ON ON ON ON FALSE)

    # V1 is the first formal GA. Frontend subsets do not encode version semantics.
    hyremote_add_release_profile_test(
        hyremote-release-profile-v100-all 1.0.0.0 ON ON ON ON ON FALSE)
    hyremote_add_release_profile_test(
        hyremote-release-profile-v100-cpp-only 1.0.0.0 ON OFF OFF OFF OFF FALSE)
    hyremote_add_release_profile_test(
        hyremote-release-profile-v100-generic-only 1.0.0.0 OFF OFF ON OFF OFF FALSE)
endif()
