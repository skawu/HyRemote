# Fast, build-free self test for the clean-consumer acquisition audit.
#
# The audit decides whether a consumer acquired HyRemote from a package instead of from the product source or build
# tree. That decision is a pure function of a CMakeCache file, so it can be proved here with synthetic caches - no
# product configure, no build, no Qt. Every fixture is written under a scratch directory that must not be the
# repository root, and the whole test finishes in well under a second.
#
# Run: cmake -DACQUISITION_AUDIT_SCRATCH_DIR=<dir under build/> -P acquisition_audit_self_test.cmake

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/acquisition_audit.cmake")

# The self test writes synthetic caches. They belong in a build directory: fixtures written into the repository tree
# are generated inputs one `git add -A` away from being committed. The build directory is therefore an explicit input
# rather than a guess - the repository's own build directory is not always called "build" (CI uses build-ci), and a
# guessed name would either reject a legitimate directory or quietly accept the tree itself.
if(NOT DEFINED ACQUISITION_AUDIT_BUILD_DIR OR ACQUISITION_AUDIT_BUILD_DIR STREQUAL "")
    message(FATAL_ERROR
        "acquisition-audit-selftest: ACQUISITION_AUDIT_BUILD_DIR is required, so that fixtures are written under the "
        "build directory and can never land in the repository tree.")
endif()
if(NOT DEFINED ACQUISITION_AUDIT_SCRATCH_DIR OR ACQUISITION_AUDIT_SCRATCH_DIR STREQUAL "")
    set(ACQUISITION_AUDIT_SCRATCH_DIR "${ACQUISITION_AUDIT_BUILD_DIR}/acquisition-audit-selftest")
endif()

normalize_path("${ACQUISITION_AUDIT_SCRATCH_DIR}" _scratch)
normalize_path("${ACQUISITION_AUDIT_BUILD_DIR}" _build_dir)
normalize_path("${CMAKE_CURRENT_LIST_DIR}/../.." _repository_root)

set(_scratch_allowed FALSE)
path_is_under("${_scratch}" "${_build_dir}" _scratch_under_build)
if(_scratch_under_build)
    set(_scratch_allowed TRUE)
else()
    # The system temporary directory is the only alternative, so a developer can run this by hand anywhere.
    foreach(_temp_variable IN ITEMS TEMP TMPDIR TMP)
        if(DEFINED ENV{${_temp_variable}})
            path_is_under("${_scratch}" "$ENV{${_temp_variable}}" _scratch_under_temp)
            if(_scratch_under_temp)
                set(_scratch_allowed TRUE)
                break()
            endif()
        endif()
    endforeach()
endif()

if(NOT _scratch_allowed)
    message(FATAL_ERROR
        "acquisition-audit-selftest: refusing to write fixtures to '${ACQUISITION_AUDIT_SCRATCH_DIR}'. Use a directory "
        "under the build directory (${_build_dir}) or the system temporary directory; the repository tree "
        "(${_repository_root}) is never a scratch directory.")
endif()

file(REMOVE_RECURSE "${ACQUISITION_AUDIT_SCRATCH_DIR}")
file(MAKE_DIRECTORY "${ACQUISITION_AUDIT_SCRATCH_DIR}")

# Synthetic roots. None of them has to exist: the audit judges spelling, which is what lets it catch a forbidden
# acquisition whose directory was already deleted.
set(CASE_RUN_DIR "CASE/run-tree")
set(CASE_BUILD_ROOT "CASE")
set(CASE_SOURCE_ROOT "CASE/product-source")
set(CASE_QT_PREFIX "EXTERNAL/Qt/6.8.3/gcc_64")
set(CASE_QT6_DIR "EXTERNAL/Qt/6.8.3/gcc_64/lib/cmake/Qt6")
set(CASE_HYREMOTE_DIR "CASE/run-tree/prefix/lib/cmake/HyRemote")

set(failures 0)

function(expect_counts label cache_file expected_source expected_build)
    audit_acquisition_entries("${cache_file}" "${CASE_RUN_DIR}" "${CASE_SOURCE_ROOT}" "${CASE_BUILD_ROOT}"
        found_source found_build)
    if(NOT found_source EQUAL expected_source OR NOT found_build EQUAL expected_build)
        message(STATUS "CASE FAILED: ${label}: expected source=${expected_source} build=${expected_build}, "
            "got source=${found_source} build=${found_build}")
        math(EXPR _failed "${failures} + 1")
        set(failures "${_failed}" PARENT_SCOPE)
        return()
    endif()
    message(STATUS "case ok: ${label} (source=${found_source} build=${found_build})")
endfunction()

function(write_cache name content)
    set(path "${ACQUISITION_AUDIT_SCRATCH_DIR}/${name}.txt")
    file(WRITE "${path}" "${content}")
    set(CASE_CACHE "${path}" PARENT_SCOPE)
endfunction()

# 1. Mixed source entry: the run's own prefix and a product source path inside one CMAKE_PREFIX_PATH value. The line
#    contains the allowed run path, which is exactly what the previous per-line audit skipped over.
write_cache(mixed-source
    "CMAKE_PREFIX_PATH:PATH=${CASE_RUN_DIR}/prefix;${CASE_SOURCE_ROOT}/forbidden\nHyRemote_DIR:PATH=${CASE_HYREMOTE_DIR}\n")
expect_counts("mixed source entry is a source violation" "${CASE_CACHE}" 1 0)

# 2. Mixed build entry: the same shape with a build-tree path that is not part of the run's own tree.
write_cache(mixed-build
    "CMAKE_PREFIX_PATH:PATH=${CASE_RUN_DIR}/prefix;${CASE_BUILD_ROOT}/forbidden-outside-run\nHyRemote_DIR:PATH=${CASE_HYREMOTE_DIR}\n")
expect_counts("mixed build entry is a build violation" "${CASE_CACHE}" 0 1)

# 3. Allowed: only the run's own child paths, plus an external Qt. Note that CASE_RUN_DIR is a child of CASE_BUILD_ROOT,
#    so a run entry that was judged against the build root before the run root would be misreported as a build
#    dependency here.
write_cache(allowed-run-and-external-qt
    "CMAKE_PREFIX_PATH:PATH=${CASE_RUN_DIR}/prefix;${CASE_QT_PREFIX}\nHyRemote_DIR:PATH=${CASE_HYREMOTE_DIR}\nQt6_DIR:PATH=${CASE_QT6_DIR}\n")
expect_counts("run-owned paths and an external Qt are allowed" "${CASE_CACHE}" 0 0)

# 4. Allowed: a single run child on its own, which is the everyday clean-consumer shape.
write_cache(allowed-single-run-child
    "HyRemote_DIR:PATH=${CASE_HYREMOTE_DIR}\nQt6_DIR:PATH=${CASE_QT6_DIR}\n")
expect_counts("a single run child is not a build dependency" "${CASE_CACHE}" 0 0)

# 5. Indexed fields and library/plugin paths are acquisition fields too.
write_cache(acquisition-fields
    "Qt6_DIR:PATH=${CASE_QT6_DIR}\nhyr_lib_LIBRARY:FILEPATH=${CASE_SOURCE_ROOT}/build/libhyremote.so\nhyremote_plugin_PLUGINS:PATH=${CASE_SOURCE_ROOT}/plugins\n")
expect_counts("library and plugin fields into the source tree are counted" "${CASE_CACHE}" 2 0)

# 6. A sibling directory whose name merely starts with the root name is not inside it.
write_cache(sibling-name
    "CMAKE_PREFIX_PATH:PATH=${CASE_BUILD_ROOT}-extra/prefix\nHyRemote_DIR:PATH=${CASE_HYREMOTE_DIR}\n")
expect_counts("a root-named sibling is not inside the root" "${CASE_CACHE}" 0 0)

# 7. Syntactic . and .. normalisation: a path that escapes the run tree into the source tree is still a violation, and
#    a path that resolves back inside the run tree is still allowed.
write_cache(dotdot-escapes-run
    "CMAKE_PREFIX_PATH:PATH=${CASE_RUN_DIR}/prefix/../../product-source/forbidden\n")
expect_counts("a .. path that lands in the source tree is a violation" "${CASE_CACHE}" 1 0)

write_cache(dotdot-stays-inside-run
    "CMAKE_PREFIX_PATH:PATH=${CASE_RUN_DIR}/prefix/../prefix\n")
expect_counts("a .. path that resolves back into the run tree is allowed" "${CASE_CACHE}" 0 0)

# 8. Windows representation on any host: backslashes and a case difference must not hide a forbidden acquisition, and
#    a case difference on an allowed path must not invent one. Case folding is passed explicitly so this is proved on
#    Linux too.
function(expect_counts_folded label cache_file expected_source expected_build)
    audit_acquisition_entries("${cache_file}" "C:/Work/Run" "C:/Repo/Product" "C:/Work"
        found_source found_build TRUE)
    if(NOT found_source EQUAL expected_source OR NOT found_build EQUAL expected_build)
        message(STATUS "CASE FAILED: ${label}: expected source=${expected_source} build=${expected_build}, "
            "got source=${found_source} build=${found_build}")
        math(EXPR _failed "${failures} + 1")
        set(failures "${_failed}" PARENT_SCOPE)
        return()
    endif()
    message(STATUS "case ok: ${label} (source=${found_source} build=${found_build})")
endfunction()

write_cache(windows-mixed-source
    "CMAKE_PREFIX_PATH:PATH=C:/Work/Run/prefix;C:\\Repo\\Product\\forbidden\n")
expect_counts_folded("windows backslash and case difference still detect a source violation" "${CASE_CACHE}" 1 0)

write_cache(windows-allowed-case-difference
    "CMAKE_PREFIX_PATH:PATH=c:/WORK/run/prefix\n")
expect_counts_folded("windows case folding does not invent a violation" "${CASE_CACHE}" 0 0)

# 9. An empty or unreadable cache contributes nothing rather than failing to compile the answer.
write_cache(empty-cache "# empty synthetic cache\n")
expect_counts("an empty cache yields no violations" "${CASE_CACHE}" 0 0)
audit_acquisition_entries("${ACQUISITION_AUDIT_SCRATCH_DIR}/does-not-exist.txt" "C:/Work/Run" "C:/Repo/Product"
    "C:/Work" missing_source missing_build TRUE)
if(NOT missing_source EQUAL 0 OR NOT missing_build EQUAL 0)
    message(STATUS "CASE FAILED: a missing cache yields no violations")
    math(EXPR failures "${failures} + 1")
else()
    message(STATUS "case ok: a missing cache yields no violations")
endif()

file(REMOVE_RECURSE "${ACQUISITION_AUDIT_SCRATCH_DIR}")

if(NOT failures EQUAL 0)
    message(FATAL_ERROR "acquisition-audit-selftest: ${failures} contradiction(s)")
endif()
message(STATUS
    "HyRemote acquisition-audit self test: PASS (per-entry judgement; mixed source and mixed build entries are "
    "detected; run-owned paths, external Qt and case-folded host names are not misreported)")
