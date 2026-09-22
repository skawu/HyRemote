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

# #143: the encrypted RFB profile, exercised through the real transport with a deterministic peer. It is only
# registered in a security-enabled tree, because the encrypted profile does not exist without the
# transport-security capability - that is a property of the product, not of the test.
if(HYREMOTE_TRANSPORT_SECURITY_AVAILABLE)
    add_executable(hyremote-runtime-secure-transport-test
        test_rfb_secure_transport.cpp
    )
    target_compile_features(hyremote-runtime-secure-transport-test PRIVATE cxx_std_17)
    target_include_directories(hyremote-runtime-secure-transport-test PRIVATE ../src)
    target_link_libraries(hyremote-runtime-secure-transport-test
        PRIVATE Qt6::Core Qt6::Network HyRemote::Core HyRemote::RemoteAccess OpenSSL::Crypto
    )
    add_test(NAME hyremote-runtime-secure-transport-test
             COMMAND hyremote-runtime-secure-transport-test)
    set_tests_properties(hyremote-runtime-secure-transport-test PROPERTIES TIMEOUT 180)
endif()

# Build/bootstrap authority is repository-level behavior, but registering it here keeps the self-test
# available in the current all-frontends CI profile without introducing another product target.
add_test(
    NAME hyremote-build-authority-selftest
    COMMAND "${CMAKE_COMMAND}"
        -DHYREMOTE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P "${CMAKE_SOURCE_DIR}/tests/build-authority/check_build_authority.cmake"
)
set_tests_properties(hyremote-build-authority-selftest PROPERTIES TIMEOUT 30)
