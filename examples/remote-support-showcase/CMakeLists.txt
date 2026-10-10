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
    # This is a real listener/TCP GUI regression. Do not register it when
    # the RFB backend is disabled and RemoteAccess has no default transport.
    if(HYREMOTE_WITH_VNC)
        add_executable(hyremote-tool-listener-port-test test_listener_port_gui.cpp)
        target_compile_features(hyremote-tool-listener-port-test PRIVATE cxx_std_17)
        target_link_libraries(hyremote-tool-listener-port-test PRIVATE
            Qt6::Widgets Qt6::Network HyRemote::RemoteAccess)
        add_test(NAME hyremote-tool-listener-port-test COMMAND hyremote-tool-listener-port-test)
        set_tests_properties(hyremote-tool-listener-port-test PROPERTIES
            TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
        if(COMMAND hyremote_label_test_family)
            hyremote_label_test_family("e2e;tool;fast" hyremote-tool-listener-port-test)
        endif()
    endif()

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

    # This is repository-level release evidence, not a test of an external add_subdirectory() consumer.
    # The nested showcase has its own project(), so PROJECT_IS_TOP_LEVEL is not the root authority here.
    # Register only when HyRemote itself is the top-level source tree.
    if(CMAKE_SOURCE_DIR STREQUAL HyRemote_SOURCE_DIR)
        add_test(
            NAME hyremote-tool-installed-diagnostic
            COMMAND "${CMAKE_COMMAND}"
                -DHYREMOTE_SOURCE_DIR=${HyRemote_SOURCE_DIR}
                -DHYREMOTE_BUILD_DIR=${HyRemote_BINARY_DIR}
                "-DEVIDENCE_CELLS=installed-tool"
                -P "${HyRemote_SOURCE_DIR}/tests/release-readiness/run_release_evidence.cmake"
        )
        set_tests_properties(hyremote-tool-installed-diagnostic PROPERTIES TIMEOUT 600)
        if(COMMAND hyremote_label_test_family)
            hyremote_label_test_family("consumer;tool;installed" hyremote-tool-installed-diagnostic)
        endif()
    endif()

    if(HYREMOTE_WITH_VNC)
        # Standard-viewer product fit is exact-candidate evidence, not a normal fast-regression dependency.
        # Register fail-closed so candidate qualification cannot pass because viewer tooling is absent.
        set(Python3_FIND_REGISTRY NEVER)
        set(Python3_FIND_STRATEGY LOCATION)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            add_test(
                NAME hyremote-tool-product-fit
                COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/product-e2e/showcase_product_fit.py"
                    --showcase $<TARGET_FILE:hyremote-remote-support-showcase>
            )
        else()
            add_test(NAME hyremote-tool-product-fit COMMAND "${CMAKE_COMMAND}" -E false)
        endif()
        set_tests_properties(hyremote-tool-product-fit PROPERTIES
            TIMEOUT 900
            LABELS "candidate-evidence"
        )
        if(COMMAND hyremote_label_test_family)
            hyremote_label_test_family("e2e;tool;qualification" hyremote-tool-product-fit)
        endif()
    endif()
endif()
