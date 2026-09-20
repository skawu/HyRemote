cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "HYREMOTE_SOURCE_DIR is required")
endif()

set(build_script "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteBuild.cmake")
if(NOT EXISTS "${build_script}")
    message(FATAL_ERROR "build-authority: missing ${build_script}")
endif()

function(run_success name)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -P "${build_script}" -- ${ARGN}
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err)
    set(combined "${out}\n${err}")
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "build-authority/${name}: expected success, rc=${rc}\n${combined}")
    endif()
    set(HYB_TEST_OUTPUT "${combined}" PARENT_SCOPE)
endfunction()

function(run_failure name)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -P "${build_script}" -- ${ARGN}
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err)
    set(combined "${out}\n${err}")
    if(rc EQUAL 0)
        message(FATAL_ERROR "build-authority/${name}: expected failure\n${combined}")
    endif()
    set(HYB_TEST_OUTPUT "${combined}" PARENT_SCOPE)
endfunction()

function(require_text name haystack needle)
    string(FIND "${haystack}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "build-authority/${name}: missing '${needle}'\n${haystack}")
    endif()
endfunction()

# 1. Built-in defaults are neutral: Common Runtime only, with no frontend silently selected.
run_success(no-config --no-config --show-config)
require_text(no-config "${HYB_TEST_OUTPUT}" "integrations.cpp  : OFF")
require_text(no-config "${HYB_TEST_OUTPUT}" "integrations.qml  : OFF")
require_text(no-config "${HYB_TEST_OUTPUT}" "integrations.generic: OFF")
require_text(no-config "${HYB_TEST_OUTPUT}" "integrations.qpa  : OFF")
require_text(no-config "${HYB_TEST_OUTPUT}" "integrations      : runtime-only")
require_text(no-config "${HYB_TEST_OUTPUT}" "transport.security: OFF")
require_text(no-config "${HYB_TEST_OUTPUT}" "examples          : OFF")

# 2. Repository build.yml is the neutral developer profile: all four peers selected explicitly.
run_success(root-config --show-config)
require_text(root-config "${HYB_TEST_OUTPUT}" "integrations.cpp  : ON")
require_text(root-config "${HYB_TEST_OUTPUT}" "integrations.qml  : ON")
require_text(root-config "${HYB_TEST_OUTPUT}" "integrations.generic: ON")
require_text(root-config "${HYB_TEST_OUTPUT}" "integrations.qpa  : ON")
require_text(root-config "${HYB_TEST_OUTPUT}" "integrations      : cpp,qml,generic,qpa")

# 3. CLI wins over build.yml and one peer never implies C++.
run_success(cli-precedence --mode=qml --security --no-examples --show-config)
require_text(cli-precedence "${HYB_TEST_OUTPUT}" "integrations.cpp  : OFF")
require_text(cli-precedence "${HYB_TEST_OUTPUT}" "integrations.qml  : ON")
require_text(cli-precedence "${HYB_TEST_OUTPUT}" "integrations.generic: OFF")
require_text(cli-precedence "${HYB_TEST_OUTPUT}" "integrations.qpa  : OFF")
require_text(cli-precedence "${HYB_TEST_OUTPUT}" "transport.security: ON")
require_text(cli-precedence "${HYB_TEST_OUTPUT}" "examples          : OFF")

# 4. Every peer mode and all/runtime resolve independently.
foreach(mode IN ITEMS cpp qml generic qpa)
    run_success("mode-${mode}" "--mode=${mode}" --show-config)
    require_text("mode-${mode}" "${HYB_TEST_OUTPUT}" "integrations      : ${mode}")
endforeach()
run_success(mode-all --mode=all --show-config)
require_text(mode-all "${HYB_TEST_OUTPUT}" "integrations      : cpp,qml,generic,qpa")
run_success(mode-runtime --mode=runtime --show-config)
require_text(mode-runtime "${HYB_TEST_OUTPUT}" "integrations      : runtime-only")

# 5. Explicit integration list is exact; it does not prepend cpp.
run_success(explicit-list --integrations=generic,qpa --show-config)
require_text(explicit-list "${HYB_TEST_OUTPUT}" "integrations.cpp  : OFF")
require_text(explicit-list "${HYB_TEST_OUTPUT}" "integrations.generic: ON")
require_text(explicit-list "${HYB_TEST_OUTPUT}" "integrations.qpa  : ON")
require_text(explicit-list "${HYB_TEST_OUTPUT}" "integrations      : generic,qpa")

# 6. Dedicated transport.security and env schema fields are parsed from YAML.
set(scratch "${CMAKE_CURRENT_BINARY_DIR}/hyremote-build-authority")
file(MAKE_DIRECTORY "${scratch}")
set(config "${scratch}/profile.yml")
file(WRITE "${config}" [=[version: 1
build:
  examples: false
integrations:
  cpp: false
  qml: false
  generic: true
  qpa: false
transport:
  security: true
env:
  HYREMOTE_BUILD_AUTHORITY_TEST: enabled
]=])
run_success(profile-yaml "--config=${config}" --show-config)
require_text(profile-yaml "${HYB_TEST_OUTPUT}" "integrations      : generic")
require_text(profile-yaml "${HYB_TEST_OUTPUT}" "transport.security: ON")
require_text(profile-yaml "${HYB_TEST_OUTPUT}" "HYREMOTE_BUILD_AUTHORITY_TEST=enabled")

# 7. Compiler/toolchain paths are resolved deterministically before configure/build.
set(fake_toolchain "${scratch}/fake-toolchain.cmake")
set(fake_cxx "${scratch}/fake-cxx")
file(WRITE "${fake_toolchain}" "# resolver fixture\n")
file(WRITE "${fake_cxx}" "resolver fixture\n")
run_success(toolchain-resolution --no-config "--toolchain=${fake_toolchain}" "--cxx-compiler=${fake_cxx}" --show-config)
require_text(toolchain-resolution "${HYB_TEST_OUTPUT}" "toolchain         : ${fake_toolchain}")
require_text(toolchain-resolution "${HYB_TEST_OUTPUT}" "C++ compiler      : ${fake_cxx}")

# 8. Unknown keys/invalid booleans fail closed.
set(invalid_key "${scratch}/invalid-key.yml")
file(WRITE "${invalid_key}" [=[version: 1
integrations:
  cpp: true
  surprise: true
]=])
run_failure(invalid-key "--config=${invalid_key}" --show-config)
require_text(invalid-key "${HYB_TEST_OUTPUT}" "unsupported integration 'surprise'")

set(invalid_bool "${scratch}/invalid-bool.yml")
file(WRITE "${invalid_bool}" [=[version: 1
transport:
  security: maybe
]=])
run_failure(invalid-bool "--config=${invalid_bool}" --show-config)
require_text(invalid-bool "${HYB_TEST_OUTPUT}" "expected boolean")

# 9. --show-config is canonical; the historical --print-config spelling must not survive silently.
run_failure(retired-print-config --no-config --print-config)
require_text(retired-print-config "${HYB_TEST_OUTPUT}" "Unknown build option '--print-config'")

message(STATUS "HyRemote build authority self-tests: PASS")
