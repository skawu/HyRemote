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

# 10. Dependency truth: the transport-security capability is VNC Authentication over OpenSSL Crypto and nothing more.
#     It must not advertise encryption/TLS, must not require the SSL component, and the runtime must not link a
#     library it never calls. #143 introduces the encrypted backend and its dependency when that backend exists; until
#     then this gate is what stops the false claim from coming back.
function(forbid_text name haystack needle)
    string(FIND "${haystack}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "build-authority/${name}: must not contain '${needle}'\n${haystack}")
    endif()
endfunction()

set(project_options "${HYREMOTE_SOURCE_DIR}/cmake/HyRemoteProjectOptions.cmake")
set(runtime_cmake "${HYREMOTE_SOURCE_DIR}/src/runtime/CMakeLists.txt")
set(vnc_auth_source "${HYREMOTE_SOURCE_DIR}/src/runtime/src/transport/vnc_auth.cpp")
set(cpp_tests_cmake "${HYREMOTE_SOURCE_DIR}/src/integrations/cpp/tests/CMakeLists.txt")
foreach(required_file "${project_options}" "${runtime_cmake}" "${vnc_auth_source}" "${cpp_tests_cmake}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "build-authority/security-truth: missing ${required_file}")
    endif()
endforeach()

file(READ "${project_options}" option_text)
require_text(security-truth "${option_text}" "find_package(OpenSSL QUIET COMPONENTS Crypto)")
require_text(security-truth "${option_text}" "VNC Authentication")
require_text(security-truth "${option_text}" "not encrypted")
foreach(overclaim IN ITEMS
        "COMPONENTS Crypto SSL" "Crypto;SSL"
        "authenticated and encrypted transport" "authenticated/encrypted transport"
        "encrypted transport available" "Crypto and SSL components")
    forbid_text(security-truth "${option_text}" "${overclaim}")
endforeach()

file(READ "${runtime_cmake}" runtime_text)
require_text(runtime-truth "${runtime_text}" "target_link_libraries(hyremote-remoteaccess PRIVATE OpenSSL::Crypto)")
forbid_text(runtime-truth "${runtime_text}" "OpenSSL::SSL")

# The implementation itself is the reason Crypto is enough: only Crypto-side primitives are used.
file(READ "${vnc_auth_source}" vnc_auth_text)
foreach(crypto_primitive IN ITEMS "openssl/rand.h" "openssl/des.h" "openssl/crypto.h" "RAND_bytes" "CRYPTO_memcmp")
    require_text(vnc_auth-crypto-only "${vnc_auth_text}" "${crypto_primitive}")
endforeach()
foreach(tls_symbol IN ITEMS "openssl/ssl.h" "SSL_CTX" "SSL_new" "TLS_method" "SSL_library_init")
    forbid_text(vnc_auth-crypto-only "${vnc_auth_text}" "${tls_symbol}")
endforeach()

# No transport-security build target may link SSL before an encrypted backend exists.
file(READ "${cpp_tests_cmake}" cpp_tests_text)
forbid_text(security-targets "${cpp_tests_text}" "OpenSSL::SSL")

function(run_env_success name env_args)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env ${env_args}
                "${CMAKE_COMMAND}" -P "${build_script}" -- ${ARGN}
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err)
    set(combined "${out}\n${err}")
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "build-authority/${name}: expected success, rc=${rc}\n${combined}")
    endif()
    set(HYB_TEST_OUTPUT "${combined}" PARENT_SCOPE)
endfunction()

function(run_env_failure name env_args)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env ${env_args}
                "${CMAKE_COMMAND}" -P "${build_script}" -- ${ARGN}
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err)
    set(combined "${out}\n${err}")
    if(rc EQUAL 0)
        message(FATAL_ERROR "build-authority/${name}: expected failure\n${combined}")
    endif()
    set(HYB_TEST_OUTPUT "${combined}" PARENT_SCOPE)
endfunction()

# CMake wraps message() text to the console width, so a phrase that is one sentence in the source can arrive with
# line breaks inside it. These two helpers collapse whitespace before matching, which lets a case assert a sentence
# without depending on where the wrapping happened.
function(require_text_flat name haystack needle)
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${haystack}")
    string(FIND "${flat}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "build-authority/${name}: missing '${needle}'\n${haystack}")
    endif()
endfunction()

function(forbid_text_flat name haystack needle)
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${haystack}")
    string(FIND "${flat}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "build-authority/${name}: must not contain '${needle}'\n${haystack}")
    endif()
endfunction()

# 11. Installed Qt kit discovery on POSIX (#387).
#
# A kit is discovered only from an empty qt.prefix, is validated by its own lib/cmake/Qt6/Qt6Config.cmake, and is
# adopted only when exactly one candidate is both real and usable from this shell. The scan roots are overridden
# here so the cases assert the rule rather than whichever kits the machine running the tests happens to have, and so
# that nothing in the repository carries this machine's Qt layout.
function(make_qt_discovery_kit kit_dir marker)
    file(MAKE_DIRECTORY "${kit_dir}/lib/cmake/Qt6")
    file(WRITE "${kit_dir}/lib/cmake/Qt6/Qt6Config.cmake" "# ${marker}\n")
endfunction()

set(qt_one_root "${scratch}/qt-discovery-one")
set(qt_two_root "${scratch}/qt-discovery-two")
set(qt_rejected_root "${scratch}/qt-discovery-rejected")
set(qt_empty_root "${scratch}/qt-discovery-empty")
set(qt_one_kit "${qt_one_root}/6.8.3/gcc_64")
make_qt_discovery_kit("${qt_one_kit}" kit-one)
make_qt_discovery_kit("${qt_two_root}/6.8.3/gcc_64" kit-two-a)
make_qt_discovery_kit("${qt_two_root}/6.8.4/gcc_64" kit-two-b)
make_qt_discovery_kit("${qt_rejected_root}/6.8.3/wasm_32" kit-wasm)
make_qt_discovery_kit("${qt_rejected_root}/6.8.4/gcc_arm64" kit-wrong-arch)
file(MAKE_DIRECTORY "${qt_empty_root}")

# An explicit --qt-prefix is used exactly as given and never second-guessed by discovery. That precedence is the
# same on every platform, so it is asserted on every platform.
run_env_success(qt-discovery-explicit-prefix-wins
    "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_two_root};QTDIR=;CMAKE_PREFIX_PATH="
    --no-config --show-config "--qt-prefix=${qt_two_root}/6.8.4/gcc_64")
require_text_flat(qt-discovery-explicit-prefix-wins "${HYB_TEST_OUTPUT}"
    "Qt prefix : ${qt_two_root}/6.8.4/gcc_64")
forbid_text_flat(qt-discovery-explicit-prefix-wins "${HYB_TEST_OUTPUT}" "(discovered)")

# The platform-specific half of the rule: which install roots POSIX discovery scans, how a POSIX kit is
# classified, and what a POSIX reader is told when nothing usable is installed. Windows keeps its own layout
# rule in the same function - the Online Installer tree and its msvc/mingw kit names - and that rule is
# exercised by the Windows lane's real build, so it is not asserted here through POSIX kit names and POSIX
# shell commands.
if(UNIX AND NOT APPLE)
    # One compatible kit is adopted, and the run says where the prefix came from.
    run_env_success(qt-discovery-one-kit
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_one_root};QTDIR=;CMAKE_PREFIX_PATH="
        --no-config --show-config)
    require_text_flat(qt-discovery-one-kit "${HYB_TEST_OUTPUT}" "Qt prefix : ${qt_one_kit} (discovered)")

    # #389: Qt ships byte-identical Qt6Config.cmake files in every kit of a release, so a kit's identity cannot be
    # that file on its own - hashing it alone identifies the Qt version, and every sibling kit of that version then
    # looks like a copy of the first one found. These two kits share the identical configuration on purpose, which
    # is the shape the defect was reported in: a usable kit next to an unusable sibling.
    set(qt_identical_root "${scratch}/qt-discovery-identical")
    make_qt_discovery_kit("${qt_identical_root}/6.8.3/gcc_64" shared-config)
    make_qt_discovery_kit("${qt_identical_root}/6.8.3/wasm_32" shared-config)

    # Exactly one usable kit is the resolution, not a problem: it is adopted, and a run that resolved to it must
    # not also claim that none of the kits on the machine could be used.
    run_env_success(qt-discovery-identical-config-unique-usable
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_identical_root};QTDIR=;CMAKE_PREFIX_PATH="
        --no-config --show-config)
    require_text_flat(qt-discovery-identical-config-unique-usable "${HYB_TEST_OUTPUT}"
        "Qt prefix : ${qt_identical_root}/6.8.3/gcc_64 (discovered)")
    forbid_text_flat(qt-discovery-identical-config-unique-usable "${HYB_TEST_OUTPUT}"
        "none of them can be used from this shell")
    forbid_text_flat(qt-discovery-identical-config-unique-usable "${HYB_TEST_OUTPUT}" "Found but not usable")

    # The unusable sibling is a diagnostic, so it belongs in the diagnostic output and nowhere else.
    run_env_success(qt-discovery-identical-config-verbose-sibling
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_identical_root};QTDIR=;CMAKE_PREFIX_PATH="
        --no-config --show-config -v)
    require_text_flat(qt-discovery-identical-config-verbose-sibling "${HYB_TEST_OUTPUT}"
        "Qt prefix : ${qt_identical_root}/6.8.3/gcc_64 (discovered)")
    require_text_flat(qt-discovery-identical-config-verbose-sibling "${HYB_TEST_OUTPUT}"
        "Qt kits found but not usable: ${qt_identical_root}/6.8.3/wasm_32 (its directory name states no compiler family)")

    # No usable kit at all is worth stopping for on a run that needs Qt: the reasons and the command to paste are
    # printed, and the run does not go on to configure against a kit that cannot build it.
    run_env_failure(qt-discovery-no-usable-kit-build
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_rejected_root};QTDIR=;CMAKE_PREFIX_PATH="
        build --examples "--build-dir=${scratch}/qt-discovery-no-usable-build")
    require_text_flat(qt-discovery-no-usable-kit-build "${HYB_TEST_OUTPUT}"
        "none of them can be used from this shell")
    require_text_flat(qt-discovery-no-usable-kit-build "${HYB_TEST_OUTPUT}"
        "Found but not usable:")
    require_text_flat(qt-discovery-no-usable-kit-build "${HYB_TEST_OUTPUT}"
        "--qt-prefix=<one-of-these>")

    # Two compatible kits are ambiguous: nothing is chosen, both are named, and the command to paste is given.
    run_env_success(qt-discovery-two-kits-ambiguous
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_two_root};QTDIR=;CMAKE_PREFIX_PATH="
        --no-config --show-config)
    require_text_flat(qt-discovery-two-kits-ambiguous "${HYB_TEST_OUTPUT}"
        "Several Qt 6.8 kits on this machine could build HyRemote")
    require_text_flat(qt-discovery-two-kits-ambiguous "${HYB_TEST_OUTPUT}" "${qt_two_root}/6.8.3/gcc_64")
    require_text_flat(qt-discovery-two-kits-ambiguous "${HYB_TEST_OUTPUT}" "${qt_two_root}/6.8.4/gcc_64")
    require_text_flat(qt-discovery-two-kits-ambiguous "${HYB_TEST_OUTPUT}"
        "sh ./build.cmd build --qt-prefix=<one-of-these>")
    forbid_text_flat(qt-discovery-two-kits-ambiguous "${HYB_TEST_OUTPUT}" "Qt prefix : ${qt_two_root}")
    # Report-only is not an execution: --show-config keeps the default `build` subcommand and returns before any
    # phase runs, so an ambiguity there is reported and nothing is configured.
    forbid_text_flat(qt-discovery-two-kits-ambiguous "${HYB_TEST_OUTPUT}" "phase: configure")

    # ... and the same ambiguity fails the build path closed instead of configuring against a guessed kit. The
    # configuration asks for a Qt-dependent product (--examples), which is the shape the defect was reported in:
    # a Core-only configuration legitimately needs no Qt at all, so asserting on that would prove nothing.
    run_env_failure(qt-discovery-two-kits-build-fails-closed
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_two_root};QTDIR=;CMAKE_PREFIX_PATH="
        build --examples "--build-dir=${scratch}/qt-discovery-two-build")
    require_text_flat(qt-discovery-two-kits-build-fails-closed "${HYB_TEST_OUTPUT}"
        "Several Qt 6.8 kits on this machine could build HyRemote")
    require_text_flat(qt-discovery-two-kits-build-fails-closed "${HYB_TEST_OUTPUT}"
        "sh ./build.cmd build --qt-prefix=<one-of-these>")
    # Fail closed means it stops before configuring anything, not after a configure that then fails for its own
    # reasons: the reader gets one clear reason, at the point the decision is made.
    forbid_text_flat(qt-discovery-two-kits-build-fails-closed "${HYB_TEST_OUTPUT}" "phase: configure")

    # The environment tier keeps its precedence over platform discovery.
    run_env_success(qt-discovery-environment-first
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_empty_root};QTDIR=${qt_one_kit};CMAKE_PREFIX_PATH="
        --no-config --show-config)
    require_text_flat(qt-discovery-environment-first "${HYB_TEST_OUTPUT}"
        "Qt prefix : ${qt_one_kit} (discovered)")

    # #389: the precedence is a frozen order of tiers - CLI/build.yml explicit, then the environment, then the
    # platform's layout - and the tiers must not be merged into one pool. An explicit choice in the environment wins
    # over anything the platform layout adds, and no platform kit may turn it into an ambiguity.
    run_env_success(qt-discovery-environment-beats-platform
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_two_root};QTDIR=${qt_one_kit};CMAKE_PREFIX_PATH="
        --no-config --show-config)
    require_text_flat(qt-discovery-environment-beats-platform "${HYB_TEST_OUTPUT}"
        "Qt prefix : ${qt_one_kit} (discovered)")
    forbid_text_flat(qt-discovery-environment-beats-platform "${HYB_TEST_OUTPUT}"
        "Several Qt 6.8 kits")

    # Two usable kits *inside the environment tier* are an ambiguity inside that tier: it fails closed there, names the
    # tier it resolved, and never falls through to the platform.
    run_env_success(qt-discovery-environment-tier-ambiguous
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_rejected_root};QTDIR=${qt_two_root}/6.8.3/gcc_64;CMAKE_PREFIX_PATH=${qt_two_root}/6.8.4/gcc_64"
        --no-config --show-config)
    require_text_flat(qt-discovery-environment-tier-ambiguous "${HYB_TEST_OUTPUT}"
        "Several Qt 6.8 kits on this machine could build HyRemote")
    require_text_flat(qt-discovery-environment-tier-ambiguous "${HYB_TEST_OUTPUT}"
        "Resolved tier: environment (QTDIR / CMAKE_PREFIX_PATH)")
    require_text_flat(qt-discovery-environment-tier-ambiguous "${HYB_TEST_OUTPUT}"
        "sh ./build.cmd build --qt-prefix=<one-of-these>")
    forbid_text_flat(qt-discovery-environment-tier-ambiguous "${HYB_TEST_OUTPUT}" "phase: configure")

    # An environment tier that names kits and can use none of them is reported as that, and the platform tier is not
    # silently substituted for the caller's own choice.
    run_env_success(qt-discovery-environment-tier-broken-not-masked
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_one_root};QTDIR=${qt_rejected_root}/6.8.4/gcc_arm64;CMAKE_PREFIX_PATH="
        --no-config --show-config)
    require_text_flat(qt-discovery-environment-tier-broken-not-masked "${HYB_TEST_OUTPUT}"
        "none of them can be used from this shell")
    require_text_flat(qt-discovery-environment-tier-broken-not-masked "${HYB_TEST_OUTPUT}"
        "Resolved tier: environment (QTDIR / CMAKE_PREFIX_PATH)")
    forbid_text_flat(qt-discovery-environment-tier-broken-not-masked "${HYB_TEST_OUTPUT}"
        "Qt prefix : ${qt_one_kit} (discovered)")

    # Identity inside a tier is the canonical physical prefix, not the Qt version, the kit class or the bytes of
    # Qt6Config.cmake: Qt ships byte-identical configuration across a release, so two different prefixes with the same
    # version, the same class and identical configuration are genuinely two candidates and fail closed.
    set(qt_same_class_root_a "${scratch}/qt-discovery-same-class-a")
    set(qt_same_class_root_b "${scratch}/qt-discovery-same-class-b")
    make_qt_discovery_kit("${qt_same_class_root_a}/6.8.3/gcc_64" same-class-config)
    make_qt_discovery_kit("${qt_same_class_root_b}/6.8.3/gcc_64" same-class-config)
    run_env_success(qt-discovery-same-class-distinct-prefixes
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_same_class_root_a};${qt_same_class_root_b};QTDIR=;CMAKE_PREFIX_PATH="
        --no-config --show-config)
    require_text_flat(qt-discovery-same-class-distinct-prefixes "${HYB_TEST_OUTPUT}"
        "Several Qt 6.8 kits on this machine could build HyRemote")
    require_text_flat(qt-discovery-same-class-distinct-prefixes "${HYB_TEST_OUTPUT}"
        "${qt_same_class_root_a}/6.8.3/gcc_64")
    require_text_flat(qt-discovery-same-class-distinct-prefixes "${HYB_TEST_OUTPUT}"
        "${qt_same_class_root_b}/6.8.3/gcc_64")
    forbid_text_flat(qt-discovery-same-class-distinct-prefixes "${HYB_TEST_OUTPUT}" "phase: configure")

    # The same physical kit reached twice is not a false ambiguity: a symbolic alias resolves to one canonical prefix,
    # so one kit stays one candidate and is selected.
    set(qt_alias_root "${scratch}/qt-discovery-alias")
    file(MAKE_DIRECTORY "${qt_alias_root}/6.8.3")
    file(CREATE_LINK "${qt_one_kit}" "${qt_alias_root}/6.8.3/gcc_64" SYMBOLIC)
    if(EXISTS "${qt_alias_root}/6.8.3/gcc_64/lib/cmake/Qt6/Qt6Config.cmake")
        run_env_success(qt-discovery-canonical-alias-not-ambiguous
            "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_alias_root};QTDIR=;CMAKE_PREFIX_PATH="
            --no-config --show-config)
        require_text_flat(qt-discovery-canonical-alias-not-ambiguous "${HYB_TEST_OUTPUT}"
            "Qt prefix : ${qt_alias_root}/6.8.3/gcc_64 (discovered)")
        forbid_text_flat(qt-discovery-canonical-alias-not-ambiguous "${HYB_TEST_OUTPUT}"
            "Several Qt 6.8 kits")
    endif()

    # --show-config reports without requiring a kit or a toolchain: it still exits 0 with no compiler on PATH,
    # and the kit it can see is reported as unusable with the reason instead of being adopted.
    run_env_success(qt-discovery-show-config-without-toolchain
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_one_root};QTDIR=;CMAKE_PREFIX_PATH=;PATH=/nonexistent"
        --no-config --show-config)
    require_text(qt-discovery-show-config-without-toolchain "${HYB_TEST_OUTPUT}" "integrations.cpp  : OFF")
    require_text_flat(qt-discovery-show-config-without-toolchain "${HYB_TEST_OUTPUT}"
        "none of them can be used from this shell")
    require_text_flat(qt-discovery-show-config-without-toolchain "${HYB_TEST_OUTPUT}"
        "needs g++, which this shell does not provide")

    # Nothing installed is reported for the shell the reader is in, never for another platform's layout.
    run_env_success(qt-discovery-none-found
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_empty_root};QTDIR=;CMAKE_PREFIX_PATH="
        --no-config --show-config)
    require_text_flat(qt-discovery-none-found "${HYB_TEST_OUTPUT}" "No Qt 6.8+ installation was found")
    require_text_flat(qt-discovery-none-found "${HYB_TEST_OUTPUT}"
        "sh ./build.cmd build --qt-prefix=<path-to-Qt/6.8.3/gcc_64>")
    require_text_flat(qt-discovery-none-found "${HYB_TEST_OUTPUT}" "/opt/Qt/6.8.*/*")
    forbid_text_flat(qt-discovery-none-found "${HYB_TEST_OUTPUT}" "C:/Qt")
    forbid_text_flat(qt-discovery-none-found "${HYB_TEST_OUTPUT}" "mingw_64")

    # A kit for another target or without a desktop toolchain is rejected with its reason, not guessed at.
    run_env_success(qt-discovery-rejects-non-desktop-kit
        "HYREMOTE_QT_DISCOVERY_ROOTS=${qt_rejected_root};QTDIR=;CMAKE_PREFIX_PATH="
        --no-config --show-config)
    require_text_flat(qt-discovery-rejects-non-desktop-kit "${HYB_TEST_OUTPUT}"
        "none of them can be used from this shell")
    require_text_flat(qt-discovery-rejects-non-desktop-kit "${HYB_TEST_OUTPUT}" "wasm_32")
    require_text_flat(qt-discovery-rejects-non-desktop-kit "${HYB_TEST_OUTPUT}" "gcc_arm64")
    require_text_flat(qt-discovery-rejects-non-desktop-kit "${HYB_TEST_OUTPUT}"
        "its directory name states no compiler family")
endif()

# 12. One cross-platform build authority, one Qt discovery pipeline, and a run that shows its work (#389).
#
# These are source-level assertions on purpose. What is being pinned is that there is a single implementation -
# one process runner and one discovery pipeline - rather than that one particular host behaves well; the
# platforms themselves prove the behaviour in their own acceptance runs.
file(READ "${build_script}" build_authority_text)

# One shared process runner, used by every phase that produces output.
require_text(orchestration-shared-runner "${build_authority_text}" "function(hyb_run_phase")
foreach(phase IN ITEMS configure build install test)
    require_text("orchestration-phase-${phase}" "${build_authority_text}" "hyb_run_phase(${phase} ")
endforeach()

# A normal run is never silent: the runner duplicates the phase's own output onto the console, and no phase
# redirects its output to a file where only a reader who knows the filename can find it.
require_text(orchestration-live-stdout "${build_authority_text}" "ECHO_OUTPUT_VARIABLE")
require_text(orchestration-live-stderr "${build_authority_text}" "ECHO_ERROR_VARIABLE")
forbid_text(orchestration-silent-stdout "${build_authority_text}" "OUTPUT_FILE")
forbid_text(orchestration-silent-stderr "${build_authority_text}" "ERROR_FILE")

# The runner itself: it names the phase, echoes that phase's output as it is produced, keeps the log the callers
# read, and gates exactly one thing on --verbose - the command echo. Anything else gated on --verbose is how a
# normal run goes silent again.
string(FIND "${build_authority_text}" "function(hyb_run_phase" _phase_runner_start)
if(_phase_runner_start EQUAL -1)
    message(FATAL_ERROR "build-authority/orchestration: hyb_run_phase is missing")
endif()
string(SUBSTRING "${build_authority_text}" ${_phase_runner_start} -1 hyb_phase_tail)
string(FIND "${hyb_phase_tail}" "endfunction()" _phase_runner_end)
if(_phase_runner_end EQUAL -1)
    message(FATAL_ERROR "build-authority/orchestration: hyb_run_phase is not terminated")
endif()
math(EXPR _phase_runner_length "${_phase_runner_end} + 13")
string(SUBSTRING "${hyb_phase_tail}" 0 ${_phase_runner_length} hyb_phase_body)
require_text(orchestration-phase-heading "${hyb_phase_body}" "message(STATUS \"phase: \${phase_name}\")")
require_text(orchestration-phase-live-stdout "${hyb_phase_body}" "ECHO_OUTPUT_VARIABLE")
require_text(orchestration-phase-live-stderr "${hyb_phase_body}" "ECHO_ERROR_VARIABLE")
require_text(orchestration-phase-log "${hyb_phase_body}" "file(WRITE \"\${phase_log}\"")
require_text(orchestration-verbose-command "${hyb_phase_body}" "  command  : \${ARGN}")

# The environment has to survive the trip to the process, and a Windows PATH contains the list separator: every
# route that puts such a value on an argument list eventually splits it, after which a path fragment is launched as
# if it were the command. So a phase's environment is applied to this process, which the phase then inherits, and no
# value ever becomes an argument.
require_text(orchestration-env-helper "${build_authority_text}" "function(hyb_apply_env")
require_text(orchestration-env-applied "${build_authority_text}"
    "set(ENV{\${_phase_env_name}} \"\${_phase_env_value}\")")
require_text(orchestration-env-applied-configure "${build_authority_text}" "hyb_apply_env(\"\${env_text}\")")
require_text(orchestration-env-applied-test "${build_authority_text}" "hyb_apply_env(\"\${test_env_text}\")")
forbid_text(orchestration-command-env-wrapper "${build_authority_text}" "COMMAND \"\${CMAKE_COMMAND}\" -E env")
string(REGEX MATCHALL "HYB_VERBOSE" _hyb_phase_verbose_uses "${hyb_phase_body}")
list(LENGTH _hyb_phase_verbose_uses _hyb_phase_verbose_count)
if(NOT _hyb_phase_verbose_count EQUAL 1)
    message(FATAL_ERROR
        "build-authority/orchestration: the phase runner gates ${_hyb_phase_verbose_count} thing(s) on --verbose; "
        "exactly the command echo may be gated, or a normal run stops being able to show that it is working")
endif()

# One Qt discovery pipeline: the platform-specific part is isolated to the two roots functions, and the policy
# that follows - validation, identity, classification, the unique-candidate rule, the ambiguity refusal -
# carries no platform branch. The tiers are resolved one at a time and never merged into a single pool.
require_text(qt-discovery-platform-roots "${build_authority_text}" "function(hyb_qt_platform_roots")
require_text(qt-discovery-environment-roots "${build_authority_text}" "function(hyb_qt_environment_roots")
require_text(qt-discovery-tiers-not-pooled "${build_authority_text}" "hyb_qt_environment_roots(_qt_environment_roots)")
require_text(qt-discovery-tiers-platform-fallback "${build_authority_text}" "hyb_qt_platform_roots(_qt_platform_roots)")
string(FIND "${build_authority_text}" "function(hyb_qt_tier_candidates roots out_list)" _qt_candidates_start)
if(_qt_candidates_start EQUAL -1)
    message(FATAL_ERROR "build-authority/qt-discovery-pipeline: hyb_qt_candidates is missing")
endif()
string(SUBSTRING "${build_authority_text}" ${_qt_candidates_start} -1 qt_candidates_tail)
string(FIND "${qt_candidates_tail}" "endfunction()" _qt_candidates_end)
if(_qt_candidates_end EQUAL -1)
    message(FATAL_ERROR "build-authority/qt-discovery-pipeline: hyb_qt_candidates is not terminated")
endif()
math(EXPR _qt_candidates_length "${_qt_candidates_end} + 13")
string(SUBSTRING "${qt_candidates_tail}" 0 ${_qt_candidates_length} qt_candidates_body)
forbid_text(qt-discovery-pipeline "${qt_candidates_body}" "WIN32")
forbid_text(qt-discovery-pipeline "${qt_candidates_body}" "UNIX")

# Verbose is additive on top of a report that is already complete: the configuration facts a run decided are
# stated with and without --verbose, and the one thing --verbose adds to the phase output is the command echo
# already pinned above. The needles are single-spaced because these comparisons normalise whitespace first.
run_success(verbose-normal --no-config --show-config)
run_success(verbose-additive --no-config --verbose --show-config)
foreach(shared_line IN ITEMS
        "integrations : runtime-only"
        "build type : Release"
        "examples : OFF")
    require_text_flat(verbose-normal "${HYB_TEST_OUTPUT}" "${shared_line}")
    require_text_flat(verbose-additive "${HYB_TEST_OUTPUT}" "${shared_line}")
endforeach()

message(STATUS "HyRemote build authority self-tests: PASS")
