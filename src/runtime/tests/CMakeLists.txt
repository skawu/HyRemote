# Application-level automatic access controller tests, plus the private notification seam and the
# build/bootstrap authority self-test. These were only ever reachable because the whole tests
# directory used to be entered behind HYREMOTE_AUTOMATIC_RUNTIME_AVAILABLE, so they keep that same
# effective guard here: freeing the Runtime-owned regressions below must not silently change what the
# runtime-only and frontend-less configurations register.
if(HYREMOTE_AUTOMATIC_RUNTIME_AVAILABLE)
    add_executable(hyremote-runtime-automatic-surface-model-test
        automatic_application_surface_model_test.cpp
        ../src/automatic/application_surface_model.cpp
    )
    target_compile_features(hyremote-runtime-automatic-surface-model-test PRIVATE cxx_std_17)
    target_include_directories(hyremote-runtime-automatic-surface-model-test PRIVATE ../src)
    target_link_libraries(hyremote-runtime-automatic-surface-model-test PRIVATE Qt6::Core)
    add_test(NAME hyremote-runtime-automatic-surface-model-test
             COMMAND hyremote-runtime-automatic-surface-model-test)
    set_tests_properties(hyremote-runtime-automatic-surface-model-test PROPERTIES TIMEOUT 15)

    add_executable(hyremote-runtime-automatic-composite-capture-test
        automatic_composite_capture_test.cpp
        ../src/automatic/application_surface_model.cpp
        ../src/automatic/composite_target.cpp
    )
    target_compile_features(hyremote-runtime-automatic-composite-capture-test PRIVATE cxx_std_17)
    target_include_directories(hyremote-runtime-automatic-composite-capture-test PRIVATE ../src)
    target_link_libraries(hyremote-runtime-automatic-composite-capture-test
        PRIVATE Qt6::Core Qt6::Gui HyRemote::Core HyRemote::RemoteAccess
    )
    add_test(NAME hyremote-runtime-automatic-composite-capture-test
             COMMAND hyremote-runtime-automatic-composite-capture-test)
    set_tests_properties(hyremote-runtime-automatic-composite-capture-test PROPERTIES TIMEOUT 15)

    add_executable(hyremote-runtime-automatic-composite-input-test
        automatic_composite_input_test.cpp
        ../src/automatic/application_surface_model.cpp
        ../src/automatic/composite_target.cpp
        ../src/automatic/composite_target_routing.cpp
        ../src/automatic/interactive_composite_target.cpp
    )
    target_compile_features(hyremote-runtime-automatic-composite-input-test PRIVATE cxx_std_17)
    target_include_directories(hyremote-runtime-automatic-composite-input-test PRIVATE ../src)
    target_link_libraries(hyremote-runtime-automatic-composite-input-test
        PRIVATE Qt6::Core Qt6::Gui HyRemote::Core HyRemote::RemoteAccess
    )
    add_test(NAME hyremote-runtime-automatic-composite-input-test
             COMMAND hyremote-runtime-automatic-composite-input-test)
    set_tests_properties(hyremote-runtime-automatic-composite-input-test PROPERTIES TIMEOUT 15)

    # #259: the private typed notification seam of the shared runtime. It drives real event boundaries
    # through the private test factories, so a notification is proven by the event that produced it rather
    # than by a timer or a wall-clock wait.
    add_executable(hyremote-runtime-notifications-test
        access_instance_notifications_test.cpp
    )
    target_compile_features(hyremote-runtime-notifications-test PRIVATE cxx_std_17)
    target_include_directories(hyremote-runtime-notifications-test PRIVATE ../src)
    target_link_libraries(hyremote-runtime-notifications-test
        PRIVATE Qt6::Core HyRemote::Core HyRemote::RemoteAccess
    )
    add_test(NAME hyremote-runtime-notifications-test
             COMMAND hyremote-runtime-notifications-test)
    set_tests_properties(hyremote-runtime-notifications-test PROPERTIES TIMEOUT 60)

    # Build/bootstrap authority is repository-level behavior, but registering it here keeps the self-test
    # available in the current all-frontends CI profile without introducing another product target.
    # Moving this registration to its own owner is a later slice, so it keeps the guard it had here
    # instead of being newly registered in configurations that never had it.
    add_test(
        NAME hyremote-build-authority-selftest
        COMMAND "${CMAKE_COMMAND}"
            -DHYREMOTE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P "${CMAKE_SOURCE_DIR}/tests/build-authority/check_build_authority.cmake"
    )
    set_tests_properties(hyremote-build-authority-selftest PROPERTIES TIMEOUT 30)
endif()

# Runtime-owned regressions. These exercise Shared Runtime internals through the C++ test-exports the
# runtime target already exposes to the build tree, so they register whenever tests are enabled -
# not only when the C++ frontend or the application-level automatic controller is built.
add_executable(hyremote-security-descriptor-test
    test_security_descriptor.cpp
    ../src/detail/security_descriptor.cpp
)
target_link_libraries(hyremote-security-descriptor-test
    PRIVATE
        hyremote-remoteaccess
        Qt6::Core
        Qt6::Network
)
target_include_directories(hyremote-security-descriptor-test
    PRIVATE
        ../src
)
add_test(NAME hyremote-security-descriptor-test COMMAND hyremote-security-descriptor-test)
set_tests_properties(hyremote-security-descriptor-test PROPERTIES TIMEOUT 15)

add_executable(hyremote-target-component-provider-test
    test_target_component_provider.cpp
)
target_link_libraries(hyremote-target-component-provider-test
    PRIVATE
        hyremote-remoteaccess
        HyRemote::Core
        Qt6::Core
)
target_include_directories(hyremote-target-component-provider-test
    PRIVATE
        ../src
)
add_test(NAME hyremote-target-component-provider-test COMMAND hyremote-target-component-provider-test)

add_executable(hyremote-input-mailbox-admission-test
    test_input_mailbox_admission.cpp
)
target_link_libraries(hyremote-input-mailbox-admission-test
    PRIVATE
        hyremote-remoteaccess
        HyRemote::Core
        Qt6::Core
)
target_include_directories(hyremote-input-mailbox-admission-test
    PRIVATE
        ../src
)
add_test(NAME hyremote-input-mailbox-admission-test COMMAND hyremote-input-mailbox-admission-test)
set_tests_properties(hyremote-input-mailbox-admission-test PROPERTIES TIMEOUT 15)

if(HYREMOTE_WITH_VNC)
    # Deterministic transport-level regression for simultaneous viewers sharing one Qt target.
    add_executable(hyremote-rfb-multi-client-input-test
        test_rfb_multi_client_input.cpp
    )
    target_link_libraries(hyremote-rfb-multi-client-input-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Network
    )
    target_include_directories(hyremote-rfb-multi-client-input-test
        PRIVATE
            ../src
    )
    add_test(NAME hyremote-rfb-multi-client-input-test COMMAND hyremote-rfb-multi-client-input-test)
    set_tests_properties(hyremote-rfb-multi-client-input-test PROPERTIES TIMEOUT 15)
endif()
