add_executable(hyremote-qml-module-test
    test_qml_module.cpp
)

target_link_libraries(hyremote-qml-module-test
    PRIVATE
        Qt6::Core
        Qt6::Qml
)

target_compile_definitions(hyremote-qml-module-test
    PRIVATE
        HYREMOTE_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)

add_dependencies(hyremote-qml-module-test hyremote-qml hyremote-qmlplugin)

# Keep the repository's CMake 3.21 contract: ENVIRONMENT_MODIFICATION was added after that
# baseline, so launch the test through a tiny CMake script that explicitly prepends the generated
# QML module runtime directory to the platform loader path.
add_test(
    NAME hyremote-qml-module-test
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:hyremote-qml-module-test>
        -DMODULE_RUNTIME_DIR=$<TARGET_FILE_DIR:hyremote-qml>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_qml_module_test.cmake
)

# #259: the declarative frontend must be driven by the shared runtime's typed notifications, on this
# object's thread, with the same one shared runtime underneath. Linking HyRemote::RemoteAccess is what
# lets this test install the runtime's private test factories - the same seam the C++ facade tests use -
# so an event can be injected instead of waited for.
add_executable(hyremote-qml-notifications-test
    test_qml_notifications.cpp
)

target_compile_features(hyremote-qml-notifications-test PRIVATE cxx_std_17)

# HyRemote::Core is a test-only dependency here, exactly as in the runtime and facade test targets: the
# private factory header is written against the Core contracts. The QML payload itself still links only
# Qt and HyRemote::RemoteAccess.
target_link_libraries(hyremote-qml-notifications-test
    PRIVATE
        Qt6::Core
        Qt6::Qml
        HyRemote::Core
        HyRemote::RemoteAccess
)

target_compile_definitions(hyremote-qml-notifications-test
    PRIVATE
        HYREMOTE_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
)

add_dependencies(hyremote-qml-notifications-test hyremote-qml hyremote-qmlplugin)

add_test(
    NAME hyremote-qml-notifications-test
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXE=$<TARGET_FILE:hyremote-qml-notifications-test>
        -DMODULE_RUNTIME_DIR=$<TARGET_FILE_DIR:hyremote-qml>
        -DADDITIONAL_RUNTIME_DIR=$<TARGET_FILE_DIR:HyRemote::RemoteAccess>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_qml_module_test.cmake
)
set_tests_properties(hyremote-qml-notifications-test PROPERTIES TIMEOUT 60)

# The public SDK owns one deployment helper. These configure-only fixtures use deterministic Qt
# command doubles to prove that ordinary applications select qt_generate_deploy_app_script() while
# declarative applications select qt_generate_deploy_qml_app_script() exclusively.
add_test(
    NAME hyremote-qml-deploy-helper-non-qml
    COMMAND ${CMAKE_COMMAND}
        -S ${CMAKE_CURRENT_SOURCE_DIR}/deploy_helper_fixture
        -B ${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-non-qml
        -DHYREMOTE_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DTEST_QML=OFF
)

add_test(
    NAME hyremote-qml-deploy-helper-qml
    COMMAND ${CMAKE_COMMAND}
        -S ${CMAKE_CURRENT_SOURCE_DIR}/deploy_helper_fixture
        -B ${CMAKE_CURRENT_BINARY_DIR}/deploy-helper-qml
        -DHYREMOTE_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DTEST_QML=ON
)

include("${PROJECT_SOURCE_DIR}/tests/semantic_ctest_labels.cmake")
hyremote_label_test_family("integration;qml;fast"
    hyremote-qml-module-test
    hyremote-qml-notifications-test
)
hyremote_label_test_family("contract;qml;installed"
    hyremote-qml-deploy-helper-non-qml
    hyremote-qml-deploy-helper-qml
)
