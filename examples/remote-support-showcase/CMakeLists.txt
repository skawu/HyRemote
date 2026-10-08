cmake_minimum_required(VERSION 3.21)

project(HyRemoteRemoteSupportShowcase LANGUAGES CXX)

add_executable(hyremote-remote-support-showcase
    main.cpp
    input_probe.cpp
    ../hyremote_branding.cpp
    ../../logo/hyremote-branding.qrc
)
set_target_properties(hyremote-remote-support-showcase PROPERTIES OUTPUT_NAME hyremote-tool)

target_compile_features(hyremote-remote-support-showcase PRIVATE cxx_std_17)
target_link_libraries(hyremote-remote-support-showcase
    PRIVATE
        Qt6::Widgets
        Qt6::Network
        HyRemote::RemoteAccess
)

# It is a user-facing example like the learning ones: it installs into the same shared prefix and is deployed by the
# same helper, so it is reachable from the product tree instead of only from the build tree.
install(TARGETS hyremote-remote-support-showcase RUNTIME DESTINATION bin)
hyremote_deploy(TARGET hyremote-remote-support-showcase)

if(HYREMOTE_BUILD_TESTS)
    add_test(
        NAME hyremote-tool-diagnostic-export-test
        COMMAND "${CMAKE_COMMAND}"
            -DTEST_EXE=$<TARGET_FILE:hyremote-remote-support-showcase>
            -DRUNTIME_DIR=$<TARGET_FILE_DIR:hyremote-remoteaccess>
            -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/hyremote-tool-diagnostic-export.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_diagnostic_export_test.cmake
    )
    set_tests_properties(hyremote-tool-diagnostic-export-test PROPERTIES TIMEOUT 20)
    if(COMMAND hyremote_label_test_family)
        hyremote_label_test_family("e2e;tool;fast" hyremote-tool-diagnostic-export-test)
    endif()
endif()
