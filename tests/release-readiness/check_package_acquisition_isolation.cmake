cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

# A Windows caller may hand us a backslash path (`F:\...`). The fixtures are *generated CMake code* and this gate writes
# absolute paths into them, where a backslash inside a string literal is a syntax error ("Invalid character escape
# '\\w'"). Normalise once, at the entrance, so both slash forms behave identically.
file(TO_CMAKE_PATH "${HYREMOTE_SOURCE_DIR}" HYREMOTE_SOURCE_DIR)

include(CMakePackageConfigHelpers)

# Where the fixture world goes. This gate runs in script mode (`cmake -P`), where CMAKE_CURRENT_BINARY_DIR is **empty**,
# so the original `${CMAKE_CURRENT_BINARY_DIR}/...` collapsed to a bare relative name and resolved against whatever
# working directory the caller happened to have: running the gate by hand from the repository root wrote a 51-file
# fixture tree into the source tree. CTest passes the real binary directory; a manual run falls back to the project's one
# documented build directory, and either way the fixtures stay out of the source root.
if(DEFINED HYREMOTE_GATE_SCRATCH_DIR)
    set(_scratch_root "${HYREMOTE_GATE_SCRATCH_DIR}")
else()
    set(_scratch_root "${HYREMOTE_SOURCE_DIR}/build")
endif()
file(TO_CMAKE_PATH "${_scratch_root}" _scratch_root)
set(test_root "${_scratch_root}/hyremote-package-acquisition-isolation")

# A fixture tree in the source root can only come from the bug above (or from a checkout made before it was fixed), and it
# is what made a stray directory appear next to the sources. Fail with the remedy instead of silently ignoring it.
if(EXISTS "${HYREMOTE_SOURCE_DIR}/hyremote-package-acquisition-isolation")
    message(FATAL_ERROR
        "package-acquisition-isolation: a fixture tree is sitting in the source root "
        "(${HYREMOTE_SOURCE_DIR}/hyremote-package-acquisition-isolation). Nothing creates it there any more; delete it, "
        "and pass -DHYREMOTE_GATE_SCRATCH_DIR=<build directory> if you want the fixtures somewhere explicit.")
endif()

file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_root}")

set(qt_prefix "${test_root}/qt-prefix")
set(first_prefix "${test_root}/hyremote-first")
set(second_prefix "${test_root}/hyremote-second")

file(MAKE_DIRECTORY "${qt_prefix}/lib/cmake/Qt6")
file(WRITE "${qt_prefix}/lib/cmake/Qt6/Qt6Config.cmake"
"set(Qt6_FOUND TRUE)\n"
"set(Qt6_VERSION 6.8.3)\n"
"set(Qt6Core_VERSION 6.8.3)\n"
"set(PACKAGE_PREFIX_DIR \"${qt_prefix}\")\n"
"if(NOT TARGET Qt6::Core)\n  add_library(Qt6::Core INTERFACE IMPORTED)\nendif()\n"
"if(NOT TARGET Qt6::Network)\n  add_library(Qt6::Network INTERFACE IMPORTED)\nendif()\n")
file(WRITE "${qt_prefix}/lib/cmake/Qt6/Qt6ConfigVersion.cmake"
"set(PACKAGE_VERSION 6.8.3)\n"
"set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
"set(PACKAGE_VERSION_EXACT TRUE)\n")

function(make_hyremote_package prefix marker)
    set(package_dir "${prefix}/lib/cmake/HyRemote")
    file(MAKE_DIRECTORY "${package_dir}")
    file(MAKE_DIRECTORY "${prefix}/lib/qml/HyRemote")
    file(MAKE_DIRECTORY "${prefix}/lib/HyRemote/plugins/platforms")
    file(WRITE "${prefix}/lib/hyremote-qml-${marker}.fixture" "fixture")

    set(HYREMOTE_PACKAGE_WITH_REMOTE_ACCESS TRUE)
    set(HYREMOTE_PACKAGE_WITH_QML TRUE)
    set(HYREMOTE_PACKAGE_QML_IMPORT_SUBDIR "lib/qml")
    set(HYREMOTE_PACKAGE_QML_BACKING_SUBDIR "lib")
    set(HYREMOTE_PACKAGE_QML_BACKING_FILENAME "hyremote-qml-${marker}.fixture")
    set(HYREMOTE_PACKAGE_WITH_QPA TRUE)
    set(HYREMOTE_PACKAGE_QPA_QT_VERSION "6.8.3")
    set(HYREMOTE_PACKAGE_QPA_PLUGIN_SUBDIR "lib/HyRemote/plugins/platforms")
    set(HYREMOTE_PACKAGE_QPA_PLUGIN_FILENAME "qhyremote-${marker}.fixture")

    configure_package_config_file(
        "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteConfig.cmake.in"
        "${package_dir}/HyRemoteConfig.cmake"
        INSTALL_DESTINATION "lib/cmake/HyRemote"
        INSTALL_PREFIX "${prefix}"
    )

    file(WRITE "${package_dir}/HyRemoteTargets.cmake"
"if(NOT TARGET HyRemote::RemoteAccess)\n"
"  add_library(HyRemote::RemoteAccess SHARED IMPORTED)\n"
"  set_target_properties(HyRemote::RemoteAccess PROPERTIES IMPORTED_LOCATION \"${prefix}/lib/HyRemoteRemoteAccess.fixture\")\n"
"endif()\n")
    configure_file(
        "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteDeploy.cmake"
        "${package_dir}/HyRemoteDeploy.cmake"
        COPYONLY
    )
endfunction()

make_hyremote_package("${first_prefix}" "first")
make_hyremote_package("${second_prefix}" "second")

function(run_configure name source_text expect_success expected_fragment)
    set(source_dir "${test_root}/${name}-src")
    set(binary_dir "${test_root}/${name}-build")
    file(MAKE_DIRECTORY "${source_dir}")
    file(WRITE "${source_dir}/CMakeLists.txt" "${source_text}")

    # One case adds the project source tree, so the nested configure must use the same generator and toolchain as the
    # build under test. Without that it inherited whatever the host prefers - it chose MSVC NMake here - and the gate
    # failed inside compiler identification instead of reaching the acquisition conflict it exists to verify.
    set(configure_toolchain "")
    if(DEFINED HYREMOTE_GATE_GENERATOR AND NOT HYREMOTE_GATE_GENERATOR STREQUAL "")
        list(APPEND configure_toolchain -G "${HYREMOTE_GATE_GENERATOR}")
    endif()
    if(DEFINED HYREMOTE_GATE_MAKE_PROGRAM AND NOT HYREMOTE_GATE_MAKE_PROGRAM STREQUAL "")
        list(APPEND configure_toolchain "-DCMAKE_MAKE_PROGRAM=${HYREMOTE_GATE_MAKE_PROGRAM}")
    endif()
    if(DEFINED HYREMOTE_GATE_C_COMPILER AND NOT HYREMOTE_GATE_C_COMPILER STREQUAL "")
        list(APPEND configure_toolchain "-DCMAKE_C_COMPILER=${HYREMOTE_GATE_C_COMPILER}")
    endif()
    if(DEFINED HYREMOTE_GATE_CXX_COMPILER AND NOT HYREMOTE_GATE_CXX_COMPILER STREQUAL "")
        list(APPEND configure_toolchain "-DCMAKE_CXX_COMPILER=${HYREMOTE_GATE_CXX_COMPILER}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${source_dir}" -B "${binary_dir}" ${configure_toolchain}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )
    set(output "${stdout}\n${stderr}")

    if(expect_success)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "package acquisition fixture ${name} failed unexpectedly:\n${output}")
        endif()
    else()
        if(result EQUAL 0)
            message(FATAL_ERROR "package acquisition fixture ${name} unexpectedly succeeded")
        endif()
        string(FIND "${output}" "${expected_fragment}" fragment_pos)
        if(fragment_pos EQUAL -1)
            message(FATAL_ERROR
                "package acquisition fixture ${name} failed for the wrong reason; expected '${expected_fragment}':\n${output}")
        endif()
    endif()
endfunction()

string(CONCAT common_prefix_setup
"cmake_minimum_required(VERSION 3.21)\n"
"project(HyRemotePackageIsolation LANGUAGES NONE)\n"
"list(PREPEND CMAKE_PREFIX_PATH \"${qt_prefix}\")\n")

# Fake Qt deliberately overwrites PACKAGE_PREFIX_DIR. HyRemote must still publish paths from its own
# prefix, and a second discovery of the exact same installed package must remain harmless.
string(CONCAT success_source "${common_prefix_setup}"
"find_package(HyRemote CONFIG REQUIRED PATHS \"${first_prefix}/lib/cmake/HyRemote\" NO_DEFAULT_PATH)\n"
"if(NOT HyRemote_QML_IMPORT_PATH STREQUAL \"${first_prefix}/lib/qml\")\n"
"  message(FATAL_ERROR \"QML import path escaped HyRemote prefix: \${HyRemote_QML_IMPORT_PATH}\")\n"
"endif()\n"
"if(NOT HyRemote_QML_BACKING_FILE STREQUAL \"${first_prefix}/lib/hyremote-qml-first.fixture\")\n"
"  message(FATAL_ERROR \"QML backing path escaped HyRemote prefix: \${HyRemote_QML_BACKING_FILE}\")\n"
"endif()\n"
"if(NOT HyRemote_QPA_PLUGIN_FILE STREQUAL \"${first_prefix}/lib/HyRemote/plugins/platforms/qhyremote-first.fixture\")\n"
"  message(FATAL_ERROR \"QPA plugin path escaped HyRemote prefix: \${HyRemote_QPA_PLUGIN_FILE}\")\n"
"endif()\n"
"find_package(HyRemote CONFIG REQUIRED PATHS \"${first_prefix}/lib/cmake/HyRemote\" NO_DEFAULT_PATH)\n")
run_configure("same-prefix" "${success_source}" TRUE "")

# Loading a second installed HyRemote prefix in the same configure would pair the first imported
# runtime target with the second package's optional metadata. Clear both normal and cache forms of
# HyRemote_DIR so the second call really enters the second package rather than reusing discovery #1.
string(CONCAT two_prefix_source "${common_prefix_setup}"
"find_package(HyRemote CONFIG REQUIRED PATHS \"${first_prefix}/lib/cmake/HyRemote\" NO_DEFAULT_PATH)\n"
"unset(HyRemote_DIR)\n"
"unset(HyRemote_DIR CACHE)\n"
"find_package(HyRemote CONFIG REQUIRED PATHS \"${second_prefix}/lib/cmake/HyRemote\" NO_DEFAULT_PATH)\n")
run_configure("two-installed-prefixes" "${two_prefix_source}" FALSE "second installed prefix")

# A local/source target followed by installed package discovery is one dangerous mix: package
# metadata could otherwise be attached to an unrelated local runtime. Match a no-space identity
# fragment because CMake may line-wrap human-readable error text in configure diagnostics.
string(CONCAT source_then_package "${common_prefix_setup}"
"add_library(hyremote-local INTERFACE)\n"
"add_library(HyRemote::RemoteAccess ALIAS hyremote-local)\n"
"find_package(HyRemote CONFIG REQUIRED PATHS \"${first_prefix}/lib/cmake/HyRemote\" NO_DEFAULT_PATH)\n")
run_configure("source-then-installed" "${source_then_package}" FALSE "source/add_subdirectory")

# The reverse order must fail closed too. Load the fake installed package first, then add the actual
# HyRemote source tree with optional modes disabled. The runtime ownership guard rejects the attempt to
# create a second product runtime, which is the current wording of this contract; the expected fragment
# must follow the guard that actually produces it.
string(CONCAT package_then_source "${common_prefix_setup}"
"find_package(HyRemote CONFIG REQUIRED PATHS \"${first_prefix}/lib/cmake/HyRemote\" NO_DEFAULT_PATH)\n"
"set(HYREMOTE_BUILD_CORE ON CACHE BOOL \"\" FORCE)\n"
"set(HYREMOTE_BUILD_REMOTE_ACCESS ON CACHE BOOL \"\" FORCE)\n"
"set(HYREMOTE_BUILD_QML_API OFF CACHE BOOL \"\" FORCE)\n"
"set(HYREMOTE_WITH_QPA_PROXY OFF CACHE BOOL \"\" FORCE)\n"
"set(HYREMOTE_BUILD_TESTS OFF CACHE BOOL \"\" FORCE)\n"
"set(HYREMOTE_BUILD_EXAMPLES OFF CACHE BOOL \"\" FORCE)\n"
"add_subdirectory(\"${HYREMOTE_SOURCE_DIR}\" hyremote-source EXCLUDE_FROM_ALL)\n")
run_configure("installed-then-source" "${package_then_source}" FALSE "runtime target already exists")

message(STATUS
    "HyRemote package acquisition isolation: PASS "
    "(dependency-safe QML/QPA payload prefix + same-prefix rediscovery + second-prefix/source mixing rejected in both orders)")
