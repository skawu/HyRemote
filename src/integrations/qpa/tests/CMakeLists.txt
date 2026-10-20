add_executable(hyremote-qpa-proxy-smoke
    qpa_proxy_smoke.cpp
)

set_target_properties(hyremote-qpa-proxy-smoke PROPERTIES AUTOMOC ON)
target_compile_features(hyremote-qpa-proxy-smoke PRIVATE cxx_std_17)
target_link_libraries(hyremote-qpa-proxy-smoke PRIVATE Qt6::Core Qt6::Gui)
add_dependencies(hyremote-qpa-proxy-smoke hyremote-qpa-platform)


# Native-semantics qualification remains mandatory for the exact Qt private-ABI QPA payload.
add_executable(hyremote-qpa-native-semantics
    qpa_native_semantics.cpp
)
set_target_properties(hyremote-qpa-native-semantics PROPERTIES AUTOMOC ON)
target_compile_features(hyremote-qpa-native-semantics PRIVATE cxx_std_17)
target_link_libraries(hyremote-qpa-native-semantics PRIVATE Qt6::Core Qt6::Gui Qt6::GuiPrivate)
add_dependencies(hyremote-qpa-native-semantics hyremote-qpa-platform)


# QPA owns only launch/configuration vocabulary. Application-surface composition is tested by
# src/runtime/tests and must not be rebuilt through QPA-owned predecessor sources.
add_executable(hyremote-qpa-remote-config-test
    qpa_remote_config_test.cpp
    ../qpa_config.cpp
)
target_compile_features(hyremote-qpa-remote-config-test PRIVATE cxx_std_17)
target_include_directories(hyremote-qpa-remote-config-test PRIVATE ..)
target_link_libraries(hyremote-qpa-remote-config-test PRIVATE Qt6::Core Qt6::Network)

if(TARGET Qt6::Widgets)
    # E4 is deliberately an ordinary Qt application. Compile its real source in the QPA build graph
    # with Qt Widgets only; do not add HyRemote targets here. The platform plugin remains a launch /
    # deployment concern rather than an application-source dependency.
    add_executable(hyremote-qpa-existing-app-build-check
        ${_hyremote_root_dir}/examples/qpa-proxy-existing-app/main.cpp
    )
    target_compile_features(hyremote-qpa-existing-app-build-check PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-existing-app-build-check PRIVATE Qt6::Widgets)

    add_executable(hyremote-qpa-auto-remoteaccess-smoke
        qpa_auto_remoteaccess_smoke.cpp
    )
    target_compile_features(hyremote-qpa-auto-remoteaccess-smoke PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-auto-remoteaccess-smoke
        PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets
    )
    add_dependencies(hyremote-qpa-auto-remoteaccess-smoke hyremote-qpa-platform)


    add_executable(hyremote-qpa-remote-failure-native-survival-smoke
        qpa_remote_failure_native_survival_smoke.cpp
    )
    target_compile_features(hyremote-qpa-remote-failure-native-survival-smoke PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-remote-failure-native-survival-smoke
        PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets
    )
    if(WIN32)
        target_link_libraries(hyremote-qpa-remote-failure-native-survival-smoke PRIVATE ws2_32)
    endif()
    add_dependencies(hyremote-qpa-remote-failure-native-survival-smoke hyremote-qpa-platform)


    add_executable(hyremote-qpa-multi-surface-connection-smoke
        qpa_multi_surface_connection_smoke.cpp
    )
    target_compile_features(hyremote-qpa-multi-surface-connection-smoke PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-multi-surface-connection-smoke
        PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets
    )
    add_dependencies(hyremote-qpa-multi-surface-connection-smoke hyremote-qpa-platform)


    add_executable(hyremote-qpa-widget-popup-connection-smoke
        qpa_widget_popup_connection_smoke.cpp
    )
    target_compile_features(hyremote-qpa-widget-popup-connection-smoke PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-widget-popup-connection-smoke
        PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets
    )
    add_dependencies(hyremote-qpa-widget-popup-connection-smoke hyremote-qpa-platform)


    # Production capture classification for QWidget + QOpenGLWidget through the real sidecar path.
    find_package(Qt6 6.8.3 EXACT QUIET COMPONENTS OpenGLWidgets)
    if(TARGET Qt6::OpenGLWidgets)
        add_executable(hyremote-qpa-widget-opengl-capture-smoke
            qpa_widget_opengl_capture_smoke.cpp
        )
        target_compile_features(hyremote-qpa-widget-opengl-capture-smoke PRIVATE cxx_std_17)
        target_link_libraries(hyremote-qpa-widget-opengl-capture-smoke
            PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets Qt6::OpenGLWidgets
        )
        add_dependencies(hyremote-qpa-widget-opengl-capture-smoke hyremote-qpa-platform)

    endif()
endif()

if(TARGET Qt6::Quick)
    add_executable(hyremote-qpa-quick-multi-window-connection-smoke
        qpa_quick_multi_window_connection_smoke.cpp
    )
    target_compile_features(hyremote-qpa-quick-multi-window-connection-smoke PRIVATE cxx_std_17)
    target_link_libraries(hyremote-qpa-quick-multi-window-connection-smoke
        PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Quick
    )
    add_dependencies(hyremote-qpa-quick-multi-window-connection-smoke hyremote-qpa-platform)

endif()

# The individual smoke binaries remain separate fixtures; three grouped CTests
# run them with their original QPA/plugin environment and report every case by
# name. This removes process bookkeeping without merging the assertions.
set(_hyremote_qpa_runtime_common
    -DQT_PLUGIN_PATH=${CMAKE_BINARY_DIR}/plugins
)

set(_hyremote_qpa_runtime_basic ${_hyremote_qpa_runtime_common}
    -DGROUP=basic
    -DPROXY_EXE=$<TARGET_FILE:hyremote-qpa-proxy-smoke>
    -DNATIVE_EXE=$<TARGET_FILE:hyremote-qpa-native-semantics>
    -DREMOTE_CONFIG_EXE=$<TARGET_FILE:hyremote-qpa-remote-config-test>
)
if(TARGET hyremote-qpa-widget-opengl-capture-smoke)
    list(APPEND _hyremote_qpa_runtime_basic
        -DOPENGL_EXE=$<TARGET_FILE:hyremote-qpa-widget-opengl-capture-smoke>)
endif()
add_test(NAME hyremote-qpa-runtime-basic
    COMMAND ${CMAKE_COMMAND} ${_hyremote_qpa_runtime_basic}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_qpa_runtime.cmake)
set_tests_properties(hyremote-qpa-runtime-basic PROPERTIES
    TIMEOUT 90
    RESOURCE_LOCK hyremote-qpa-runtime)

set(_hyremote_qpa_runtime_surfaces ${_hyremote_qpa_runtime_common} -DGROUP=surfaces)
set(_hyremote_qpa_runtime_surfaces_has_case FALSE)
if(TARGET hyremote-qpa-multi-surface-connection-smoke)
    list(APPEND _hyremote_qpa_runtime_surfaces
        -DMULTI_EXE=$<TARGET_FILE:hyremote-qpa-multi-surface-connection-smoke>)
    set(_hyremote_qpa_runtime_surfaces_has_case TRUE)
endif()
if(TARGET hyremote-qpa-widget-popup-connection-smoke)
    list(APPEND _hyremote_qpa_runtime_surfaces
        -DPOPUP_EXE=$<TARGET_FILE:hyremote-qpa-widget-popup-connection-smoke>)
    set(_hyremote_qpa_runtime_surfaces_has_case TRUE)
endif()
if(TARGET hyremote-qpa-quick-multi-window-connection-smoke)
    list(APPEND _hyremote_qpa_runtime_surfaces
        -DQUICK_MULTI_EXE=$<TARGET_FILE:hyremote-qpa-quick-multi-window-connection-smoke>)
    set(_hyremote_qpa_runtime_surfaces_has_case TRUE)
endif()
if(_hyremote_qpa_runtime_surfaces_has_case)
    add_test(NAME hyremote-qpa-runtime-surfaces
        COMMAND ${CMAKE_COMMAND} ${_hyremote_qpa_runtime_surfaces}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_qpa_runtime.cmake)
    set_tests_properties(hyremote-qpa-runtime-surfaces PROPERTIES
        TIMEOUT 120
        RESOURCE_LOCK hyremote-qpa-runtime)
endif()

set(_hyremote_qpa_runtime_recovery ${_hyremote_qpa_runtime_common} -DGROUP=recovery)
set(_hyremote_qpa_runtime_recovery_has_case FALSE)
if(TARGET hyremote-qpa-auto-remoteaccess-smoke)
    list(APPEND _hyremote_qpa_runtime_recovery
        -DAUTO_EXE=$<TARGET_FILE:hyremote-qpa-auto-remoteaccess-smoke>)
    set(_hyremote_qpa_runtime_recovery_has_case TRUE)
endif()
if(TARGET hyremote-qpa-remote-failure-native-survival-smoke)
    list(APPEND _hyremote_qpa_runtime_recovery
        -DFAILURE_EXE=$<TARGET_FILE:hyremote-qpa-remote-failure-native-survival-smoke>)
    set(_hyremote_qpa_runtime_recovery_has_case TRUE)
endif()
if(_hyremote_qpa_runtime_recovery_has_case)
    add_test(NAME hyremote-qpa-runtime-recovery
        COMMAND ${CMAKE_COMMAND} ${_hyremote_qpa_runtime_recovery}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_qpa_runtime.cmake)
    set_tests_properties(hyremote-qpa-runtime-recovery PROPERTIES
        TIMEOUT 90
        RESOURCE_LOCK hyremote-qpa-runtime)
endif()

include("${PROJECT_SOURCE_DIR}/tests/semantic_ctest_labels.cmake")
hyremote_label_test_family("integration;qpa;fast"
    hyremote-qpa-runtime-basic
    hyremote-qpa-runtime-surfaces
    hyremote-qpa-runtime-recovery
)
# The basic group includes the remote-config contract case; labels are additive.
hyremote_label_test_family("contract;qpa;fast"
    hyremote-qpa-runtime-basic
)

# The parent QPA directory registers the T4 deployment matrix after this subdirectory returns.
cmake_language(DEFER DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.." CALL hyremote_apply_qpa_deploy_semantic_test_labels)
