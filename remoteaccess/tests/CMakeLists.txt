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
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME hyremote-remoteaccess-test COMMAND hyremote-remoteaccess-test)

if(HYREMOTE_WITH_VNC)
    # Network interoperability harness used by the dual-OS maintained-client workflow. It links the
    # same private factory as RemoteAccess; it is not installed and is not part of the public SDK.
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
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    # Deterministic disconnect-balancing evidence (#90): drives the real transport over loopback with a
    # minimal client and asserts on the normalized InputEvents. Links the same private factory as the
    # interoperability harness; it is not installed and is not part of the public SDK.
    add_executable(hyremote-rfb-input-release-test
        test_rfb_input_release.cpp
    )
    target_link_libraries(hyremote-rfb-input-release-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Core
            Qt6::Network
    )
    target_include_directories(hyremote-rfb-input-release-test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    add_test(NAME hyremote-rfb-input-release-test COMMAND hyremote-rfb-input-release-test)
endif()

if(HYREMOTE_REMOTEACCESS_WITH_WIDGETS)
    add_executable(hyremote-widgets-capture-test
        test_widgets_capture.cpp
    )
    target_link_libraries(hyremote-widgets-capture-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Widgets
    )
    target_include_directories(hyremote-widgets-capture-test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    add_test(NAME hyremote-widgets-capture-test COMMAND hyremote-widgets-capture-test)
    set_tests_properties(hyremote-widgets-capture-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(hyremote-widgets-input-backpressure-test
        test_widgets_input_backpressure.cpp
    )
    target_link_libraries(hyremote-widgets-input-backpressure-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Widgets
    )
    target_include_directories(hyremote-widgets-input-backpressure-test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    add_test(NAME hyremote-widgets-input-backpressure-test COMMAND hyremote-widgets-input-backpressure-test)
    set_tests_properties(hyremote-widgets-input-backpressure-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

if(HYREMOTE_REMOTEACCESS_WITH_QUICK)
    add_executable(hyremote-quick-capture-test
        test_quick_capture.cpp
    )
    target_link_libraries(hyremote-quick-capture-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Quick
    )
    target_include_directories(hyremote-quick-capture-test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    add_test(NAME hyremote-quick-capture-test COMMAND hyremote-quick-capture-test)
    set_tests_properties(hyremote-quick-capture-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    )

    add_executable(hyremote-quick-input-backpressure-test
        test_quick_input_backpressure.cpp
    )
    target_link_libraries(hyremote-quick-input-backpressure-test
        PRIVATE
            hyremote-remoteaccess
            HyRemote::Core
            Qt6::Quick
    )
    target_include_directories(hyremote-quick-input-backpressure-test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    add_test(NAME hyremote-quick-input-backpressure-test COMMAND hyremote-quick-input-backpressure-test)
    set_tests_properties(hyremote-quick-input-backpressure-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    )
endif()
