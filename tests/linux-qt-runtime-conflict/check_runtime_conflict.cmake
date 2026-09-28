if(NOT HYREMOTE_SOURCE_DIR OR NOT HYREMOTE_FIXTURE_BINARY_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR and HYREMOTE_FIXTURE_BINARY_DIR are required")
endif()

# Linux Qt runtime conflict regression (#382).
#
# The release defect is a deployment-time decision: two roots offer the same SONAME, and CMake refuses to
# choose. This runner builds two real roots that offer it and then asks the shipped deployment code to do
# what it now promises, once per case. Nothing here depends on a distribution Qt being installed, on a
# particular Qt layout, or on the developer machine's own loader configuration: the roots are built by this
# regression, and the conflict is created by the fixture's RUNPATH entries.

set(_fixture_source "${HYREMOTE_SOURCE_DIR}/tests/linux-qt-runtime-conflict")
set(_assets_source "${_fixture_source}/assets")
set(_assets_binary "${HYREMOTE_FIXTURE_BINARY_DIR}/assets")
set(_asset_dir "${_assets_binary}/payloads")

set(_assets_configure
    "${CMAKE_COMMAND}"
    -S "${_assets_source}"
    -B "${_assets_binary}"
    "-DASSET_DIR=${_asset_dir}")
if(HYREMOTE_GATE_GENERATOR)
    list(APPEND _assets_configure -G "${HYREMOTE_GATE_GENERATOR}")
endif()
if(HYREMOTE_GATE_MAKE_PROGRAM)
    list(APPEND _assets_configure "-DCMAKE_MAKE_PROGRAM=${HYREMOTE_GATE_MAKE_PROGRAM}")
endif()
if(HYREMOTE_GATE_C_COMPILER)
    list(APPEND _assets_configure "-DCMAKE_C_COMPILER=${HYREMOTE_GATE_C_COMPILER}")
endif()
if(HYREMOTE_GATE_CXX_COMPILER)
    list(APPEND _assets_configure "-DCMAKE_CXX_COMPILER=${HYREMOTE_GATE_CXX_COMPILER}")
endif()
if(HYREMOTE_GATE_BUILD_TYPE)
    list(APPEND _assets_configure "-DCMAKE_BUILD_TYPE=${HYREMOTE_GATE_BUILD_TYPE}")
endif()

execute_process(COMMAND ${_assets_configure} RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR
        "linux-qt-runtime-conflict: configuring the synthetic ELF assets failed (${_rc}):\n${_out}")
endif()

set(_assets_build "${CMAKE_COMMAND}" --build "${_assets_binary}")
if(HYREMOTE_GATE_BUILD_TYPE)
    list(APPEND _assets_build --config "${HYREMOTE_GATE_BUILD_TYPE}")
endif()
execute_process(COMMAND ${_assets_build} RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR
        "linux-qt-runtime-conflict: building the synthetic ELF assets failed (${_rc}):\n${_out}")
endif()

foreach(_payload IN ITEMS
        "qt-a/lib/libQt6Core.so.6"
        "qt-a/lib/sub/libQt6Core.so.6"
        "qt-b/lib/libQt6Core.so.6"
        "qt-c/lib/libQt6Core.so.6"
        "qt-empty/lib/libplaceholder.so"
        "qt-b/lib/libhyremote-probe.so.1"
        "qt-c/lib/libhyremote-probe.so.1"
        "consumer-echo/libhyremote-qtconflict-consumer.so"
        "consumer-into-a/libhyremote-qtconflict-consumer.so"
        "consumer-into-b/libhyremote-qtconflict-consumer.so"
        "consumer-probe-into-a/libhyremote-qtconflict-consumer.so"
        "plugin-into-b/libqconflictplugin-b.so"
        "plugin-into-c/libqconflictplugin-c.so"
        "plugin-into-a-sub/libqconflictplugin-a-sub.so"
        "plugin-probe-into-a/libqconflictplugin-probe-a-c.so")
    if(NOT EXISTS "${_asset_dir}/${_payload}")
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: the asset build produced no ${_payload}, so the case that needs it "
            "would silently assert nothing")
    endif()
endforeach()

# The release shape is a versioned shared library with a SONAME symlink. A regular .so.6 fixture would let a
# deployment that drops the alias pass while the real application remains unloadable.
foreach(_qt_alias IN ITEMS
        "qt-a/lib/libQt6Core.so.6"
        "qt-a/lib/sub/libQt6Core.so.6"
        "qt-b/lib/libQt6Core.so.6"
        "qt-c/lib/libQt6Core.so.6")
    if(NOT IS_SYMLINK "${_asset_dir}/${_qt_alias}")
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: ${_qt_alias} is not a SONAME symlink, so the fixture cannot prove that "
            "deployment preserves a normal Qt shared-library chain")
    endif()
endforeach()

# The selected Qt root may itself be reached through a stable alias such as /opt/Qt/current/gcc_64. The conflict
# decision compares canonical paths, so the regression gives it exactly that shape rather than only literal roots.
set(_selected_root_alias "${_asset_dir}/qt-a-link")
file(REMOVE "${_selected_root_alias}")
file(CREATE_LINK "${_asset_dir}/qt-a/lib" "${_selected_root_alias}" SYMBOLIC RESULT _alias_result)
if(NOT _alias_result STREQUAL "0" OR NOT EXISTS "${_selected_root_alias}/libQt6Core.so.6")
    message(FATAL_ERROR
        "linux-qt-runtime-conflict: could not create the selected-root symlink fixture: ${_alias_result}")
endif()

# name | selected consumer Qt root | deployed-runtime depending file | plugin depending file | plugin file name |
# prepopulation mode (NONE, SELECTED, FOREIGN)
set(_cases
    "without-collection|qt-a/lib|consumer-into-b|plugin-into-c|libqconflictplugin-c.so|NONE"
    "selected-root-wins|qt-a/lib|consumer-into-a|plugin-into-b|libqconflictplugin-b.so|NONE"
    "selected-root-symlink-wins|qt-a-link|consumer-into-a|plugin-into-b|libqconflictplugin-b.so|NONE"
    "deployed-copy-wins|qt-a/lib|consumer-echo|plugin-into-b|libqconflictplugin-b.so|SELECTED"
    "foreign-deployed-copy-refused|qt-a/lib|consumer-echo|plugin-into-c|libqconflictplugin-c.so|FOREIGN"
    "no-selected-candidate|qt-empty/lib|consumer-into-b|plugin-into-c|libqconflictplugin-c.so|NONE"
    "ambiguous-selected-candidates|qt-a/lib|consumer-into-a|plugin-into-a-sub|libqconflictplugin-a-sub.so|NONE"
    "non-qt-conflict|qt-a/lib|consumer-probe-into-a|plugin-probe-into-a|libqconflictplugin-probe-a-c.so|NONE")

foreach(_case IN LISTS _cases)
    string(REPLACE "|" ";" _fields "${_case}")
    list(GET _fields 0 _name)
    list(GET _fields 1 _selected_root)
    list(GET _fields 2 _consumer)
    list(GET _fields 3 _plugin)
    list(GET _fields 4 _plugin_name)
    list(GET _fields 5 _prepopulate)
    set(_case_binary "${HYREMOTE_FIXTURE_BINARY_DIR}/${_name}")
    set(_consumer_file "${_asset_dir}/${_consumer}/libhyremote-qtconflict-consumer.so")
    set(_plugin_file "${_asset_dir}/${_plugin}/${_plugin_name}")

    # Configure the case so the shipped bootstrap writes this case's real deployment step, then run it and
    # assert on the files it produced.
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -S "${_fixture_source}/case"
            -B "${_case_binary}"
            "-DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}"
            "-DTEST_CASE=${_name}"
            "-DSELECTED_ROOT=${_asset_dir}/${_selected_root}"
            "-DCONSUMER=${_consumer_file}"
            "-DPLUGIN_NAME=${_plugin_name}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _out)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR
            "linux-qt-runtime-conflict: generating the deployment step for case '${_name}' failed:\n${_out}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DCASE_DIR=${_case_binary}"
            "-DASSET_DIR=${_asset_dir}"
            "-DTEST_CASE=${_name}"
            "-DSELECTED_ROOT=${_asset_dir}/${_selected_root}"
            "-DCONSUMER=${_consumer_file}"
            "-DPLUGIN=${_plugin_file}"
            "-DPLUGIN_NAME=${_plugin_name}"
            "-DPREPOPULATE=${_prepopulate}"
            -P "${_fixture_source}/case/verify.cmake"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _out)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "linux-qt-runtime-conflict: case '${_name}' failed:\n${_out}")
    endif()
    message(STATUS "linux-qt-runtime-conflict: ${_name} PASS")
endforeach()

message(STATUS
    "linux-qt-runtime-conflict: PASS (the selected consumer Qt runtime root decides the same-SONAME conflict, "
    "preserves the SONAME symlink through canonical root aliases, accepts only a matching staged copy, and no "
    "candidate from it / more than one candidate from it / a non-Qt conflict all fail closed)")
