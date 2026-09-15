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
