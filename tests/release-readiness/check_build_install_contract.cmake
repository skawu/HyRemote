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

# The legacy spellings are shims, and a shim that still carried build logic would be a second authority wearing an old
# name. They must forward, and they must not own any part of the contract.
foreach(shim IN ITEMS compile.cmd clean.cmd)
    set(shim_path "${source_dir}/${shim}")
    _read("${shim_path}" shim_text)
    _require_text("${shim_text}" "build.cmd" "${shim} must forward to the single authority")
    _forbid_text("${shim_text}" "HyRemoteBuild.cmake" "${shim} must not own the build system")
    _forbid_text("${shim_text}" "--integrations" "${shim} must not carry an option surface")
    _forbid_text("${shim_text}" "cmake -S" "${shim} must not configure anything itself")
endforeach()
_row("LEGACY_SHIMS" "PASS(forward-only)")

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
foreach(key IN ITEMS SOURCE_SHA OS ARCH QT_VERSION BUILD_TYPE CPP QML GENERIC QPA SECURITY_STATE)
    _require_text("${build_system}" "${key}=" "the manifest must state ${key}")
endforeach()
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
set(manifest_keys SOURCE_SHA OS ARCH QT_VERSION BUILD_TYPE CPP QML GENERIC QPA SECURITY_STATE)
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

message(STATUS "build-install-contract: PASS")
