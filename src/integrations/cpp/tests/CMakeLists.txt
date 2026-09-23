get_filename_component(_hyremote_runtime_src
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../runtime/src" ABSOLUTE)

add_executable(hyremote-remoteaccess-test
    test_remote_access.cpp
)

target_link_libraries(hyremote-remoteaccess-test
    PRIVATE
        hyremote-remoteaccess
        HyRemote::Core
)

target_include_directories(hyremote-remoteaccess-test
    PRIVATE
        ${_hyremote_runtime_src}
)

if(HYREMOTE_WITH_VNC AND HYREMOTE_TRANSPORT_SECURITY_AVAILABLE)
    target_compile_definitions(hyremote-remoteaccess-test
        PRIVATE HYREMOTE_HAS_TRANSPORT_SECURITY=1
    )
endif()

add_test(NAME hyremote-remoteaccess-test COMMAND hyremote-remoteaccess-test)

add_executable(hyremote-remoteaccess-error-ack-test
    test_remote_access_error_ack.cpp
)
target_link_libraries(hyremote-remoteaccess-error-ack-test
    PRIVATE
        hyremote-remoteaccess
        HyRemote::Core
        Qt6::Core
)
target_include_directories(hyremote-remoteaccess-error-ack-test
    PRIVATE
        ${_hyremote_runtime_src}
)
add_test(NAME hyremote-remoteaccess-error-ack-test COMMAND hyremote-remoteaccess-error-ack-test)
set_tests_properties(hyremote-remoteaccess-error-ack-test PROPERTIES TIMEOUT 15)

add_executable(hyremote-remoteaccess-target-loss-test
    test_remote_access_target_loss.cpp
)
target_link_libraries(hyremote-remoteaccess-target-loss-test
    PRIVATE
        hyremote-remoteaccess
        HyRemote::Core
        Qt6::Core
)
target_include_directories(hyremote-remoteaccess-target-loss-test
    PRIVATE
        ${_hyremote_runtime_src}
)
add_test(NAME hyremote-remoteaccess-target-loss-test COMMAND hyremote-remoteaccess-target-loss-test)
set_tests_properties(hyremote-remoteaccess-target-loss-test PROPERTIES TIMEOUT 15)

if(HYREMOTE_WITH_VNC)
    add_executable(hyremote-rfb-test-server
        rfb_test_server.cpp
    )
    target_link_libraries(hyremote-rfb-test-server
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Network
    )
    target_include_directories(hyremote-rfb-test-server
        PRIVATE
            ${_hyremote_runtime_src}
    )

    # The real-viewer product fit: a standard RFB client drives the production transport end to end - bounded
    # occupied-port failure, incomplete-handshake slot expiry and recovery, abrupt-disconnect held-input cleanup, a real
    # framebuffer pixel read back through vncdotool, pointer/buttons/wheel, keyboard/text, reconnect and a clean
    # listener stop. It is candidate acceptance evidence rather than a regression test, so the classifier keeps it out
    # of ordinary fast pull requests and requires it in the exact-candidate full gate.
    #
    # The registration is deliberately not conditional on the viewer tooling: #229 requires executable product-fit
    # evidence, so the candidate lane must not be able to pass because the test was quietly never registered. When the
    # interpreter is absent the test is registered as a failure; when it is present the script itself fails closed on a
    # missing Pillow/vncdotool import instead of skipping.
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME hyremote-v01-rfb-product-fit
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/rfb_product_fit.py"
                "$<TARGET_FILE:hyremote-rfb-test-server>"
        )
    else()
        add_test(NAME hyremote-v01-rfb-product-fit COMMAND "${CMAKE_COMMAND}" -E false)
    endif()
    set_tests_properties(hyremote-v01-rfb-product-fit PROPERTIES
        TIMEOUT 900
        LABELS "candidate-evidence")
endif()

# The listener/address matrix still mixes public facade validation with lower-level bind/address
# behavior. Phase-B4 owns that semantic split, so it remains under the C++ frontend until then.
if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS)
    add_executable(hyremote-listener-address-matrix-test
        test_listener_address_matrix.cpp
    )
    target_link_libraries(hyremote-listener-address-matrix-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Network
            Qt6::Widgets
    )
    target_include_directories(hyremote-listener-address-matrix-test
        PRIVATE
            ${_hyremote_runtime_src}
    )
    add_test(NAME hyremote-listener-address-matrix-test COMMAND hyremote-listener-address-matrix-test)
    set_tests_properties(hyremote-listener-address-matrix-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

unset(_hyremote_runtime_src)
