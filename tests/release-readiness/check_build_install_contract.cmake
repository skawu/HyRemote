# #326/#339: the build/install contract is a user-facing promise, so it is checked rather than described.
#
# The promise is a small one and is deliberately kept small: one entry point, six commands, one install root inside the
# build tree, and a tree that a user can develop against and run from without the Qt SDK on their PATH. This gate has
# two halves, and the split is the honest part of it:
#
#   * structural rows always run. They read the entry point, the subcommand table and the deploy family, and they fail
#     closed if a second authority or a second packaging mechanism appears.
#   * measured rows run against a materialized install tree when one is present. They are what the user actually holds
#     - the manifest, the per-frontend payloads, the private headers that must not be there and the runtime identities
#     that must appear exactly once. Without a tree they report SKIP and name the command that produces one, rather
#     than passing on the strength of the source alone.
#
# Run:  cmake -DHYREMOTE_SOURCE_DIR=<repo> [-DHYREMOTE_INSTALL_ROOT=<build-dir>/install] -P check_build_install_contract.cmake

cmake_policy(SET CMP0057 NEW)  # IN_LIST membership, as written below

if(NOT DEFINED HYREMOTE_SOURCE_DIR)
    message(FATAL_ERROR "build-install-contract: HYREMOTE_SOURCE_DIR is required")
endif()

set(source_dir "${HYREMOTE_SOURCE_DIR}")
set(build_script "${source_dir}/cmake/HyRemoteBuild.cmake")
set(entry_point "${source_dir}/build.cmd")
if(NOT DEFINED HYREMOTE_INSTALL_ROOT OR HYREMOTE_INSTALL_ROOT STREQUAL "")
    set(HYREMOTE_INSTALL_ROOT "${source_dir}/build/install")
endif()
set(install_root "${HYREMOTE_INSTALL_ROOT}")

foreach(required "${entry_point}" "${build_script}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "build-install-contract: missing ${required}")
    endif()
endforeach()

function(_read path out_var)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "build-install-contract: missing ${path}")
    endif()
    file(READ "${path}" content)
    set(${out_var} "${content}" PARENT_SCOPE)
endfunction()

# A row that a reader can see in the output, so a green run states what it proved.
function(_row name outcome)
    message(STATUS "build-install-contract: ${name}=${outcome}")
endfunction()

function(_require_text haystack needle description)
    string(FIND "${haystack}" "${needle}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "build-install-contract: ${description}; missing: ${needle}")
    endif()
endfunction()

function(_forbid_text haystack needle description)
    string(FIND "${haystack}" "${needle}" at)
    if(NOT at EQUAL -1)
        message(FATAL_ERROR "build-install-contract: ${description}; forbidden: ${needle}")
    endif()
endfunction()

# A file that must exist somewhere below a directory, by glob. Used for the measured rows.
function(_glob_under root pattern out_var)
    file(GLOB_RECURSE hits "${root}/${pattern}")
    list(LENGTH hits count)
    set(${out_var} "${count}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------- 1. one entry point, and it is the only one
_read("${entry_point}" entry)
_read("${build_script}" build_system)

foreach(command IN ITEMS build install test clean rebuild help)
    _require_text("${entry}" "${command}" "the entry point must advertise every command")
endforeach()
_require_text("${entry}" "HyRemoteBuild.cmake" "the entry point must delegate its semantics")
_row("ENTRY_POINT" "PASS(build.cmd)")

# The legacy spellings are gone, not shimmed. A second entry point is a second authority even when it forwards, and the
# repository root is expected to hold exactly one build-related .cmd.
foreach(legacy IN ITEMS compile.cmd clean.cmd)
    if(EXISTS "${source_dir}/${legacy}")
        message(FATAL_ERROR
            "build-install-contract: ${legacy} still exists. build.cmd is the single entry point and the legacy "
            "spelling must not be shipped beside it.")
    endif()
endforeach()
file(GLOB build_related_entries "${source_dir}/*.cmd" "${source_dir}/*.sh" "${source_dir}/*.ps1")
list(LENGTH build_related_entries build_related_count)
if(NOT build_related_count EQUAL 1 OR NOT build_related_entries MATCHES "/build\.cmd$")
    message(FATAL_ERROR
        "build-install-contract: the repository root must hold exactly one build entry point, build.cmd; found: "
        "${build_related_entries}")
endif()
_row("LEGACY_SHIMS" "PASS(absent)")

# The entry point is a shell/batch polyglot and neither shell may misread it. cmd.exe executes a shebang first line as a
# command, which is where "'#!' is not recognized" came from, and sh needs the heredoc opener as its first line.
if(entry MATCHES "^#!")
    message(FATAL_ERROR "build-install-contract: build.cmd must not start with a shebang; cmd.exe would execute it")
endif()
_require_text("${entry}" ": <<'HYREMOTE_BATCH'" "the batch half must be introduced by the heredoc opener")

# Identity drift is the tool's problem, not the user's; a foreign build tree is not the tool's to delete.
_forbid_text("${build_system}" "Run build.cmd rebuild once after changing"
             "a configuration change must not require a manual rebuild")
_require_text("${build_system}" "HyRemote: build configuration changed; recreating"
              "a changed identity must recreate the build tree")
_require_text("${build_system}" "will not delete it automatically"
              "a build tree HyRemote did not create must fail closed")
_require_text("${build_system}" "if(HYB_SHOW_CONFIG OR HYB_VERBOSE)"
              "the full configuration dump must be opt-in, not the default output")
_row("ENTRY_POINT_POLYGLOT" "PASS")

# ---------------------------------------------------------------- 2. the command semantics
_require_text("${build_system}" "set(HYB_SUBCOMMAND \"build\")"
              "an invocation with no command must mean build")
_require_text("${build_system}" "Use one of: build, install, test, clean, rebuild, help."
              "an unknown command must fail closed and name the legal ones")
_require_text("${build_system}" "Usage: build.cmd [command] [options]" "the usage text belongs to build.cmd")
_require_text("${build_system}" "list(APPEND HYB_ARGS \"--help\")"
              "help must be folded into the one option parser that owns the usage text")

# install = build if required, then install; test = build and run the tests; clean/rebuild = clean.
_require_text("${build_system}" "set(HYB_INSTALL ON)" "install must enable the install step")
_require_text("${build_system}" "set(HYB_TESTS_BUILD ON)\n    set(HYB_TESTS_RUN ON)"
              "test must own both test switches")
_require_text("${build_system}" "set(HYB_CLEAN ON)" "clean must enable the clean step")
_row("BUILD_COMMAND" "PASS")
_row("INSTALL_COMMAND" "PASS")
_row("TEST_COMMAND" "PASS")
_row("CLEAN_COMMAND" "PASS")
_row("REBUILD_COMMAND" "PASS")

# ---------------------------------------------------------------- 3. the install root, and refusing to inherit it
_require_text("${build_system}" "set(HYB_INSTALL_ROOT \"\${HYB_BUILD_DIR}/install\")"
              "the install root must default inside the build tree")
_require_text("${build_system}" "message(STATUS \"HYREMOTE_INSTALL_ROOT=\${HYB_INSTALL_ROOT}\")"
              "install must print the root it produced")

# Stale safety: the tree has to be materialized from this configuration alone. A leftover frontend, example or security
# payload from a previous configuration would otherwise be indistinguishable from a freshly built one.
string(FIND "${build_system}" "file(REMOVE_RECURSE \"\${HYB_INSTALL_ROOT}\")" remove_at)
string(FIND "${build_system}" "--install \"\${HYB_BUILD_DIR}\" --prefix" install_at)
if(remove_at EQUAL -1 OR install_at EQUAL -1)
    message(FATAL_ERROR "build-install-contract: the install step must use the existing install family")
endif()
if(NOT remove_at LESS install_at)
    message(FATAL_ERROR "build-install-contract: the install root must be cleared before it is materialized")
endif()
_require_text("${build_system}" "HYREMOTE-MANIFEST.txt" "the tree must carry its own manifest")

# #350: the shipped facts are separate fields. SECURITY_STATE used to carry the build capability while reading like the
# shipped state, which was wrong in both directions - the default profile is unauthenticated and unencrypted whether or
# not the capability was compiled in - so it is gone and must not come back.
foreach(key IN ITEMS
        SOURCE_SHA OS ARCH QT_VERSION BUILD_TYPE CPP QML GENERIC QPA
        LISTENER_DEFAULT AUTHENTICATION_ENABLED AUTHENTICATION_PROFILE
        TRANSPORT_ENCRYPTION_ENABLED TRANSPORT_ENCRYPTION_PROFILE REMOTE_INPUT_DEFAULT
        TRANSPORT_SECURITY_CAPABILITY)
    _require_text("${build_system}" "${key}=" "the manifest must state ${key}")
endforeach()
_forbid_text("${build_system}" "SECURITY_STATE="
             "the manifest must not conflate the build capability with the shipped state")

# The defaults the manifest states have to be the runtime's own defaults, or the manifest is a claim rather than a fact.
_read("${source_dir}/src/runtime/src/access_instance.cpp" runtime_defaults)
_require_text("${runtime_defaults}" "SecurityProfile securityProfile = SecurityProfile::Insecure"
              "the manifest states authentication off, so the runtime default must still be Insecure")
_require_text("${runtime_defaults}" "bool remoteInputEnabled = false"
              "the manifest states remote input off, so the runtime default must still be off")
_row("MANIFEST_FACTS_SOURCE" "PASS(runtime defaults)")
_row("INSTALL_ROOT" "PASS(${install_root})")
_row("STALE_SAFETY" "PASS(cleared before materializing)")

# ---------------------------------------------------------------- 4. one deploy family, no second packaging route
file(GLOB build_modules "${source_dir}/cmake/*.cmake")
set(definer_count 0)
foreach(module IN LISTS build_modules)
    file(READ "${module}" module_text)
    string(REGEX MATCHALL "function\\(hyremote_deploy\\)" found "${module_text}")
    list(LENGTH found here)
    math(EXPR definer_count "${definer_count} + ${here}")
endforeach()
if(NOT definer_count EQUAL 1)
    message(FATAL_ERROR "build-install-contract: expected exactly one hyremote_deploy definition, found ${definer_count}")
endif()

# Every example that installs a runnable must deploy through that family. A hand-rolled per-target Qt copy is what
# makes N examples cost N Qt trees, which is the duplication this gate exists to prevent.
file(GLOB_RECURSE example_cmakelists "${source_dir}/examples/*/CMakeLists.txt")
set(example_consumers 0)
foreach(list_file IN LISTS example_cmakelists)
    file(READ "${list_file}" list_text)
    if(NOT list_text MATCHES "install\\(TARGETS")
        continue()
    endif()
    math(EXPR example_consumers "${example_consumers} + 1")
    _require_text("${list_text}" "hyremote_deploy("
                  "${list_file} installs a runnable but does not use the shared deploy family")
    _forbid_text("${list_text}" "windeployqt" "${list_file} must not deploy a Qt closure itself")
    _forbid_text("${list_text}" "macdeployqt" "${list_file} must not deploy a Qt closure itself")
endforeach()
if(example_consumers EQUAL 0)
    message(FATAL_ERROR "build-install-contract: no example installs a runnable to check")
endif()

# A frontend installs its own payload - that is allowed, because the payload is the product. What is not allowed is a
# frontend building a second Qt closure beside the shared one.
file(GLOB_RECURSE frontend_cmakelists "${source_dir}/src/integrations/*/CMakeLists.txt")
set(frontend_consumers 0)
foreach(list_file IN LISTS frontend_cmakelists)
    if(list_file MATCHES "/tests/")
        continue()
    endif()
    file(READ "${list_file}" list_text)
    if(NOT list_text MATCHES "install\\(TARGETS")
        continue()
    endif()
    math(EXPR frontend_consumers "${frontend_consumers} + 1")
    _forbid_text("${list_text}" "windeployqt" "${list_file} must not deploy a second Qt closure")
    _forbid_text("${list_text}" "macdeployqt" "${list_file} must not deploy a second Qt closure")
endforeach()
_row("SHARED_DEPLOY_FAMILY" "PASS(${example_consumers} examples, ${frontend_consumers} frontend payloads)")

# ---------------------------------------------------------------- 5. the runtime is never fixed up by its environment
#
file(GLOB_RECURSE example_sources "${source_dir}/examples/*")
set(text_extensions .cpp .h .hpp .cmake .txt .md .in .sh .cmd .py)
set(launcher_files)
foreach(candidate IN LISTS example_sources)
    # Comparing the extension keeps a regex anchor's trailing dollar sign away from a closing quote, where CMake reads
    # it as a malformed variable reference.
    if(IS_DIRECTORY "${candidate}")
        continue()
    endif()
    get_filename_component(candidate_extension "${candidate}" LAST_EXT)
    if(candidate_extension IN_LIST text_extensions)
        list(APPEND launcher_files "${candidate}")
    endif()
endforeach()


# A launcher may pass product arguments and nothing else. If reaching a deployed runtime required PATH,
# QT_PLUGIN_PATH or LD_LIBRARY_PATH assistance, then a "clean machine" check would be testing the launcher instead of
# the tree. Only code that runs is inspected: an example's own documentation is deliberately out of scope, because
# stating that a variable is *not* required is exactly the claim this row defends.
set(deploy_code_extensions .cpp .h .hpp .cmake .txt .in .sh .cmd .py)
set(assistance_patterns
    "QT_PLUGIN_PATH=" "QT_QPA_PLATFORM_PLUGIN_PATH="
    "set(QT_PLUGIN_PATH" "set(QT_QPA_PLATFORM_PLUGIN_PATH"
    "export QT_PLUGIN_PATH"
    "LD_LIBRARY_PATH=" "set(LD_LIBRARY_PATH" "export LD_LIBRARY_PATH")
foreach(candidate IN LISTS launcher_files)
    get_filename_component(candidate_extension "${candidate}" LAST_EXT)
    if(NOT candidate_extension IN_LIST deploy_code_extensions)
        continue()
    endif()
    file(READ "${candidate}" candidate_text)
    foreach(pattern IN LISTS assistance_patterns)
        _forbid_text("${candidate_text}" "${pattern}"
                     "${candidate} must not locate a deployed runtime through the environment")
    endforeach()
endforeach()
_row("SDK_PATH_ASSISTANCE" "FALSE")

# ---------------------------------------------------------------- 6. measured rows: the tree a user actually holds
if(NOT EXISTS "${install_root}/HYREMOTE-MANIFEST.txt")
    message(STATUS "build-install-contract: MEASURED_ROWS=SKIP (no tree at ${install_root}; run: build.cmd install)")
    message(STATUS "build-install-contract: PASS")
    return()
endif()

_read("${install_root}/HYREMOTE-MANIFEST.txt" manifest)

# The manifest is written with the platform's line endings, so it is normalised and read one line at a time rather than
# matched with one multi-line pattern: a manifest is a list of facts, and reading it as a list is both simpler and free
# of the escaping that a pattern would need.
string(REPLACE "\r\n" "\n" manifest "${manifest}")
string(REPLACE "\r" "\n" manifest "${manifest}")
string(REPLACE "\n" ";" manifest_lines "${manifest}")
set(manifest_keys SOURCE_SHA OS ARCH QT_VERSION BUILD_TYPE CPP QML GENERIC QPA
        LISTENER_DEFAULT AUTHENTICATION_ENABLED AUTHENTICATION_PROFILE
        TRANSPORT_ENCRYPTION_ENABLED TRANSPORT_ENCRYPTION_PROFILE REMOTE_INPUT_DEFAULT
        TRANSPORT_SECURITY_CAPABILITY)
foreach(line IN LISTS manifest_lines)
    if(line MATCHES "^([A-Z_]+)=(.*)")
        set(manifest_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
    endif()
endforeach()
foreach(key IN LISTS manifest_keys)
    if(NOT DEFINED manifest_${key} OR "${manifest_${key}}" STREQUAL "")
        message(FATAL_ERROR "build-install-contract: the manifest does not state a usable ${key}")
    endif()
endforeach()
_row("MANIFEST" "PASS(${manifest_OS}/${manifest_ARCH}, qt ${manifest_QT_VERSION})")

# #143/#350: the manifest has to agree with the frozen shipped facts, not merely carry the fields.
foreach(expected IN ITEMS
        "AUTHENTICATION_ENABLED=OFF"
        "AUTHENTICATION_PROFILE=none"
        "TRANSPORT_ENCRYPTION_ENABLED=OFF"
        "TRANSPORT_ENCRYPTION_PROFILE=none"
        "REMOTE_INPUT_DEFAULT=OFF"
        "LISTENER_DEFAULT=0.0.0.0")
    _require_text("${manifest}" "${expected}" "the manifest must state ${expected}")
endforeach()
if(NOT manifest_LISTENER_DEFAULT MATCHES "^0\\.0\\.0\\.0:[0-9]+")
    message(FATAL_ERROR "build-install-contract: the manifest records LISTENER_DEFAULT=${manifest_LISTENER_DEFAULT}")
endif()
if(NOT manifest_TRANSPORT_SECURITY_CAPABILITY STREQUAL "ON"
        AND NOT manifest_TRANSPORT_SECURITY_CAPABILITY STREQUAL "OFF")
    message(FATAL_ERROR "build-install-contract: TRANSPORT_SECURITY_CAPABILITY must be ON or OFF")
endif()
_row("MANIFEST_SHIPPED_FACTS" "PASS(authentication off, no encryption, input off, listener ${manifest_LISTENER_DEFAULT})")

# The manifest carries a commit rather than a version label, so a holder of the directory knows exactly which source it
# came from. Whether the tree is *current* is a statement about the working copy, not about the product, so a stale tree
# is reported and a malformed commit is refused: a label that cannot be traced is the thing worth failing on.
if(NOT manifest_SOURCE_SHA MATCHES "^[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]")
    message(FATAL_ERROR
        "build-install-contract: the manifest records SOURCE_SHA=${manifest_SOURCE_SHA}, which is not a commit")
endif()
execute_process(COMMAND git -C "${source_dir}" rev-parse HEAD
                RESULT_VARIABLE git_rc OUTPUT_VARIABLE git_head
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
if(git_rc EQUAL 0 AND NOT git_head STREQUAL "")
    if(manifest_SOURCE_SHA STREQUAL git_head)
        _row("MANIFEST_SOURCE_SHA" "PASS(matches HEAD)")
    else()
        _row("MANIFEST_SOURCE_SHA" "STALE(tree ${manifest_SOURCE_SHA} vs HEAD ${git_head})")
    endif()
endif()

# The Embedded C++ SDK is the one frontend whose consumption contract is headers plus a CMake package. Both have to be
# in the tree, and neither may be smuggled in from the build directory at consume time.
if(manifest_CPP STREQUAL "ON")
    _glob_under("${install_root}/include" "HyRemote/*.h*" sdk_headers)
    if(sdk_headers EQUAL 0)
        message(FATAL_ERROR "build-install-contract: CPP=ON but the tree installs no public HyRemote headers")
    endif()
    file(GLOB_RECURSE package_files "${install_root}/lib/cmake/*/*.cmake" "${install_root}/lib/cmake/*/*.in")
    set(package_ok FALSE)
    foreach(package_file IN LISTS package_files)
        file(READ "${package_file}" package_text)
        if(package_text MATCHES "HyRemote::RemoteAccess")
            set(package_ok TRUE)
        endif()
    endforeach()
    if(NOT package_ok)
        message(FATAL_ERROR "build-install-contract: CPP=ON but the tree has no HyRemote::RemoteAccess CMake package")
    endif()
    _row("CPP_SDK_HEADERS" "PASS(${sdk_headers})")
    _row("CPP_CMAKE_PACKAGE" "PASS")

    # Packaging convenience is never a reason to publish an implementation header. These are the private headers whose
    # presence in a consume path would turn the SDK into an implementation dump.
    foreach(private IN ITEMS
            "access_instance.hpp" "access_types.hpp" "listener_binding.hpp" "automatic_access_config.hpp"
            "component_factories.hpp" "QmlRemoteAccess.h" "qpa_config.hpp" "generic_config.hpp")
        _glob_under("${install_root}" "*/${private}" leaked)
        if(NOT leaked EQUAL 0)
            message(FATAL_ERROR "build-install-contract: the tree installs the private header ${private}")
        endif()
    endforeach()
    _row("PRIVATE_HEADERS_CONTAINED" "PASS")
endif()

# §16: adding examples must not add runtime copies. The question "is the closure shared?" is really "is there one
# place the runtime lives?", so the payloads are counted by the directory they are in. Counting files instead would
# confuse an import library or a symbol file with a runtime - a consumer links against those, nothing loads them - and
# would double-count the versioned name plus its symlink on Linux. A second directory is exactly what a per-example
# deploy produces, so that is the thing worth failing on.
function(_runtime_dirs stem out_var out_files)
    set(dirs)
    set(files 0)
    file(GLOB_RECURSE versioned "${install_root}/*${stem}*.so.*")
    foreach(suffix IN ITEMS .dll .so .dylib)
        file(GLOB_RECURSE hits "${install_root}/*${stem}*${suffix}")
        list(APPEND versioned ${hits})
    endforeach()
    foreach(hit IN LISTS versioned)
        get_filename_component(dir "${hit}" DIRECTORY)
        list(APPEND dirs "${dir}")
        math(EXPR files "${files} + 1")
    endforeach()
    if(dirs)
        list(REMOVE_DUPLICATES dirs)
    endif()
    list(LENGTH dirs dir_count)
    set(${out_var} "${dir_count}" PARENT_SCOPE)
    set(${out_files} "${files}" PARENT_SCOPE)
endfunction()

_runtime_dirs("HyRemoteRemoteAccess" runtime_dirs runtime_files)
if(NOT runtime_dirs EQUAL 1)
    message(FATAL_ERROR
        "build-install-contract: the runtime lives in ${runtime_dirs} directories, so the closure is not shared")
endif()

_runtime_dirs("Qt6Core" qt_dirs qt_files)
if(qt_dirs GREATER 1)
    message(FATAL_ERROR "build-install-contract: the Qt runtime lives in ${qt_dirs} directories")
endif()

file(GLOB native_plugins "${install_root}/plugins/platforms/qwindows.dll"
                        "${install_root}/plugins/platforms/libqxcb.so"
                        "${install_root}/plugins/platforms/libqeglfs.so")
list(LENGTH native_plugins native_plugin_count)
if(native_plugin_count GREATER 1)
    message(FATAL_ERROR "build-install-contract: the tree carries ${native_plugin_count} native platform plugins")
endif()

_row("RUNTIME_IDENTITIES" "PASS(${runtime_files} runtime payload(s) in ${runtime_dirs} dir, ${qt_files} qt in ${qt_dirs}, ${native_plugin_count} platform)")
# Each selected frontend has to be present as a real payload, not merely switched on in the manifest. The manifest
# states what was configured; these rows state what arrived, which is the difference a user notices.
if(manifest_QML STREQUAL "ON")
    if(NOT EXISTS "${install_root}/qml/HyRemote/qmldir")
        message(FATAL_ERROR "build-install-contract: QML=ON but the tree has no HyRemote QML module (qml/HyRemote/qmldir)")
    endif()
    _glob_under("${install_root}/qml/HyRemote" "*hyremote*" qml_plugin)
    if(qml_plugin EQUAL 0)
        message(FATAL_ERROR "build-install-contract: QML=ON but the module has no backing runtime")
    endif()
    _row("QML_PAYLOAD" "PASS(qmldir + backing runtime)")
endif()

if(manifest_GENERIC STREQUAL "ON")
    _glob_under("${install_root}/plugins/generic" "*hyremote*" generic_payload)
    if(generic_payload EQUAL 0)
        message(FATAL_ERROR "build-install-contract: GENERIC=ON but the tree has no Generic plugin payload")
    endif()
    _row("GENERIC_PAYLOAD" "PASS(${generic_payload} file(s) under plugins/generic)")
endif()

if(manifest_QPA STREQUAL "ON")
    _glob_under("${install_root}/plugins/platforms" "*hyremote*" qpa_payload)
    if(qpa_payload EQUAL 0)
        message(FATAL_ERROR "build-install-contract: QPA=ON but the tree has no QPA platform payload")
    endif()
    _row("QPA_PAYLOAD" "PASS(${qpa_payload} file(s) under plugins/platforms)")
endif()

# ---------------------------------------------------------------- 7. real invocations of the entry point
#
# Kept cheap on purpose: this gate must not turn a normal test run into a second build, so it drives the paths that
# return or fail before configure and proves the mechanisms. The end-to-end proof is the recorded transcript and CI,
# which configures and builds through this same entry point.
if(WIN32)
    set(entry_launcher cmd /c)
else()
    set(entry_launcher sh)
endif()
set(entry_path "${source_dir}/build.cmd")

function(_run_entry out_var)
    execute_process(COMMAND ${entry_launcher} "${entry_path}" ${ARGN}
                    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(${out_var} "${rc}|${out}|${err}" PARENT_SCOPE)
endfunction()

set(scratch_root "${CMAKE_CURRENT_BINARY_DIR}/hyremote-entry-point-scratch")
file(REMOVE_RECURSE "${scratch_root}")
file(MAKE_DIRECTORY "${scratch_root}")

# A. the entry point runs and prints no bootstrap error. This is the check the Windows users saw fail: cmd.exe executed
#    the shebang line and printed "'#!' is not recognized" before anything else happened.
_run_entry(help_run --show-config --build-dir=${scratch_root}/a)
string(FIND "${help_run}" "0|" help_rc_at)
if(NOT help_rc_at EQUAL 0)
    message(FATAL_ERROR "build-install-contract: the entry point did not run cleanly: ${help_run}")
endif()
_forbid_text("${help_run}" "#!' is not recognized" "the entry point must not print a bootstrap error")
_forbid_text("${help_run}" "is not recognized as an internal or external command"
             "the entry point must not print a bootstrap error")
_row("ENTRY_POINT_RUNS" "PASS")

# Unknown command: fails closed and names the legal ones.
_run_entry(unknown_run frobnicate --build-dir=${scratch_root}/b)
string(FIND "${unknown_run}" "0|" unknown_rc_at)
if(unknown_rc_at EQUAL 0)
    message(FATAL_ERROR "build-install-contract: an unknown command must fail closed: ${unknown_run}")
endif()

# D. a build directory HyRemote did not create is refused, and it survives.
set(foreign_dir "${scratch_root}/foreign")
file(MAKE_DIRECTORY "${foreign_dir}")
file(WRITE "${foreign_dir}/CMakeCache.txt" "// a build tree HyRemote did not create
")
file(WRITE "${foreign_dir}/someone-elses-file.txt" "keep me
")
_run_entry(foreign_run --build-dir=${foreign_dir})
string(FIND "${foreign_run}" "0|" foreign_rc_at)
if(foreign_rc_at EQUAL 0)
    message(FATAL_ERROR "build-install-contract: a foreign build directory must fail closed: ${foreign_run}")
endif()
if(NOT EXISTS "${foreign_dir}/someone-elses-file.txt")
    message(FATAL_ERROR "build-install-contract: a foreign build directory was modified")
endif()
_row("FOREIGN_BUILD_DIR" "PASS(refused, untouched)")

# C. a configuration change in a tree HyRemote owns is handled by the tool: the tree is recreated, the stale sentinel
#    goes with it, and the marker is rewritten. The configure that follows is deliberately impossible to complete, so
#    the run is expected to fail after the recovery; what is asserted is the recovery itself.
set(owned_dir "${scratch_root}/owned")
file(MAKE_DIRECTORY "${owned_dir}")
file(WRITE "${owned_dir}/.hyremote-build-identity" "generator=old\nqt=old\n")
file(WRITE "${owned_dir}/stale-sentinel.txt" "from the previous configuration\n")
_run_entry(owned_run --build-dir=${owned_dir} --generator=HyRemoteNonexistentGenerator)
_forbid_text("${owned_run}" "Run build.cmd rebuild once"
             "a configuration change must not ask the user to rebuild by hand")
_require_text("${owned_run}" "build configuration changed; recreating"
              "a changed identity must announce that it recreates the tree")
if(EXISTS "${owned_dir}/stale-sentinel.txt")
    message(FATAL_ERROR "build-install-contract: the recreated build tree still holds the previous configuration")
endif()
file(READ "${owned_dir}/.hyremote-build-identity" rewritten_identity)
_forbid_text("${rewritten_identity}" "generator=old" "the identity marker must be rewritten for the new configuration")
_row("IDENTITY_SELF_HEAL" "PASS(recreated, marker rewritten)")

file(REMOVE_RECURSE "${scratch_root}")
_row("ENTRY_POINT_MECHANISMS" "PASS")

message(STATUS "build-install-contract: PASS")
