# Licensing boundary gate.
#
# What it protects. HyRemote ships under Apache-2.0 and is used inside applications of every licence. That is only
# tenable while two things stay true: the repository contains no third-party code under terms that would contaminate
# that distribution, and the two dependencies with real obligations - Qt and OpenSSL - are consumed the way the
# dependency policy and NOTICE.md describe. Both were previously prose only, and prose does not fail a build.
#
# What it checks:
#   1. No git submodule and no vendored dependency tree. Third-party applications used as verification examples are
#      fetched at a recorded commit into an ignored build directory, never committed (see the example policy).
#   2. Qt and OpenSSL are never acquired by the build itself. OpenSSL comes from the environment or an explicit
#      OPENSSL_ROOT_DIR, and Qt comes from the user's Qt installation - fetching either would silently change the
#      licence story of every binary built here.
#   3. The licence material a release must carry still exists and still names both dependencies.

set(_vendored_dirs "vendor" "extern" "deps" "subprojects" "third_party")

if(EXISTS "${HYREMOTE_SOURCE_DIR}/.gitmodules")
    message(FATAL_ERROR
        "licensing-boundary: a .gitmodules file exists. HyRemote consumes its dependencies from the environment; a "
        "submodule silently re-licenses code into every build and must be an explicit product decision.")
endif()

foreach(_dir IN LISTS _vendored_dirs)
    if(IS_DIRECTORY "${HYREMOTE_SOURCE_DIR}/${_dir}")
        file(GLOB_RECURSE _vendored_files RELATIVE "${HYREMOTE_SOURCE_DIR}" "${HYREMOTE_SOURCE_DIR}/${_dir}/*")
        if(_vendored_files)
            list(LENGTH _vendored_files _count)
            list(GET _vendored_files 0 _first)
            message(FATAL_ERROR
                "licensing-boundary: ${_count} tracked file(s) under the vendored directory '${_dir}/' (for example "
                "'${_first}'). Third-party sources must be fetched into a build directory, never committed; see "
                "docs/dependency-policy.md.")
        endif()
    endif()
endforeach()

# `tests/third_party/` is the planned lane for real-world verification applications: scripts and CMake only. Upstream
# sources there would be exactly the vendoring this gate exists to prevent, so any file that looks like a whole
# upstream project is rejected.
if(IS_DIRECTORY "${HYREMOTE_SOURCE_DIR}/tests/third_party")
    file(GLOB_RECURSE _lane_files RELATIVE "${HYREMOTE_SOURCE_DIR}"
         "${HYREMOTE_SOURCE_DIR}/tests/third_party/*")
    foreach(_lane_file IN LISTS _lane_files)
        if(_lane_file MATCHES "(LICENSE|COPYING|COPYRIGHT)(\\.|$)" OR _lane_file MATCHES "/src/" AND _lane_file MATCHES "\\.(c|cc|cpp|h|hpp)$")
            message(FATAL_ERROR
                "licensing-boundary: '${_lane_file}' looks like content vendored from an upstream application. The "
                "third-party lane holds harnesses that fetch their target, never upstream sources or licence files.")
        endif()
    endforeach()
endif()

# Qt and OpenSSL must come from the environment: fetching them here would make every produced binary a redistribution
# this project is not prepared to license.
file(GLOB_RECURSE _build_drivers RELATIVE "${HYREMOTE_SOURCE_DIR}"
     "${HYREMOTE_SOURCE_DIR}/CMakeLists.txt"
     "${HYREMOTE_SOURCE_DIR}/cmake/*.cmake"
     "${HYREMOTE_SOURCE_DIR}/cmake/*.in"
     "${HYREMOTE_SOURCE_DIR}/src/*/CMakeLists.txt"
     "${HYREMOTE_SOURCE_DIR}/src/*/*/CMakeLists.txt"
     "${HYREMOTE_SOURCE_DIR}/tests/*/CMakeLists.txt"
     "${HYREMOTE_SOURCE_DIR}/examples/*/CMakeLists.txt")
foreach(_driver IN LISTS _build_drivers)
    # A build tree may live inside the source tree (the repository's own convention is build/ or build-*), and nested
    # builds - release evidence sub-builds, consumer fixtures - create and delete transient CMake scratch files there
    # as they run. Those are generated artifacts, not build drivers, and reading one that a concurrent or finished
    # nested configure has already removed made this gate fail nondeterministically while a concurrent run was in
    # flight. Only source-tree drivers are in scope, so paths below a build output directory are skipped.
    if(_driver MATCHES "(^|/)(build|build[-_][^/]*|_build|out|CMakeFiles|CMakeScratch)(/)")
        continue()
    endif()
    file(READ "${HYREMOTE_SOURCE_DIR}/${_driver}" _driver_text)
    if(_driver_text MATCHES "FetchContent|ExternalProject_Add|CPMAddPackage")
        foreach(_forbidden IN ITEMS "Qt6" "Qt5" "OpenSSL" "openssl")
            if(_driver_text MATCHES "${_forbidden}")
                message(FATAL_ERROR
                    "licensing-boundary: ${_driver} acquires ${_forbidden} with FetchContent/ExternalProject. Qt and "
                    "OpenSSL are consumed from the environment or an explicit prefix; acquiring them here would change "
                    "the licence story of every binary built from this repository.")
            endif()
        endforeach()
    endif()
endforeach()

foreach(_required IN ITEMS "LICENSE" "NOTICE.md")
    if(NOT EXISTS "${HYREMOTE_SOURCE_DIR}/${_required}")
        message(FATAL_ERROR "licensing-boundary: the release must carry ${_required}")
    endif()
endforeach()

file(READ "${HYREMOTE_SOURCE_DIR}/NOTICE.md" _notice)
foreach(_classified IN ITEMS "Qt" "OpenSSL")
    if(NOT _notice MATCHES "${_classified}")
        message(FATAL_ERROR
            "licensing-boundary: NOTICE.md no longer classifies ${_classified}, whose terms do not disappear by being "
            "unmentioned")
    endif()
endforeach()

message(STATUS
    "HyRemote licensing boundary gate: PASS (no submodules, no vendored dependency trees, no Qt/OpenSSL acquisition by "
    "the build, and the release still carries LICENSE + NOTICE.md classifying both dependencies)")
