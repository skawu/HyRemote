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

# Root/T3-T6 tests are registered after module subdirectories return. This function is scheduled
# from Core's test directory to execute in the top-level CMake directory after those registrations
# exist, so their properties are still set in the directory that owns the tests.
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
