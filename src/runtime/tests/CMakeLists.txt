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

# Build/bootstrap authority is repository-level behavior, but registering it here keeps the self-test
# available in the current all-frontends CI profile without introducing another product target.
add_test(
    NAME hyremote-build-authority-selftest
    COMMAND "${CMAKE_COMMAND}"
        -DHYREMOTE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P "${CMAKE_SOURCE_DIR}/tests/build-authority/check_build_authority.cmake"
)
set_tests_properties(hyremote-build-authority-selftest PROPERTIES TIMEOUT 30)
