include_guard(GLOBAL)

# Phase C (#344): labels are additive metadata only. Capability guards, CTest identities,
# commands, timeouts, environments and existing special labels remain execution authority.
function(hyremote_label_test_family labels)
    foreach(_hyremote_test IN LISTS ARGN)
        if(TEST "${_hyremote_test}")
            set_property(TEST "${_hyremote_test}" APPEND PROPERTY LABELS "${labels}")
        endif()
    endforeach()
endfunction()

# #274 final reconciliation: TEST_CATALOG.md is the human authority for every registered CTest.
# Collect the configured tree at configure completion so a new registration cannot silently escape
# that catalog. This is intentionally a configure-time repository/T3 guard rather than another CTest:
# it adds no inventory, selector or execution-tier behavior of its own.
function(_hyremote_collect_registered_tests directory out_var)
    get_property(_hyremote_local_tests DIRECTORY "${directory}" PROPERTY TESTS)
    set(_hyremote_all_tests ${_hyremote_local_tests})

    get_property(_hyremote_subdirs DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(_hyremote_subdir IN LISTS _hyremote_subdirs)
        _hyremote_collect_registered_tests("${_hyremote_subdir}" _hyremote_child_tests)
        list(APPEND _hyremote_all_tests ${_hyremote_child_tests})
    endforeach()

    set(${out_var} "${_hyremote_all_tests}" PARENT_SCOPE)
endfunction()

function(hyremote_assert_test_catalog_complete)
    set(_hyremote_catalog "${PROJECT_SOURCE_DIR}/tests/TEST_CATALOG.md")
    if(NOT EXISTS "${_hyremote_catalog}")
        message(FATAL_ERROR "#274 authority is missing: ${_hyremote_catalog}")
    endif()

    file(READ "${_hyremote_catalog}" _hyremote_catalog_text)
    _hyremote_collect_registered_tests("${PROJECT_SOURCE_DIR}" _hyremote_registered_tests)
    list(REMOVE_DUPLICATES _hyremote_registered_tests)
    list(SORT _hyremote_registered_tests)

    set(_hyremote_missing_tests "")
    foreach(_hyremote_test IN LISTS _hyremote_registered_tests)
        # Exact Markdown code spans avoid accidental substring matches between similarly named tests.
        string(FIND "${_hyremote_catalog_text}" "`${_hyremote_test}`" _hyremote_catalog_pos)
        if(_hyremote_catalog_pos EQUAL -1)
            list(APPEND _hyremote_missing_tests "${_hyremote_test}")
        endif()
    endforeach()

    if(_hyremote_missing_tests)
        string(JOIN "\n  - " _hyremote_missing_lines ${_hyremote_missing_tests})
        message(FATAL_ERROR
            "tests/TEST_CATALOG.md is missing registered CTest identities:\n"
            "  - ${_hyremote_missing_lines}\n"
            "Update the #274 catalog in the same change that adds or renames a CTest.")
    endif()
endfunction()

# Root/T3-T6 tests are registered after module subdirectories return. This function is scheduled
# from the top-level release-profile module to execute in the top-level CMake directory after those
# registrations exist, so their properties are still set in the directory that owns the tests.
function(hyremote_apply_root_semantic_test_labels)
    hyremote_label_test_family("contract;repository;fast"
        hyremote-build-authority-selftest
        hyremote-release-readiness-runtime-contract
        hyremote-release-readiness-repository-layout
        hyremote-release-readiness-documentation-paths
        hyremote-release-readiness-ci-environment-baseline
        hyremote-release-readiness-licensing-boundary
        hyremote-ci-scope-self-test
        hyremote-mainline-audit-self-test
        hyremote-branch-name-gate-self-test
    )

    hyremote_label_test_family("contract;consumer;fast"
        hyremote-acquisition-audit-self-test
    )
    hyremote_label_test_family("contract;consumer;installed"
        hyremote-release-readiness-security-runtime-deploy
        hyremote-release-readiness-build-install-contract
        hyremote-release-readiness-deployment-relocation
        hyremote-release-readiness-deploy-helper-contract
        hyremote-release-readiness-consumer-simplicity
        hyremote-release-readiness-package-acquisition-isolation
        hyremote-release-readiness-source-qpa-authority
    )
    hyremote_label_test_family("consumer;cpp;installed"
        hyremote-cpp-installed-consumers
    )
    hyremote_label_test_family("consumer;generic;installed"
        hyremote-generic-installed-consumers
    )

    hyremote_label_test_family("e2e;cpp;e2e"
        hyremote-v01-example-smoke
    )

    hyremote_label_test_family("release;repository;qualification"
        hyremote-release-readiness-metadata
        hyremote-release-readiness-release-authority-policy
        hyremote-release-readiness-release-documentation-layout
        hyremote-release-scope-self-test
        hyremote-release-authority-v02-user-first
        hyremote-release-profile-develop-all
        hyremote-release-profile-develop-runtime-only
        hyremote-release-profile-retire-v001
        hyremote-release-profile-retire-v002
        hyremote-release-profile-retire-v003
        hyremote-release-profile-v010-cpp-only
        hyremote-release-profile-v020-runtime
        hyremote-release-profile-v030-all
        hyremote-release-profile-v040-all
        hyremote-release-profile-v040-maintenance
        hyremote-release-profile-v100-all
        hyremote-release-profile-v100-cpp-only
        hyremote-release-profile-v100-generic-only
    )

    hyremote_assert_test_catalog_complete()
endfunction()

# QPA's T4 deploy-helper matrix is registered by the parent QPA directory after tests/ returns.
function(hyremote_apply_qpa_deploy_semantic_test_labels)
    hyremote_label_test_family("contract;qpa;installed"
        hyremote-qpa-deploy-helper-ordinary
        hyremote-qpa-deploy-helper-qml-only
        hyremote-qpa-deploy-helper-qml-composed
        hyremote-qpa-deploy-helper-installed-payload
        hyremote-qpa-deploy-helper-installed-payload-qml-only
        hyremote-qpa-deploy-helper-installed-payload-qml
        hyremote-qpa-deploy-helper-reject-missing-qml
        hyremote-qpa-deploy-helper-reject-stale-qml-metadata
        hyremote-qpa-deploy-helper-reject-missing-qml-root
        hyremote-qpa-deploy-helper-reject-missing-qml-module-dir
        hyremote-qpa-deploy-helper-reject-stale-qpa-metadata
        hyremote-qpa-deploy-helper-reject-qt-mismatch
        hyremote-qpa-deploy-helper-reject-missing-package
        hyremote-qpa-deploy-helper-single-config-generator
        hyremote-qpa-deploy-helper-multi-config-generator
        hyremote-qpa-source-payload-relocation
    )
endfunction()
