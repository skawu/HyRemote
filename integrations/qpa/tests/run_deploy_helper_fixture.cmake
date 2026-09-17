if(NOT FIXTURE_SOURCE_DIR OR NOT FIXTURE_BINARY_DIR OR NOT HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "FIXTURE_SOURCE_DIR, FIXTURE_BINARY_DIR and HYREMOTE_SOURCE_DIR are required")
endif()

if(NOT DEFINED TEST_QML)
    set(TEST_QML OFF)
endif()
if(NOT DEFINED TEST_DEPLOY_QPA)
    set(TEST_DEPLOY_QPA ON)
endif()
if(NOT DEFINED TEST_QML_AVAILABLE)
    set(TEST_QML_AVAILABLE ON)
endif()
if(NOT DEFINED TEST_QML_IMPORT_PATH_EXISTS)
    set(TEST_QML_IMPORT_PATH_EXISTS ON)
endif()
if(NOT DEFINED TEST_QML_MODULE_DIR_EXISTS)
    set(TEST_QML_MODULE_DIR_EXISTS ON)
endif()
if(NOT DEFINED TEST_STALE_QML_METADATA)
    set(TEST_STALE_QML_METADATA OFF)
endif()
if(NOT DEFINED TEST_QPA_AVAILABLE)
    set(TEST_QPA_AVAILABLE ON)
endif()
if(NOT DEFINED TEST_STALE_QPA_METADATA)
    set(TEST_STALE_QPA_METADATA OFF)
endif()
if(NOT DEFINED TEST_STALE_QPA_NEGATIVE_METADATA)
    set(TEST_STALE_QPA_NEGATIVE_METADATA OFF)
endif()
if(NOT DEFINED TEST_QT_VERSION)
    set(TEST_QT_VERSION "6.8.3")
endif()
if(NOT DEFINED TEST_INSTALLED_PAYLOAD)
    set(TEST_INSTALLED_PAYLOAD OFF)
endif()
if(NOT DEFINED EXPECT_CONFIGURE_FAILURE)
    set(EXPECT_CONFIGURE_FAILURE OFF)
endif()
if(NOT DEFINED EXPECT_FAILURE_FRAGMENT)
    set(EXPECT_FAILURE_FRAGMENT "")
endif()
# Generator-shape coverage knobs. FIXTURE_GENERATOR pins the sub-configure generator (empty keeps the
# platform default, which is what the hosted jobs exercise); FIXTURE_BUILD_TYPE pins the single-config
# build type; FIXTURE_EXPECTED_CONFIG pins which configuration's script carries the deep assertions.
if(NOT DEFINED FIXTURE_GENERATOR)
    set(FIXTURE_GENERATOR "")
endif()
if(NOT DEFINED FIXTURE_BUILD_TYPE)
    set(FIXTURE_BUILD_TYPE "")
endif()
if(NOT DEFINED FIXTURE_EXPECTED_CONFIG)
    set(FIXTURE_EXPECTED_CONFIG "")
endif()

set(_fixture_configure_args
    -S "${FIXTURE_SOURCE_DIR}"
    -B "${FIXTURE_BINARY_DIR}"
    "-DHYREMOTE_SOURCE_DIR=${HYREMOTE_SOURCE_DIR}"
    "-DTEST_QML=${TEST_QML}"
    "-DTEST_DEPLOY_QPA=${TEST_DEPLOY_QPA}"
    "-DTEST_QML_AVAILABLE=${TEST_QML_AVAILABLE}"
    "-DTEST_QML_IMPORT_PATH_EXISTS=${TEST_QML_IMPORT_PATH_EXISTS}"
    "-DTEST_QML_MODULE_DIR_EXISTS=${TEST_QML_MODULE_DIR_EXISTS}"
    "-DTEST_STALE_QML_METADATA=${TEST_STALE_QML_METADATA}"
    "-DTEST_QPA_AVAILABLE=${TEST_QPA_AVAILABLE}"
    "-DTEST_STALE_QPA_METADATA=${TEST_STALE_QPA_METADATA}"
    "-DTEST_STALE_QPA_NEGATIVE_METADATA=${TEST_STALE_QPA_NEGATIVE_METADATA}"
    "-DTEST_QT_VERSION=${TEST_QT_VERSION}"
    "-DTEST_INSTALLED_PAYLOAD=${TEST_INSTALLED_PAYLOAD}"
)
if(FIXTURE_GENERATOR)
    list(APPEND _fixture_configure_args -G "${FIXTURE_GENERATOR}")
endif()
if(FIXTURE_BUILD_TYPE)
    list(APPEND _fixture_configure_args "-DCMAKE_BUILD_TYPE=${FIXTURE_BUILD_TYPE}")
endif()

file(REMOVE_RECURSE "${FIXTURE_BINARY_DIR}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" ${_fixture_configure_args}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_stdout
    ERROR_VARIABLE configure_stderr
)

if(EXPECT_CONFIGURE_FAILURE)
    if(configure_result EQUAL 0)
        message(FATAL_ERROR
            "deploy fixture unexpectedly configured successfully\n${configure_stdout}\n${configure_stderr}")
    endif()
    if("${EXPECT_FAILURE_FRAGMENT}" STREQUAL "")
        message(FATAL_ERROR
            "negative deploy fixture must declare EXPECT_FAILURE_FRAGMENT so unrelated CMake failures cannot pass")
    endif()

    # CMake formats fatal diagnostics to terminal width, so a semantically stable phrase may be split
    # across arbitrary newlines/indentation. Test registrations use underscores as explicit spaces;
    # normalize both the expected phrase and configure output before matching so line wrapping cannot
    # create a false negative while an unrelated configure error still cannot satisfy the contract.
    set(_expected_fragment "${EXPECT_FAILURE_FRAGMENT}")
    string(REPLACE "_" " " _expected_fragment "${_expected_fragment}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _expected_fragment "${_expected_fragment}")
    set(_configure_output "${configure_stdout}\n${configure_stderr}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _configure_output "${_configure_output}")

    string(FIND "${_configure_output}" "${_expected_fragment}" _failure_fragment_pos)
    if(_failure_fragment_pos EQUAL -1)
        message(FATAL_ERROR
            "deploy fixture failed for the wrong reason; expected '${_expected_fragment}'\n${_configure_output}")
    endif()
    return()
endif()

if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "deploy fixture configuration failed\n${configure_stdout}\n${configure_stderr}")
endif()

if(TEST_DEPLOY_QPA)
    set(_script_kind "QPA")
    set(_script_prefix "${FIXTURE_BINARY_DIR}/hyremote-qpa-deploy-deploy-probe")
else()
    set(_script_kind "runtime")
    set(_script_prefix "${FIXTURE_BINARY_DIR}/hyremote-runtime-deploy-deploy-probe")
endif()
# The fixture's own CMake_CURRENT_BINARY_DIR is always absolute, so resolve the expectation the same way
# before comparing it against glob results.
get_filename_component(_script_prefix "${_script_prefix}" ABSOLUTE)
file(GLOB generated_scripts "${_script_prefix}-*.cmake")

# The production helper names its script `.../$<CONFIG>.cmake` and file(GENERATE) expands that once per
# configuration, so the contract below is the generated configuration **set**, not a fixed count. A
# multi-config generator legitimately produces one script per configuration (each referencing that
# configuration's own payload locations); a single-config generator produces exactly one.
set(_fixture_cache "${FIXTURE_BINARY_DIR}/CMakeCache.txt")
if(NOT EXISTS "${_fixture_cache}")
    message(FATAL_ERROR "deploy fixture did not produce a CMake cache at ${_fixture_cache}")
endif()
file(STRINGS "${_fixture_cache}" _configuration_types_line REGEX "^CMAKE_CONFIGURATION_TYPES:")
file(STRINGS "${_fixture_cache}" _build_type_line REGEX "^CMAKE_BUILD_TYPE:")
string(REGEX REPLACE "^[^=]*=" "" declared_configurations "${_configuration_types_line}")
string(REGEX REPLACE "^[^=]*=" "" declared_build_type "${_build_type_line}")
# file(STRINGS) escapes the cache value's list separators, so split the declared configuration names out
# of the escaped value instead of treating it as a single element.
string(REGEX MATCHALL "[^;\\\\]+" declared_configurations "${declared_configurations}")
list(LENGTH declared_configurations declared_configuration_count)

list(LENGTH generated_scripts generated_count)
if(declared_configuration_count GREATER 0)
    if(NOT generated_count EQUAL declared_configuration_count)
        message(FATAL_ERROR
            "expected one generated ${_script_kind} deploy script per declared configuration "
            "(${declared_configuration_count}: ${declared_configurations}), found ${generated_count}: "
            "${generated_scripts}")
    endif()
    # This runner executes in `cmake -P` script mode, where policy CMP0057 stays OLD, so membership is
    # tested with list(FIND) rather than the IN_LIST operator.
    if(FIXTURE_EXPECTED_CONFIG)
        list(FIND declared_configurations "${FIXTURE_EXPECTED_CONFIG}" _expected_configuration_pos)
        if(_expected_configuration_pos EQUAL -1)
            message(FATAL_ERROR
                "FIXTURE_EXPECTED_CONFIG='${FIXTURE_EXPECTED_CONFIG}' is not a declared configuration: "
                "${declared_configurations}")
        endif()
        set(_selected_configuration "${FIXTURE_EXPECTED_CONFIG}")
    else()
        list(GET declared_configurations 0 _selected_configuration)
    endif()
    foreach(_declared_configuration IN LISTS declared_configurations)
        set(_expected_script "${_script_prefix}-${_declared_configuration}.cmake")
        list(FIND generated_scripts "${_expected_script}" _expected_script_pos)
        if(_expected_script_pos EQUAL -1)
            message(FATAL_ERROR
                "multi-config deploy generated no script for declared configuration "
                "'${_declared_configuration}': ${generated_scripts}")
        endif()
    endforeach()
    set(generated_script "${_script_prefix}-${_selected_configuration}.cmake")

    # Internal consistency of the generated set: every configuration's script must either reference its
    # own configuration location (the normal per-configuration layout) or none may, because a mixed set
    # would mean some configuration deploys another configuration's payload.
    set(_per_configuration_scripts 0)
    foreach(_declared_configuration IN LISTS declared_configurations)
        file(READ "${_script_prefix}-${_declared_configuration}.cmake" _candidate_content)
        string(FIND "${_candidate_content}" "/${_declared_configuration}/" _configuration_dir_pos)
        if(NOT _configuration_dir_pos EQUAL -1)
            math(EXPR _per_configuration_scripts "${_per_configuration_scripts} + 1")
        endif()
    endforeach()
    if(_per_configuration_scripts GREATER 0 AND
       NOT _per_configuration_scripts EQUAL declared_configuration_count)
        message(FATAL_ERROR
            "multi-config deploy set is internally inconsistent: only ${_per_configuration_scripts} of "
            "${declared_configuration_count} configuration scripts reference their own configuration "
            "location, so at least one configuration deploys another configuration's payload")
    endif()
else()
    if(NOT generated_count EQUAL 1)
        message(FATAL_ERROR
            "expected exactly one generated ${_script_kind} deploy script, found ${generated_count}: "
            "${generated_scripts}")
    endif()
    list(GET generated_scripts 0 generated_script)
    if(FIXTURE_EXPECTED_CONFIG)
        string(FIND "${generated_script}" "-${FIXTURE_EXPECTED_CONFIG}.cmake" _expected_suffix_pos)
        if(_expected_suffix_pos EQUAL -1)
            message(FATAL_ERROR
                "single-config deploy script does not name the expected configuration "
                "'${FIXTURE_EXPECTED_CONFIG}': ${generated_script}")
        endif()
    elseif(NOT declared_build_type STREQUAL "")
        # Single-config: $<CONFIG> is the configured build type, so the generated script must name it.
        string(FIND "${generated_script}" "-${declared_build_type}.cmake" _declared_suffix_pos)
        if(_declared_suffix_pos EQUAL -1)
            message(FATAL_ERROR
                "single-config deploy script does not name the configured build type "
                "'${declared_build_type}': ${generated_script}")
        endif()
    endif()
endif()
file(READ "${generated_script}" generated_content)

foreach(required_fragment IN ITEMS
        "qt_deploy_runtime_dependencies"
        "ADDITIONAL_LIBRARIES")
    string(FIND "${generated_content}" "${required_fragment}" fragment_pos)
    if(fragment_pos EQUAL -1)
        message(FATAL_ERROR
            "generated ${_script_kind} deploy script is missing '${required_fragment}':\n${generated_content}")
    endif()
endforeach()

# Multi-config: assert the universal fragments for every generated configuration, so a regression that
# only affects one configuration of a multi-config generator cannot pass silently.
if(declared_configuration_count GREATER 0)
    foreach(_declared_configuration IN LISTS declared_configurations)
        set(_candidate_script "${_script_prefix}-${_declared_configuration}.cmake")
        file(READ "${_candidate_script}" _candidate_content)
        foreach(required_fragment IN ITEMS
                "qt_deploy_runtime_dependencies"
                "ADDITIONAL_LIBRARIES")
            string(FIND "${_candidate_content}" "${required_fragment}" _candidate_fragment_pos)
            if(_candidate_fragment_pos EQUAL -1)
                message(FATAL_ERROR
                    "generated ${_script_kind} deploy script for configuration "
                    "'${_declared_configuration}' is missing '${required_fragment}':\n${_candidate_content}")
            endif()
        endforeach()
    endforeach()
endif()

if(TEST_INSTALLED_PAYLOAD)
    set(_expected_runtime "HyRemoteRemoteAccess")
    set(_forbidden_runtime "fake-remoteaccess")
else()
    set(_expected_runtime "fake-remoteaccess")
    set(_forbidden_runtime "HyRemoteRemoteAccess")
endif()
string(FIND "${generated_content}" "${_expected_runtime}" runtime_pos)
if(runtime_pos EQUAL -1)
    message(FATAL_ERROR
        "generated ${_script_kind} deploy script is missing expected runtime '${_expected_runtime}':\n${generated_content}")
endif()
string(FIND "${generated_content}" "${_forbidden_runtime}" wrong_runtime_pos)
if(NOT wrong_runtime_pos EQUAL -1)
    message(FATAL_ERROR
        "generated ${_script_kind} deploy script mixed source/installed runtime identity '${_forbidden_runtime}':\n${generated_content}")
endif()

if(TEST_DEPLOY_QPA)
    foreach(required_fragment IN ITEMS
            "ADDITIONAL_MODULES"
            "platforms")
        string(FIND "${generated_content}" "${required_fragment}" fragment_pos)
        if(fragment_pos EQUAL -1)
            message(FATAL_ERROR
                "generated QPA deploy script is missing '${required_fragment}':\n${generated_content}")
        endif()
    endforeach()

    if(TEST_INSTALLED_PAYLOAD)
        set(_expected_qpa_name
            "${CMAKE_SHARED_MODULE_PREFIX}qhyremote${CMAKE_SHARED_MODULE_SUFFIX}")
        string(FIND "${generated_content}" "${_expected_qpa_name}" installed_payload_pos)
        if(installed_payload_pos EQUAL -1)
            message(FATAL_ERROR
                "installed-payload QPA deployment did not use exact platform MODULE name '${_expected_qpa_name}':\n${generated_content}")
        endif()
    endif()
else()
    foreach(forbidden_fragment IN ITEMS
            "ADDITIONAL_MODULES"
            "qhyremote"
            "RPATH_CHANGE")
        string(FIND "${generated_content}" "${forbidden_fragment}" forbidden_pos)
        if(NOT forbidden_pos EQUAL -1)
            message(FATAL_ERROR
                "QML-only runtime deploy script unexpectedly contains '${forbidden_fragment}':\n${generated_content}")
        endif()
    endforeach()
endif()

string(FIND "${generated_content}" "fake-core" fake_core_pos)
if(NOT fake_core_pos EQUAL -1)
    message(FATAL_ERROR
        "deployment unexpectedly exposes a separate Core runtime:\n${generated_content}")
endif()

string(FIND "${generated_content}" "QT_PLUGIN_PATH" plugin_path_pos)
if(NOT plugin_path_pos EQUAL -1)
    message(FATAL_ERROR
        "normal deployment must not require QT_PLUGIN_PATH: ${generated_content}")
endif()

if(TEST_DEPLOY_QPA AND UNIX AND NOT APPLE)
    set(_literal_deploy_lib_dir "$ORIGIN/../../\${QT_DEPLOY_LIB_DIR}")
    foreach(rpath_fragment IN ITEMS
            "RPATH_CHANGE"
            "$ORIGIN/../../../."
            "${_literal_deploy_lib_dir}")
        string(FIND "${generated_content}" "${rpath_fragment}" rpath_pos)
        if(rpath_pos EQUAL -1)
            message(FATAL_ERROR
                "shared-facade QPA deployment script is missing RPATH relocation '${rpath_fragment}':\n${generated_content}")
        endif()
    endforeach()
endif()
